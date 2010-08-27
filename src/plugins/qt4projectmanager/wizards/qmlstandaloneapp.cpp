/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2010 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** Commercial Usage
**
** Licensees holding valid Qt Commercial licenses may use this file in
** accordance with the Qt Commercial License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Nokia.
**
** GNU Lesser General Public License Usage
**
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** If you are unsure which license is appropriate for your use, please
** contact the sales department at http://qt.nokia.com/contact.
**
**************************************************************************/

#include "qmlstandaloneapp.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QRegExp>
#include <QtCore/QTextStream>

#ifndef CREATORLESSTEST
#include <coreplugin/icore.h>
#endif // CREATORLESSTEST

namespace Qt4ProjectManager {
namespace Internal {

const QString qmldir(QLatin1String("qmldir"));
const QString qmldir_plugin(QLatin1String("plugin"));
const QString appViewerBaseName(QLatin1String("qmlapplicationviewer"));
const QString appViewerPriFileName(appViewerBaseName + QLatin1String(".pri"));
const QString appViewerCppFileName(appViewerBaseName + QLatin1String(".cpp"));
const QString appViewerHFileName(appViewerBaseName + QLatin1String(".h"));
const QString appViewerOriginsSubDir(appViewerBaseName + QLatin1Char('/'));
const QString fileChecksum(QLatin1String("checksum"));
const QString fileStubVersion(QLatin1String("version"));

QmlModule::QmlModule(const QString &uri, const QFileInfo &rootDir, const QFileInfo &qmldir,
                     bool isExternal, QmlStandaloneApp *qmlStandaloneApp)
    : uri(uri)
    , rootDir(rootDir)
    , qmldir(qmldir)
    , isExternal(isExternal)
    , qmlStandaloneApp(qmlStandaloneApp)
{}

QString QmlModule::path(Path path) const
{
    switch (path) {
        case Root: {
            return rootDir.canonicalFilePath();
        }
        case ContentDir: {
            const QDir proFile(qmlStandaloneApp->path(QmlStandaloneApp::AppProPath));
            return proFile.relativeFilePath(qmldir.canonicalPath());
        }
        case ContentBase: {
            const QString localRoot = rootDir.canonicalFilePath() + QLatin1Char('/');
            QDir contentDir = qmldir.dir();
            contentDir.cdUp();
            const QString localContentDir = contentDir.canonicalPath();
            return localContentDir.right(localContentDir.length() - localRoot.length());
        }
        case DeployedContentBase: {
            const QString modulesDir = qmlStandaloneApp->path(QmlStandaloneApp::ModulesDir);
            return modulesDir + QLatin1Char('/') + this->path(ContentBase);
        }
        default: qFatal("QmlModule::path() needs more work");
    }
    return QString();
}

QmlCppPlugin::QmlCppPlugin(const QString &name, const QFileInfo &path,
                           const QmlModule *module, const QFileInfo &proFile)
    : name(name)
    , path(path)
    , module(module)
    , proFile(proFile)
{}

GeneratedFileInfo::GeneratedFileInfo()
    : file(MainQmlFile)
    , version(-1)
    , dataChecksum(0)
    , statedChecksum(0)
    , updateReason(Undefined)
{
}

QmlStandaloneApp::QmlStandaloneApp()
    : m_loadDummyData(false)
    , m_orientation(Auto)
    , m_networkEnabled(false)
{
}

QmlStandaloneApp::~QmlStandaloneApp()
{
    clearModulesAndPlugins();
}

QString QmlStandaloneApp::symbianUidForPath(const QString &path)
{
    quint32 hash = 5381;
    for (int i = 0; i < path.size(); ++i) {
        const char c = path.at(i).toAscii();
        hash ^= c + ((c - i) << i % 20) + ((c + i) << (i + 5) % 20) + ((c - 2 * i) << (i + 10) % 20) + ((c + 2 * i) << (i + 15) % 20);
    }
    return QString::fromLatin1("0xE")
            + QString::fromLatin1("%1").arg(hash, 7, 16, QLatin1Char('0')).right(7);
}

void QmlStandaloneApp::setMainQmlFile(const QString &qmlFile)
{
    m_mainQmlFile.setFile(qmlFile);
}

QString QmlStandaloneApp::mainQmlFile() const
{
    return path(MainQml);
}

void QmlStandaloneApp::setOrientation(Orientation orientation)
{
    m_orientation = orientation;
}

QmlStandaloneApp::Orientation QmlStandaloneApp::orientation() const
{
    return m_orientation;
}

void QmlStandaloneApp::setProjectName(const QString &name)
{
    m_projectName = name;
}

QString QmlStandaloneApp::projectName() const
{
    return m_projectName;
}

void QmlStandaloneApp::setProjectPath(const QString &path)
{
    m_projectPath.setFile(path);
}

void QmlStandaloneApp::setSymbianSvgIcon(const QString &icon)
{
    m_symbianSvgIcon = icon;
}

QString QmlStandaloneApp::symbianSvgIcon() const
{
    return path(SymbianSvgIconOrigin);
}

void QmlStandaloneApp::setSymbianTargetUid(const QString &uid)
{
    m_symbianTargetUid = uid;
}

QString QmlStandaloneApp::symbianTargetUid() const
{
    return !m_symbianTargetUid.isEmpty() ? m_symbianTargetUid
        : symbianUidForPath(path(AppPro));
}

void QmlStandaloneApp::setLoadDummyData(bool loadIt)
{
    m_loadDummyData = loadIt;
}

bool QmlStandaloneApp::loadDummyData() const
{
    return m_loadDummyData;
}

void QmlStandaloneApp::setNetworkEnabled(bool enabled)
{
    m_networkEnabled = enabled;
}

bool QmlStandaloneApp::networkEnabled() const
{
    return m_networkEnabled;
}

bool QmlStandaloneApp::setExternalModules(const QStringList &uris,
                                          const QStringList &importPaths)
{
    clearModulesAndPlugins();
    m_importPaths.clear();
    foreach (const QFileInfo &importPath, importPaths) {
        if (!importPath.exists()) {
            m_error = tr("The Qml import path '%1' cannot be found.")
                      .arg(QDir::toNativeSeparators(importPath.filePath()));
            return false;
        } else {
            m_importPaths.append(importPath.canonicalFilePath());
        }
    }
    foreach (const QString &uri, uris) {
        QString uriPath = uri;
        uriPath.replace(QLatin1Char('.'), QLatin1Char('/'));
        const int modulesCount = m_modules.count();
        foreach (const QFileInfo &importPath, m_importPaths) {
            const QFileInfo qmlDirFile(
                    importPath.absoluteFilePath() + QLatin1Char('/')
                    + uriPath + QLatin1Char('/') + qmldir);
            if (qmlDirFile.exists()) {
                if (!addExternalModule(uri, importPath, qmlDirFile))
                    return false;
                break;
            }
        }
        if (modulesCount == m_modules.count()) { // no module was added
            m_error = tr("The Qml module '%1' cannot be found.").arg(uri);
            return false;
        }
    }
    m_error.clear();
    return true;
}

QString QmlStandaloneApp::path(Path path) const
{
    const QString qmlSubDir = QLatin1String("qml/")
                              + (useExistingMainQml() ? m_mainQmlFile.dir().dirName() : m_projectName)
                              + QLatin1Char('/');
    const QString originsRoot = templatesRoot();
    const QString appViewerTargetSubDir = appViewerOriginsSubDir;
    const QString qmlExtension = QLatin1String(".qml");
    const QString mainCppFileName = QLatin1String("main.cpp");
    const QString symbianIconFileName = QLatin1String("symbianicon.svg");
    const QString pathBase = m_projectPath.absoluteFilePath() + QLatin1Char('/')
                             + m_projectName + QLatin1Char('/');
    const QDir appProFilePath(pathBase);

    switch (path) {
        case MainQml:                       return useExistingMainQml() ? m_mainQmlFile.canonicalFilePath()
                                                : pathBase + qmlSubDir + m_projectName + qmlExtension;
        case MainQmlDeployed:               return useExistingMainQml() ? qmlSubDir + m_mainQmlFile.fileName()
                                                : QString(qmlSubDir + m_projectName + qmlExtension);
        case MainQmlOrigin:                 return originsRoot + QLatin1String("qml/app/app.qml");
        case MainCpp:                       return pathBase + mainCppFileName;
        case MainCppOrigin:                 return originsRoot + mainCppFileName;
        case AppPro:                        return pathBase + m_projectName + QLatin1String(".pro");
        case AppProOrigin:                  return originsRoot + QLatin1String("app.pro");
        case AppProPath:                    return pathBase;
        case AppViewerPri:                  return pathBase + appViewerTargetSubDir + appViewerPriFileName;
        case AppViewerPriOrigin:            return originsRoot + appViewerOriginsSubDir + appViewerPriFileName;
        case AppViewerCpp:                  return pathBase + appViewerTargetSubDir + appViewerCppFileName;
        case AppViewerCppOrigin:            return originsRoot + appViewerOriginsSubDir + appViewerCppFileName;
        case AppViewerH:                    return pathBase + appViewerTargetSubDir + appViewerHFileName;
        case AppViewerHOrigin:              return originsRoot + appViewerOriginsSubDir + appViewerHFileName;
        case SymbianSvgIcon:                return pathBase + symbianIconFileName;
        case SymbianSvgIconOrigin:          return !m_symbianSvgIcon.isEmpty() ? m_symbianSvgIcon
                                                : originsRoot + symbianIconFileName;
        case QmlDir:                        return pathBase + qmlSubDir;
        case QmlDirProFileRelative:         return useExistingMainQml() ? appProFilePath.relativeFilePath(m_mainQmlFile.canonicalPath())
                                                : QString(qmlSubDir).remove(qmlSubDir.length() - 1, 1);
        case ModulesDir:                    return QLatin1String("modules");
        default:                            qFatal("QmlStandaloneApp::path() needs more work");
    }
    return QString();
}

static QString insertParameter(const QString &line, const QString &parameter)
{
    return QString(line).replace(QRegExp(QLatin1String("\\([^()]+\\)")),
                                 QLatin1Char('(') + parameter + QLatin1Char(')'));
}

QByteArray QmlStandaloneApp::generateMainCpp(const QString *errorMessage) const
{
    Q_UNUSED(errorMessage)

    QFile sourceFile(path(MainCppOrigin));
    sourceFile.open(QIODevice::ReadOnly);
    Q_ASSERT(sourceFile.isOpen());
    QTextStream in(&sourceFile);

    QByteArray mainCppContent;
    QTextStream out(&mainCppContent, QIODevice::WriteOnly);

    QString line;
    while (!(line = in.readLine()).isNull()) {
        if (line.contains(QLatin1String("// MAINQML"))) {
            line = insertParameter(line, QLatin1Char('"') + path(MainQmlDeployed) + QLatin1Char('"'));
        } else if (line.contains(QLatin1String("// ADDIMPORTPATH"))) {
            if (m_modules.isEmpty())
                continue;
            else
                line = insertParameter(line, QLatin1Char('"') + path(ModulesDir) + QLatin1Char('"'));
        } else if (line.contains(QLatin1String("// ORIENTATION"))) {
            if (m_orientation == Auto)
                continue;
            else
                line = insertParameter(line, QLatin1String("QmlApplicationView::")
                                       + QLatin1String(m_orientation == LockLandscape ?
                                                       "LockLandscape" : "LockPortrait"));
        } else if (line.contains(QLatin1String("// LOADDUMMYDATA"))) {
            continue;
        }
        const int commentIndex = line.indexOf(QLatin1String(" //"));
        if (commentIndex != -1)
            line.truncate(commentIndex);
        out << line << endl;
    };

    return mainCppContent;
}

QByteArray QmlStandaloneApp::generateProFile(const QString *errorMessage) const
{
    Q_UNUSED(errorMessage)

    const QChar comment = QLatin1Char('#');
    QFile proFile(path(AppProOrigin));
    proFile.open(QIODevice::ReadOnly);
    Q_ASSERT(proFile.isOpen());
    QTextStream in(&proFile);

    QByteArray proFileContent;
    QTextStream out(&proFileContent, QIODevice::WriteOnly);

    QString valueOnNextLine;
    bool uncommentNextLine = false;
    QString line;
    while (!(line = in.readLine()).isNull()) {
        if (line.contains(QLatin1String("# TARGETUID3"))) {
            valueOnNextLine = symbianTargetUid();
        } else if (line.contains(QLatin1String("# DEPLOYMENTFOLDERS"))) {
            // Eat lines
            while (!(line = in.readLine()).isNull() &&
                   !line.contains(QLatin1String("# DEPLOYMENTFOLDERS_END")))
            { }
            if (line.isNull())
                break;
            QStringList folders;
            out << "folder_01.source = " << path(QmlDirProFileRelative) << endl;
            out << "folder_01.target = qml" << endl;
            folders.append(QLatin1String("folder_01"));
            int foldersCount = 1;
            foreach (const QmlModule *module, m_modules) {
                if (module->isExternal) {
                    foldersCount ++;
                    const QString folder =
                            QString::fromLatin1("folder_%1").arg(foldersCount, 2, 10, QLatin1Char('0'));
                    folders.append(folder);
                    out << folder << ".source = " << module->path(QmlModule::ContentDir) << endl;
                    out << folder << ".target = " << module->path(QmlModule::DeployedContentBase) << endl;
                }
            }
            out << "DEPLOYMENTFOLDERS = " << folders.join(QLatin1String(" ")) << endl;
        } else if (line.contains(QLatin1String("# ORIENTATIONLOCK")) && m_orientation == QmlStandaloneApp::Auto) {
            uncommentNextLine = true;
        } else if (line.contains(QLatin1String("# NETWORKACCESS")) && !m_networkEnabled) {
            uncommentNextLine = true;
        } else if (line.contains(QLatin1String("# QMLINSPECTOR"))) {
            // ### disabled for now; figure out the private headers problem first.
            //uncommentNextLine = true;
        }

        // Remove all marker comments
        if (line.trimmed().startsWith(comment)
            && line.trimmed().endsWith(comment))
            continue;

        if (!valueOnNextLine.isEmpty()) {
            out << line.left(line.indexOf(QLatin1Char('=')) + 2)
                << QDir::fromNativeSeparators(valueOnNextLine) << endl;
            valueOnNextLine.clear();
            continue;
        }

        if (uncommentNextLine) {
            out << comment << line << endl;
            uncommentNextLine = false;
            continue;
        }
        out << line << endl;
    };

    return proFileContent;
}

void QmlStandaloneApp::clearModulesAndPlugins()
{
    qDeleteAll(m_modules);
    m_modules.clear();
    qDeleteAll(m_cppPlugins);
    m_cppPlugins.clear();
}

bool QmlStandaloneApp::addCppPlugin(const QString &qmldirLine, QmlModule *module)
{
    const QStringList qmldirLineElements =
            qmldirLine.split(QLatin1Char(' '), QString::SkipEmptyParts);
    if (qmldirLineElements.count() < 2) {
        m_error = tr("Invalid '%1' entry in '%2' of module '%3'.")
                  .arg(qmldir_plugin).arg(qmldir).arg(module->uri);
        return false;
    }
    const QString name = qmldirLineElements.at(1);
    const QFileInfo path(module->qmldir.dir(), qmldirLineElements.value(2, QString()));

    // TODO: Add more magic to find a good .pro file..
    const QString proFileName = name + QLatin1String(".pro");
    const QFileInfo proFile_guess1(module->qmldir.dir(), proFileName);
    const QFileInfo proFile_guess2(QString(module->qmldir.dir().absolutePath() + QLatin1String("/../")),
                                   proFileName);
    const QFileInfo proFile_guess3(module->qmldir.dir(),
                                   QFileInfo(module->qmldir.path()).fileName() + QLatin1String(".pro"));
    const QFileInfo proFile_guess4(proFile_guess3.absolutePath() + QLatin1String("/../")
                                   + proFile_guess3.fileName());

    QFileInfo foundProFile;
    if (proFile_guess1.exists()) {
        foundProFile = proFile_guess1.canonicalFilePath();
    } else if (proFile_guess2.exists()) {
        foundProFile = proFile_guess2.canonicalFilePath();
    } else if (proFile_guess3.exists()) {
        foundProFile = proFile_guess3.canonicalFilePath();
    } else if (proFile_guess4.exists()) {
        foundProFile = proFile_guess4.canonicalFilePath();
    } else {
        m_error = tr("No .pro file for plugin '%1' cannot be found.").arg(name);
        return false;
    }
    QmlCppPlugin *plugin =
            new QmlCppPlugin(name, path, module, foundProFile);
    m_cppPlugins.append(plugin);
    module->cppPlugins.insert(name, plugin);
    return true;
}

bool QmlStandaloneApp::addCppPlugins(QmlModule *module)
{
    QFile qmlDirFile(module->qmldir.absoluteFilePath());
    if (qmlDirFile.open(QIODevice::ReadOnly)) {
        QTextStream in(&qmlDirFile);
        QString line;
        while (!(line = in.readLine()).isNull()) {
            line = line.trimmed();
            if (line.startsWith(qmldir_plugin) && !addCppPlugin(line, module))
                return false;
        };
    }
    return true;
}

bool QmlStandaloneApp::addExternalModule(const QString &name, const QFileInfo &dir,
                                         const QFileInfo &contentDir)
{
    QmlModule *module = new QmlModule(name, dir, contentDir, true, this);
    m_modules.append(module);
    return addCppPlugins(module);
}

#ifndef CREATORLESSTEST
QString QmlStandaloneApp::templatesRoot()
{
    return Core::ICore::instance()->resourcePath() + QLatin1String("/templates/qmlapp/");
}

static Core::GeneratedFile file(const QByteArray &data, const QString &targetFile)
{
    Core::GeneratedFile generatedFile(targetFile);
    generatedFile.setBinary(true);
    generatedFile.setBinaryContents(data);
    return generatedFile;
}

Core::GeneratedFiles QmlStandaloneApp::generateFiles(QString *errorMessage) const
{
    Core::GeneratedFiles files;

    if (!useExistingMainQml()) {
        files.append(file(generateFile(GeneratedFileInfo::MainQmlFile, errorMessage), path(MainQml)));
        files.last().setAttributes(Core::GeneratedFile::OpenEditorAttribute);
    }

    files.append(file(generateFile(GeneratedFileInfo::AppProFile, errorMessage), path(AppPro)));
    files.last().setAttributes(Core::GeneratedFile::OpenProjectAttribute);
    files.append(file(generateFile(GeneratedFileInfo::MainCppFile, errorMessage), path(MainCpp)));
    files.append(file(generateFile(GeneratedFileInfo::SymbianSvgIconFile, errorMessage), path(SymbianSvgIcon)));

    files.append(file(generateFile(GeneratedFileInfo::AppViewerPriFile, errorMessage), path(AppViewerPri)));
    files.append(file(generateFile(GeneratedFileInfo::AppViewerCppFile, errorMessage), path(AppViewerCpp)));
    files.append(file(generateFile(GeneratedFileInfo::AppViewerHFile, errorMessage), path(AppViewerH)));

    return files;
}
#endif // CREATORLESSTEST

bool QmlStandaloneApp::useExistingMainQml() const
{
    return !m_mainQmlFile.filePath().isEmpty();
}

QString QmlStandaloneApp::error() const
{
    return m_error;
}

const QList<QmlModule*> QmlStandaloneApp::modules() const
{
    return m_modules;
}

static QByteArray readBlob(const QString &source)
{
    QFile sourceFile(source);
    sourceFile.open(QIODevice::ReadOnly);
    Q_ASSERT(sourceFile.isOpen());
    return sourceFile.readAll();
}

QByteArray QmlStandaloneApp::generateFile(GeneratedFileInfo::File file,
                                          const QString *errorMessage) const
{
    QByteArray data;
    const QString cFileComment = QLatin1String("//");
    const QString proFileComment = QLatin1String("#");
    QString comment = cFileComment;
    bool versionAndChecksum = false;
    switch (file) {
        case GeneratedFileInfo::MainQmlFile:
            data = readBlob(path(MainQmlOrigin));
            break;
        case GeneratedFileInfo::MainCppFile:
            data = generateMainCpp(errorMessage);
            break;
        case GeneratedFileInfo::SymbianSvgIconFile:
            data = readBlob(path(SymbianSvgIconOrigin));
            break;
        case GeneratedFileInfo::AppProFile:
            data = generateProFile(errorMessage);
            comment = proFileComment;
            break;
        case GeneratedFileInfo::AppViewerPriFile:
            data = readBlob(path(AppViewerPriOrigin));
            comment = proFileComment;
            versionAndChecksum = true;
            break;
        case GeneratedFileInfo::AppViewerCppFile:
            data = readBlob(path(AppViewerCppOrigin));
            versionAndChecksum = true;
            break;
        case GeneratedFileInfo::AppViewerHFile:
        default:
            data = readBlob(path(AppViewerHOrigin));
            versionAndChecksum = true;
            break;
    }
    if (!versionAndChecksum)
        return data;
    QByteArray versioned = data;
    versioned.replace('\x0D', "");
    versioned.replace('\x0A', "");
    const quint16 checkSum = qChecksum(versioned.constData(), versioned.length());
    const QString checkSumString = QString::number(checkSum, 16);
    const QString versionString = QString::number(stubVersion());
    const QChar sep = QLatin1Char(' ');
    const QString versionLine =
            comment + sep + fileChecksum + sep + QLatin1String("0x") + checkSumString
            + sep + fileStubVersion + sep + versionString + QLatin1Char('\x0A');
    return versionLine.toAscii() + data;
}

int QmlStandaloneApp::stubVersion()
{
    return 1;
}

static QList<GeneratedFileInfo> updateableFiles(const QString &mainProFile)
{
    QList<GeneratedFileInfo> result;
    static const struct {
        GeneratedFileInfo::File file;
        QString fileName;
    } files[] = {
        {GeneratedFileInfo::AppViewerPriFile, appViewerPriFileName},
        {GeneratedFileInfo::AppViewerHFile, appViewerCppFileName},
        {GeneratedFileInfo::AppViewerCppFile, appViewerHFileName}
    };
    const QFileInfo mainProFileInfo(mainProFile);
    for (int i = 0; i < sizeof files / sizeof files[0]; ++i) {
        const QString fileName = mainProFileInfo.dir().absolutePath()
                + QLatin1Char('/') + appViewerOriginsSubDir + files[i].fileName;
        if (!QFile::exists(fileName))
            continue;
        GeneratedFileInfo file;
        file.file = files[i].file;
        file.fileInfo = QFileInfo(fileName);
        result.append(file);
    }
    return result;
}

QList<GeneratedFileInfo> QmlStandaloneApp::fileUpdates(const QString &mainProFile)
{
    QList<GeneratedFileInfo> result;
    foreach (const GeneratedFileInfo &file, updateableFiles(mainProFile)) {
        GeneratedFileInfo newFile = file;
        QFile readFile(newFile.fileInfo.absoluteFilePath());
        if (!readFile.open(QIODevice::ReadOnly))
           continue;
        const QString firstLine = readFile.readLine();
        const QStringList elements = firstLine.split(QLatin1Char(' '));
        if (elements.count() != 5 || elements.at(1) != fileChecksum
                || elements.at(3) != fileStubVersion)
            continue;
        newFile.version = elements.at(4).toInt();
        newFile.statedChecksum = elements.at(2).toUShort(0, 16);
        QByteArray data = readFile.readAll();
        data.replace('\x0D', "");
        data.replace('\x0A', "");
        newFile.dataChecksum = qChecksum(data.constData(), data.length());
        if (newFile.version < stubVersion())
            newFile.updateReason = GeneratedFileInfo::HasOutdatedVersion;
        else if (newFile.version > stubVersion())
            newFile.updateReason = GeneratedFileInfo::HasFutureVersion;
        else if (newFile.dataChecksum != newFile.statedChecksum)
            newFile.updateReason = GeneratedFileInfo::ContentChanged;
        else
            newFile.updateReason = GeneratedFileInfo::IsUpToDate;
        result.append(newFile);
    }
    return result;
}

bool QmlStandaloneApp::updateFiles(const QList<GeneratedFileInfo> &list, QString &error)
{
    error.clear();
    const QmlStandaloneApp app;
    foreach (const GeneratedFileInfo &info, list) {
        const QByteArray data = app.generateFile(info.file, &error);
        if (!error.isEmpty())
            return false;
        QFile file(info.fileInfo.absoluteFilePath());
        if (!file.open(QIODevice::WriteOnly) || file.write(data) == -1) {
            error = tr("Could not write file '%1'.").arg(QDir::toNativeSeparators(info.fileInfo.canonicalFilePath()));
            return false;
        }
    }
    return true;
}

} // namespace Internal
} // namespace Qt4ProjectManager
