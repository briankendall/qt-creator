/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company.  For licensing terms and
** conditions see http://www.qt.io/terms-conditions.  For further information
** use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file.  Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, The Qt Company gives you certain additional
** rights.  These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#ifndef DEBUGGER_LLDBENGINE
#define DEBUGGER_LLDBENGINE

#include <debugger/debuggerengine.h>
#include <debugger/disassembleragent.h>
#include <debugger/memoryagent.h>
#include <debugger/watchhandler.h>
#include <debugger/debuggertooltipmanager.h>
#include <debugger/debuggerprotocol.h>

#include <utils/consoleprocess.h>
#include <utils/qtcprocess.h>

#include <QPointer>
#include <QProcess>
#include <QQueue>
#include <QMap>
#include <QStack>
#include <QVariant>


namespace Debugger {
namespace Internal {

class WatchData;
class GdbMi;

/* A debugger engine interfacing the LLDB debugger
 * using its Python interface.
 */

class LldbEngine : public DebuggerEngine
{
    Q_OBJECT

public:
    explicit LldbEngine(const DebuggerRunParameters &runParameters);
    ~LldbEngine();

signals:
    void outputReady(const QByteArray &data);

private:
    DebuggerEngine *cppEngine() { return this; }

    void executeStep();
    void executeStepOut();
    void executeNext();
    void executeStepI();
    void executeNextI();

    void setupEngine();
    void startLldb();
    void startLldbStage2();
    void setupInferior();
    void runEngine();
    void shutdownInferior();
    void shutdownEngine();
    void abortDebugger();

    bool canHandleToolTip(const DebuggerToolTipContext &) const;

    void continueInferior();
    void interruptInferior();

    void executeRunToLine(const ContextData &data);
    void executeRunToFunction(const QString &functionName);
    void executeJumpToLine(const ContextData &data);

    void activateFrame(int index);
    void selectThread(ThreadId threadId);
    void fetchFullBacktrace();

    // This should be always the last call in a function.
    bool stateAcceptsBreakpointChanges() const;
    bool acceptsBreakpoint(Breakpoint bp) const;
    void insertBreakpoint(Breakpoint bp);
    void removeBreakpoint(Breakpoint bp);
    void changeBreakpoint(Breakpoint bp);

    void assignValueInDebugger(WatchItem *item, const QString &expr, const QVariant &value);
    void executeDebuggerCommand(const QString &command, DebuggerLanguages languages);

    void loadSymbols(const QString &moduleName);
    void loadAllSymbols();
    void requestModuleSymbols(const QString &moduleName);
    void reloadModules();
    void reloadRegisters();
    void reloadSourceFiles() {}
    void reloadFullStack();
    void reloadDebuggingHelpers();
    void fetchDisassembler(Internal::DisassemblerAgent *);

    bool isSynchronous() const { return true; }
    void setRegisterValue(const QByteArray &name, const QString &value);

    void fetchMemory(Internal::MemoryAgent *, QObject *, quint64 addr, quint64 length);
    void changeMemory(Internal::MemoryAgent *, QObject *, quint64 addr, const QByteArray &data);

    QString errorMessage(QProcess::ProcessError error) const;
    bool hasCapability(unsigned cap) const;

    void handleLldbFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void handleLldbError(QProcess::ProcessError error);
    void readLldbStandardOutput();
    void readLldbStandardError();

    void handleStateNotification(const GdbMi &state);
    void handleLocationNotification(const GdbMi &location);
    void handleOutputNotification(const GdbMi &output);

    void handleResponse(const QByteArray &data);
    void updateAll();
    void doUpdateLocals(const UpdateParameters &params);
    void updateBreakpointData(Breakpoint bp, const GdbMi &bkpt, bool added);
    void fetchStack(int limit);

    void notifyEngineRemoteSetupFinished(const RemoteSetupResult &result);

    void runCommand(const DebuggerCommand &cmd);
    void debugLastCommand();

private:
    DebuggerCommand m_lastDebuggableCommand;

    QByteArray m_inbuffer;
    QString m_scriptFileName;
    Utils::QtcProcess m_lldbProc;
    QString m_lldbCmd;

    // FIXME: Make generic.
    int m_lastAgentId;
    int m_continueAtNextSpontaneousStop;
    QMap<QPointer<DisassemblerAgent>, int> m_disassemblerAgents;
    QMap<QPointer<MemoryAgent>, int> m_memoryAgents;
    QHash<int, QPointer<QObject> > m_memoryAgentTokens;

    QHash<int, DebuggerCommand> m_commandForToken;

    // Console handling.
    Q_SLOT void stubError(const QString &msg);
    Q_SLOT void stubExited();
    Q_SLOT void stubStarted();
    bool prepareCommand();
    Utils::ConsoleProcess m_stubProc;
};

} // namespace Internal
} // namespace Debugger

#endif // DEBUGGER_LLDBENGINE
