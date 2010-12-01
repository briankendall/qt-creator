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

#include "metainfoparser.h"
#include "metainfo.h"

#include "model/propertyparser.h"
#include <QXmlStreamReader>
#include <QString>
#include <QFile>
#include <QtDebug>
#include <QIcon>

namespace QmlDesigner {
namespace Internal {


MetaInfoParser::MetaInfoParser(const MetaInfo &metaInfo)
        : m_metaInfo(metaInfo)
{
}

void MetaInfoParser::parseFile(const QString &path)
{
    QFile file;
    file.setFileName(path);
    if (!file.open(QIODevice::ReadOnly))
        throw new InvalidMetaInfoException(__LINE__, __FUNCTION__, __FILE__);

    QXmlStreamReader reader;
    reader.setDevice(&file);

    while (!reader.atEnd()) {
        reader.readNext();
        tokenHandler(reader);
    }
    errorHandling(reader, file);
}

void MetaInfoParser::tokenHandler(QXmlStreamReader &reader)
{
    if (reader.isStartElement() && reader.name() == "metainfo")
        handleMetaInfoElement(reader);
}

void MetaInfoParser::handleMetaInfoElement(QXmlStreamReader &reader)
{
    while (!reader.atEnd() && !(reader.isEndElement() && reader.name() == "metainfo")) {
        reader.readNext();
        metaInfoHandler(reader);
    }
}

void MetaInfoParser::metaInfoHandler(QXmlStreamReader &reader)
{
    if (reader.isStartElement())
    {
        if (reader.name() == "node")
            handleNodeElement(reader);
    }
}

void MetaInfoParser::handleNodeElement(QXmlStreamReader &reader)
{
    const QXmlStreamAttributes attributes = reader.attributes();

    const QString className = attributes.value("name").toString();
    const QIcon icon = QIcon(attributes.value("icon").toString());

    if (className.isEmpty()) {
        reader.raiseError("Invalid element 'node' - mandatory attribute 'name' is missing");
        return;
    }

    while (!reader.atEnd() && !(reader.isEndElement() && reader.name() == "node")) {
        reader.readNext();

        handleNodeItemLibraryEntryElement(reader, className, icon);
    }
}

void MetaInfoParser::handleNodeItemLibraryEntryElement(QXmlStreamReader &reader, const QString &className, const QIcon &icon)
{
    if (reader.isStartElement() && reader.name() == "itemlibraryentry")
    {
        const QString name = reader.attributes().value("name").toString();

        ItemLibraryEntry entry;
        entry.setType(className, 4, 7);
        entry.setName(name);
        entry.setIcon(icon);

        QString iconPath = reader.attributes().value("libraryIcon").toString();
        if (!iconPath.isEmpty()) 
            entry.setIconPath(iconPath);

        QString category = reader.attributes().value("category").toString();
        if (!category.isEmpty())
            entry.setCategory(category);

        while (!reader.atEnd() && !(reader.isEndElement() && reader.name() == "itemlibraryentry")) {
            reader.readNext();
            handleItemLibraryEntryPropertyElement(reader, entry);
        }

        m_metaInfo.itemLibraryInfo()->addEntry(entry);
    }
}

void MetaInfoParser::handleItemLibraryEntryPropertyElement(QXmlStreamReader &reader, ItemLibraryEntry &itemLibraryEntry)
{
    if (reader.isStartElement() && reader.name() == "property")
    {
        QXmlStreamAttributes attributes(reader.attributes());
        QString name = attributes.value("name").toString();
        QString type = attributes.value("type").toString();
        QString value = attributes.value("value").toString();
        itemLibraryEntry.addProperty(name, type, value);

        reader.readNext();
    }
}

void MetaInfoParser::errorHandling(QXmlStreamReader &reader, QFile &file)
{
    if (!reader.hasError())
        return;

    qDebug() << QString("Error at %1, %2:%3: %4")
            .arg(file.fileName())
            .arg(reader.lineNumber())
            .arg(reader.columnNumber())
            .arg(reader.errorString());

    file.reset();

    QString fileString = file.readAll();
    QString snippetString;
    int lineCount = 0;
    int position = reader.characterOffset();
    while (position >= 0)
    {
        if (fileString[position] == '\n') {
            if (lineCount > 3)
                break;
            lineCount++;
        }

        snippetString.prepend(fileString[position]);
        position--;
    }

    lineCount = 0;
    position = reader.characterOffset();
    while (position >= 0)
    {
        position++;
        if (fileString[position] == '\n') {
            if (lineCount > 1)
                break;
            lineCount++;
        }

        snippetString.append(fileString[position]);
    }

    qDebug() << snippetString;

}


}
}
