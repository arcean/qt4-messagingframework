/****************************************************************************
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the Qt Messaging Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "imapclient.h"
#include "imapauthenticator.h"
#include "imapconfiguration.h"
#include "imapstrategy.h"
#include <longstream_p.h>
#include <qmaillog.h>
#include <qmailmessagebuffer.h>
#include <qmailfolder.h>
#include <qmailnamespace.h>
#include <qmaildisconnected.h>
#include <qmailheartbeattimer.h>
#include <ssoaccountmanager.h>
#include <limits.h>
#include <QFile>
#include <QDir>

#ifdef QT_QMF_HAVE_ZLIB
#define QMFALLOWCOMPRESS 1
#else
#define QMFALLOWCOMPRESS 0
#endif

class MessageFlushedWrapper : public QMailMessageBufferFlushCallback
{
    ImapStrategyContext *context;
public:
    MessageFlushedWrapper(ImapStrategyContext *_context)
        : context(_context)
    {
    }

    void messageFlushed(QMailMessage *message)
    {
        context->messageFlushed(*message);
        context->client()->removeAllFromBuffer(message);
    }
};

class DataFlushedWrapper : public QMailMessageBufferFlushCallback
{
    ImapStrategyContext *context;
    QString uid;
    QString section;
public:
    DataFlushedWrapper(ImapStrategyContext *_context, const QString &_uid, const QString &_section)
        : context(_context)
        , uid(_uid)
        , section(_section)
    {
    }

    void messageFlushed(QMailMessage *message)
    {
        context->dataFlushed(*message, uid, section);
        context->client()->removeAllFromBuffer(message);
    }
};

namespace {

    QString decodeModifiedBase64(QString in)
    {
        //remove  & -
        in.remove(0,1);
        in.remove(in.length()-1,1);

        if(in.isEmpty())
            return "&";

        QByteArray buf(in.length(),static_cast<char>(0));
        QByteArray out(in.length() * 3 / 4 + 2,static_cast<char>(0));

        //chars to numeric
        QByteArray latinChars = in.toLatin1();
        for (int x = 0; x < in.length(); x++) {
            int c = latinChars[x];
            if ( c >= 'A' && c <= 'Z')
                buf[x] = c - 'A';
            if ( c >= 'a' && c <= 'z')
                buf[x] = c - 'a' + 26;
            if ( c >= '0' && c <= '9')
                buf[x] = c - '0' + 52;
            if ( c == '+')
                buf[x] = 62;
            if ( c == ',')
                buf[x] = 63;
        }

        int i = 0; //in buffer index
        int j = i; //out buffer index

        unsigned char z;
        QString result;

        while(i+1 < buf.size())
        {
            out[j] = buf[i] & (0x3F); //mask out top 2 bits
            out[j] = out[j] << 2;
            z = buf[i+1] >> 4;
            out[j] = (out[j] | z);      //first byte retrieved

            i++;
            j++;

            if(i+1 >= buf.size())
                break;

            out[j] = buf[i] & (0x0F);   //mask out top 4 bits
            out[j] = out[j] << 4;
            z = buf[i+1] >> 2;
            z &= 0x0F;
            out[j] = (out[j] | z);      //second byte retrieved

            i++;
            j++;

            if(i+1 >= buf.size())
                break;

            out[j] = buf[i] & 0x03;   //mask out top 6 bits
            out[j] = out[j] <<  6;
            z = buf[i+1];
            out[j] = out[j] | z;  //third byte retrieved

            i+=2; //next byte
            j++;
        }

        //go through the buffer and extract 16 bit unicode network byte order
        for(int z = 0; z < out.count(); z+=2) {
            unsigned short outcode = 0x0000;
            outcode = out[z];
            outcode <<= 8;
            outcode &= 0xFF00;

            unsigned short b = 0x0000;
            b = out[z+1];
            b &= 0x00FF;
            outcode = outcode | b;
            if(outcode)
                result += QChar(outcode);
        }

        return result;
    }

    QString decodeModUTF7(QString in)
    {
        QRegExp reg("&[^&-]*-");

        int startIndex = 0;
        int endIndex = 0;

        startIndex = in.indexOf(reg,endIndex);
        while (startIndex != -1) {
            endIndex = startIndex;
            while(endIndex < in.length() && in[endIndex] != '-')
                endIndex++;
            endIndex++;

            //extract the base64 string from the input string
            QString mbase64 = in.mid(startIndex,(endIndex - startIndex));
            QString unicodeString = decodeModifiedBase64(mbase64);

            //remove encoding
            in.remove(startIndex,(endIndex-startIndex));
            in.insert(startIndex,unicodeString);

            endIndex = startIndex + unicodeString.length();
            startIndex = in.indexOf(reg,endIndex);
        }

        return in;
    }

    QString decodeFolderName(const QString &name)
    {
        return decodeModUTF7(name);
    }
    
    struct FlagInfo
    {
        FlagInfo(QString flagName, quint64 flag, QMailFolder::StandardFolder standardFolder, quint64 messageFlag)
            :_flagName(flagName), _flag(flag), _standardFolder(standardFolder), _messageFlag(messageFlag) {};
        
        QString _flagName;
        quint64 _flag;
        QMailFolder::StandardFolder _standardFolder;
        quint64 _messageFlag;
    };
    
    static void setFolderFlags(QMailAccount *account, QMailFolder *folder, const QString &flags, bool setStandardFlags)
    {
        // Set permitted flags
        bool childCreationPermitted(!flags.contains("\\NoInferiors", Qt::CaseInsensitive));
        bool messagesPermitted(!flags.contains("\\NoSelect", Qt::CaseInsensitive));
        folder->setStatus(QMailFolder::ChildCreationPermitted, childCreationPermitted);
        folder->setStatus(QMailFolder::MessagesPermitted, messagesPermitted);
        if (!folder->id().isValid()) {
            qWarning() << "setFolderFlags must be called on folder in store " << folder->id();
            return;
        }

        if (!setStandardFlags)
            return;

        // Set standard folder flags
        QList<FlagInfo> flagInfoList;
        flagInfoList << FlagInfo("\\Inbox", QMailFolder::Incoming, QMailFolder::InboxFolder, QMailMessage::Incoming)
            << FlagInfo("\\Drafts", QMailFolder::Drafts, QMailFolder::DraftsFolder, QMailMessage::Draft)
            << FlagInfo("\\Trash", QMailFolder::Trash, QMailFolder::TrashFolder, QMailMessage::Trash)
            << FlagInfo("\\Sent", QMailFolder::Sent, QMailFolder::SentFolder, QMailMessage::Sent)
            << FlagInfo("\\Spam", QMailFolder::Junk, QMailFolder::JunkFolder, QMailMessage::Junk);
        
        for (int i = 0; i < flagInfoList.count(); ++i) {
            QString flagName(flagInfoList[i]._flagName);
            quint64 flag(flagInfoList[i]._flag);
            QMailFolder::StandardFolder standardFolder(flagInfoList[i]._standardFolder);
            quint64 messageFlag(flagInfoList[i]._messageFlag);
            bool isFlagged(flags.contains(flagName, Qt::CaseInsensitive));

            folder->setStatus(flag, isFlagged);
            if (isFlagged) {
                QMailFolderId oldFolderId = account->standardFolder(standardFolder);
                if (oldFolderId.isValid() && (oldFolderId != folder->id())) {
                    QMailFolder oldFolder(oldFolderId);
                    oldFolder.setStatus(flag, false);
                    // Do the updates in the right order, so if there is a crash
                    // there will be a graceful recovery next time folders are list.
                    // It is expected that no disconnected move operations will be outstanding
                    // otherwise flags for those messages may be updated incorrectly.
                    // So call exportUpdates before retrieveFolderList
                    QMailMessageKey oldFolderKey(QMailMessageKey::parentFolderId(oldFolderId));
                    if (!QMailStore::instance()->updateMessagesMetaData(oldFolderKey, messageFlag, false)) {
                        qWarning() << "Unable to update messages in folder" << oldFolderId << "to remove flag" << flagName;
                    }
                    if (!QMailStore::instance()->updateFolder(&oldFolder)) {
                        qWarning() << "Unable to update folder" << oldFolderId << "to remove flag" << flagName;
                    }
                }
                if (!oldFolderId.isValid() || (oldFolderId != folder->id())) {
                    account->setStandardFolder(standardFolder, folder->id());                
                    if (!QMailStore::instance()->updateAccount(account)) {
                        qWarning() << "Unable to update account" << account->id() << "to set flag" << flagName;
                    }
                    QMailMessageKey folderKey(QMailMessageKey::parentFolderId(folder->id()));
                    if (!QMailStore::instance()->updateMessagesMetaData(folderKey, messageFlag, true)) {
                        qWarning() << "Unable to update messages in folder" << folder->id() << "to set flag" << flagName;
                    }
                }
            }
        }
    }

    // FIXME: another challenge/response method should be used(SASL ?)
    const QString ssoMethod = QLatin1String("password");

    void ssoSendLogin(const SessionData &sessionData, QMailAccountConfiguration* config, ImapProtocol* protocol)
    {
        Q_ASSERT (config);
        Q_ASSERT (protocol);

        // Find the authentication mode to use
        protocol->sendLogin(*config, sessionData.Secret());
    }
}

class IdleProtocol : public ImapProtocol {
    Q_OBJECT

public:
    IdleProtocol(ImapClient *client, const QMailFolder &folder, QString &password);
    virtual ~IdleProtocol() {}

    virtual void handleIdling() { _client->idling(_folder.id()); }
    virtual bool open(const ImapConfiguration& config, qint64 bufferSize = 10*1024);

signals:
    void idleNewMailNotification(QMailFolderId);
    void idleFlagsChangedNotification(QMailFolderId);
    void openRequest();

protected slots:
    virtual void idleContinuation(ImapCommand, const QString &);
    virtual void idleCommandTransition(ImapCommand, OperationStatus);
    virtual void idleTimeOut();
    virtual void idleTransportError();
    virtual void idleErrorRecovery();

    void ssoResponse(const  SignOn::SessionData &sessionData);
    void ssoSessionError(const  SignOn::Error &code);

protected:
    ImapClient *_client;
    QMailFolder _folder;

private:
    QTimer _idleTimer; // Send a DONE command every 29 minutes
    QTimer _idleRecoveryTimer; // Check command hasn't hung
    bool _waitForSSO;
    QString &_password;
};

IdleProtocol::IdleProtocol(ImapClient *client, const QMailFolder &folder, QString &password)
    :_waitForSSO(false), _password(password)
{
    _client = client;
    _folder = folder;
    connect(this, SIGNAL(continuationRequired(ImapCommand, QString)),
            this, SLOT(idleContinuation(ImapCommand, QString)) );
    connect(this, SIGNAL(completed(ImapCommand, OperationStatus)),
            this, SLOT(idleCommandTransition(ImapCommand, OperationStatus)) );
    connect(this, SIGNAL(connectionError(int,QString)),
            this, SLOT(idleTransportError()) );
    connect(this, SIGNAL(connectionError(QMailServiceAction::Status::ErrorCode,QString)),
            this, SLOT(idleTransportError()) );

    _idleTimer.setSingleShot(true);
    connect(&_idleTimer, SIGNAL(timeout()),
            this, SLOT(idleTimeOut()));
    _idleRecoveryTimer.setSingleShot(true);
    connect(&_idleRecoveryTimer, SIGNAL(timeout()),
            this, SLOT(idleErrorRecovery()));
}

bool IdleProtocol::open(const ImapConfiguration& config, qint64 bufferSize)
{
    _idleRecoveryTimer.start(_client->idleRetryDelay()*1000);
    return ImapProtocol::open(config, bufferSize);
}

void IdleProtocol::idleContinuation(ImapCommand command, const QString &type)
{
    const int idleTimeout = 28*60*1000;

    if (command == IMAP_Idle) {
        if (type == QString("idling")) {
            qMailLog(IMAP) << objectName() << "IDLE: Idle connection established.";
            
            // We are now idling
            _idleTimer.start(idleTimeout);
            _idleRecoveryTimer.stop();

            handleIdling();
        } else if (type == QString("newmail")) {
            qMailLog(IMAP) << objectName() << "IDLE: new mail event occurred";
            // A new mail event occurred during idle 
            emit idleNewMailNotification(_folder.id());
        } else if (type == QString("flagschanged")) {
            qMailLog(IMAP) << objectName() << "IDLE: flags changed event occurred";
            // A flags changed event occurred during idle 
            emit idleFlagsChangedNotification(_folder.id());
        } else {
            qWarning("idleContinuation: unknown continuation event");
        }
    }
}

void IdleProtocol::idleCommandTransition(const ImapCommand command, const OperationStatus status)
{
    if ( status != OpOk ) {
        idleTransportError();
        int oldDelay = _client->idleRetryDelay();
        handleIdling();
        _client->setIdleRetryDelay(oldDelay); // don't modify retry delay on failure
        return;
    }
    
    QMailAccountConfiguration config(_client->account());
    switch( command ) {
        case IMAP_Init:
        {
            if (receivedCapabilities()) {
                // Already received capabilities in unsolicited response, no need to request them again
                setReceivedCapabilities(false);
                idleCommandTransition(IMAP_Capability, status);
                return;
            }
            // We need to request the capabilities
            sendCapability();
            return;
        }
        case IMAP_Capability:
        {
            if (!encrypted()) {
                if (ImapAuthenticator::useEncryption(config.serviceConfiguration("imap4"), capabilities())) {
                    // Switch to encrypted mode
                    sendStartTLS();
                    break;
                }
            }

            // We are now connected
            sendLogin(config, _password);
            return;
        }
        case IMAP_StartTLS:
        {
            sendLogin(config, _password);
            break;
        }
        case IMAP_Login: // Fall through
        case IMAP_Compress:
        {
        bool compressCapable(capabilities().contains("COMPRESS=DEFLATE", Qt::CaseInsensitive));
        if (!encrypted() && QMFALLOWCOMPRESS && compressCapable && !compress()) {
                // Server supports COMPRESS and we are not yet compressing
                sendCompress(); // Must not pipeline compress
                return;
            }

            // Server does not support COMPRESS or already compressing
            sendSelect(_folder);
            return;
        }
        case IMAP_Select:
        {
            sendIdle();
            return;
        }
        case IMAP_Idle:
        {
            // Restart idling (TODO: unless we're closing)
            sendIdle();
            return;
        }
        case IMAP_Logout:
        {
            // Ensure connection is closed on logout
            close();
            return;
        }
        default:        //default = all critical messages
        {
            qMailLog(IMAP) << objectName() << "IDLE: IMAP Idle unknown command response: " << command;
            return;
        }
    }
}

void IdleProtocol::idleTimeOut()
{
    _idleRecoveryTimer.start(_client->idleRetryDelay()*1000); // Detect an unresponsive server
    _idleTimer.stop();
    sendIdleDone();
}

void IdleProtocol::idleTransportError()
{
    qMailLog(IMAP) << objectName()
                   << "IDLE: An IMAP IDLE related error occurred.\n"
                   << "An attempt to automatically recover is scheduled in" << _client->idleRetryDelay() << "seconds.";

    if (inUse())
        close();

    _idleRecoveryTimer.stop();

    QTimer::singleShot(_client->idleRetryDelay()*1000, this, SLOT(idleErrorRecovery()));
}

void IdleProtocol::idleErrorRecovery()
{
    const int oneHour = 60*60;
    _idleRecoveryTimer.stop();

    _client->setIdleRetryDelay(qMin( oneHour, _client->idleRetryDelay()*2 ));

    emit openRequest();
}

void IdleProtocol::ssoResponse(const  SignOn::SessionData &sessionData)
{
    if (_waitForSSO) {
        _waitForSSO = false;
        qMailLog(IMAP) << "IDLE: Got SSO response";
        QMailAccountConfiguration config(_client->account());
        _password = sessionData.Secret();
        ssoSendLogin(sessionData, &config, this);
    }
}

void IdleProtocol::ssoSessionError(const  SignOn::Error &code)
{
    if (_waitForSSO) {
        _waitForSSO = false;
        qMailLog(IMAP) <<  "IDLE: Got SSO error:" << code.type() << code.message();
        idleTransportError();
        handleIdling();
    }
}

ImapClient::ImapClient(QObject* parent)
    : QObject(parent),
      _closeCount(0),
      _waitingForIdle(false),
      _idlesEstablished(false),
      _qresyncEnabled(false),
      _requestRapidClose(false),
      _rapidClosing(false),
      _idleRetryDelay(InitialIdleRetryDelay),
      _pushConnectionsReserved(0),
      _identity(0),
      _session(0),
      _waitForSSO(false),
      loginFailed(false),
      _sendLogin(false),
      password(QString())
{
    static int count(0);
    ++count;

    _protocol.setObjectName(QString("%1").arg(count));
    _strategyContext = new ImapStrategyContext(this);
    _strategyContext->setStrategy(&_strategyContext->synchronizeAccountStrategy);
    connect(&_protocol, SIGNAL(completed(ImapCommand, OperationStatus)),
            this, SLOT(commandCompleted(ImapCommand, OperationStatus)) );
    connect(&_protocol, SIGNAL(mailboxListed(QString,QString)),
            this, SLOT(mailboxListed(QString,QString)));
    connect(&_protocol, SIGNAL(messageFetched(QMailMessage&, const QString &, bool)),
            this, SLOT(messageFetched(QMailMessage&, const QString &, bool)) );
    connect(&_protocol, SIGNAL(dataFetched(QString, QString, QString, int)),
            this, SLOT(dataFetched(QString, QString, QString, int)) );
    connect(&_protocol, SIGNAL(nonexistentUid(QString)),
            this, SLOT(nonexistentUid(QString)) );
    connect(&_protocol, SIGNAL(messageStored(QString)),
            this, SLOT(messageStored(QString)) );
    connect(&_protocol, SIGNAL(messageCopied(QString, QString)),
            this, SLOT(messageCopied(QString, QString)) );
    connect(&_protocol, SIGNAL(messageCreated(QMailMessageId, QString)),
            this, SLOT(messageCreated(QMailMessageId, QString)) );
    connect(&_protocol, SIGNAL(downloadSize(QString, int)),
            this, SLOT(downloadSize(QString, int)) );
    connect(&_protocol, SIGNAL(urlAuthorized(QString)),
            this, SLOT(urlAuthorized(QString)) );
    connect(&_protocol, SIGNAL(folderCreated(QString)),
            this, SLOT(folderCreated(QString)));
    connect(&_protocol, SIGNAL(folderDeleted(QMailFolder)),
            this, SLOT(folderDeleted(QMailFolder)));
    connect(&_protocol, SIGNAL(folderRenamed(QMailFolder, QString)),
            this, SLOT(folderRenamed(QMailFolder, QString)));
    connect(&_protocol, SIGNAL(updateStatus(QString)),
            this, SLOT(transportStatus(QString)) );
    connect(&_protocol, SIGNAL(connectionError(int,QString)),
            this, SLOT(transportError(int,QString)) );
    connect(&_protocol, SIGNAL(connectionError(QMailServiceAction::Status::ErrorCode,QString)),
            this, SLOT(transportError(QMailServiceAction::Status::ErrorCode,QString)) );

    _inactiveTimer.setSingleShot(true);
    connect(&_inactiveTimer, SIGNAL(timeout()),
            this, SLOT(connectionInactive()));

    connect(QMailMessageBuffer::instance(), SIGNAL(flushed()), this, SLOT(messageBufferFlushed()));    

    connect (QMailStore::instance(), SIGNAL(accountsUpdated(QMailAccountIdList)), this, SLOT(onAccountsUpdated(QMailAccountIdList)));
}

ImapClient::~ImapClient()
{
    if (_protocol.inUse()) {
        _protocol.close();
    }
    foreach(const QMailFolderId &id, _monitored.keys()) {
        IdleProtocol *protocol = _monitored.take(id);
        if (protocol->inUse())
            protocol->close();
        delete protocol;
    }
    foreach(QMailMessageBufferFlushCallback *callback, callbacks) {
        QMailMessageBuffer::instance()->removeCallback(callback);
    }
    delete _strategyContext;
    deleteSsoIdentity();
}

// Called to begin executing a strategy
void ImapClient::newConnection()
{
    // Reload the account configuration
    _config = QMailAccountConfiguration(_config.id());
    if (_protocol.loggingOut())
        _protocol.close();
    if (!_protocol.inUse()) {
        _qresyncEnabled = false;
    }
    if (_requestRapidClose && !_inactiveTimer.isActive())
        _rapidClosing = true; // Don't close the connection rapidly if interactive checking has recently occurred
    _requestRapidClose = false;
    _inactiveTimer.stop();

    ImapConfiguration imapCfg(_config);
    if ( imapCfg.mailServer().isEmpty() ) {
        operationFailed(QMailServiceAction::Status::ErrConfiguration, tr("Cannot open connection without IMAP server configuration"));
        return;
    }

    _strategyContext->newConnection();
}

ImapStrategyContext *ImapClient::strategyContext()
{
    return _strategyContext;
}

ImapStrategy *ImapClient::strategy() const
{
    return _strategyContext->strategy();
}

void ImapClient::ssoProcessLogin()
{
    if (_session) {
        SessionData data;
        if(loginFailed) {
            data.setUiPolicy(SignOn::RequestPasswordPolicy);
        }
        if (!_waitForSSO)
        {
            _waitForSSO = true;
            _session->process(data, "password");
        }
    } else
        _protocol.sendLogin(_config, password);
}


void ImapClient::setStrategy(ImapStrategy *strategy)
{
    _strategyContext->setStrategy(strategy);
}

void ImapClient::commandCompleted(ImapCommand command, OperationStatus status)
{
    checkCommandResponse(command, status);
    if (status == OpOk)
        commandTransition(command, status);
}

void ImapClient::checkCommandResponse(ImapCommand command, OperationStatus status)
{
    if ( status != OpOk ) {
        switch ( command ) {
            case IMAP_Enable:
            {
                // Couldn't enable QRESYNC, remove capability and continue
                qMailLog(IMAP) << _protocol.objectName() << "unable to enable QRESYNC";
                QStringList capa(_protocol.capabilities());
                capa.removeAll("QRESYNC");
                capa.removeAll("CONDSTORE");
                _protocol.setCapabilities(capa);
                commandTransition(command, OpOk);
                break;
            }
            case IMAP_UIDStore:
            {
                // Couldn't set a flag, ignore as we can stil continue
                qMailLog(IMAP) << _protocol.objectName() << "could not store message flag";
                commandTransition(command, OpOk);
                break;
            }

            case IMAP_Login:
            {
                if (!loginFailed)
                {
                    loginFailed = true;
                    _sendLogin = true;
                    ssoProcessLogin();
                    return;
                }
                else
                {
                    operationFailed(QMailServiceAction::Status::ErrLoginFailed, _protocol.lastError());
                    return;
                }
            }

            case IMAP_Full:
            {
                operationFailed(QMailServiceAction::Status::ErrFileSystemFull, _protocol.lastError());
                return;
            }

            default:        //default = all critical messages
            {
                QString msg;
                if (_config.id().isValid()) {
                    ImapConfiguration imapCfg(_config);
                    msg = imapCfg.mailServer() + ": ";
                }
                msg.append(_protocol.lastError());

                operationFailed(QMailServiceAction::Status::ErrUnknownResponse, msg);
                return;
            }
        }
    }
    
    switch (command) {
        case IMAP_Full:
            qFatal( "Logic error, IMAP_Full" );
            break;
        case IMAP_Unconnected:
            operationFailed(QMailServiceAction::Status::ErrNoConnection, _protocol.lastError());
            return;
         case IMAP_Login:
            loginFailed = false;
            break;
        default:
            break;
    }

}

void ImapClient::commandTransition(ImapCommand command, OperationStatus status)
{
    switch( command ) {
        case IMAP_Init:
        {
            // We need to request the capabilities. Even in the case that an unsolicited response
            // has been received, as some servers report incomplete info in unsolicited responses,
            // missing the IDLE capability.
            // Currently idle connections must be established before main connection will
            // service requests.
            emit updateStatus( tr("Checking capabilities" ) );
            _protocol.sendCapability();
            break;
        }
        
        case IMAP_Capability:
        {
            if (_protocol.authenticated()) {
                // Received capabilities after logging in, continue with post login process
                Q_ASSERT(_protocol.receivedCapabilities());
                _protocol.setReceivedCapabilities(true);
                commandTransition(IMAP_Login, status);
                return;
            }

            if (!_protocol.encrypted()) {
                if (ImapAuthenticator::useEncryption(_config.serviceConfiguration("imap4"), _protocol.capabilities())) {
                    // Switch to encrypted mode
                    emit updateStatus( tr("Starting TLS" ) );
                    _protocol.sendStartTLS();
                    break;
                }
            }

            // We are now connected
            ImapConfiguration imapCfg(_config);            
            _waitingForIdleFolderIds = configurationIdleFolderIds();

            if (!_idlesEstablished
                && _protocol.supportsCapability("IDLE")
                && !_waitingForIdleFolderIds.isEmpty()
                && _pushConnectionsReserved) {
                _waitingForIdle = true;
                emit updateStatus( tr("Logging in idle connection" ) );
                monitor(_waitingForIdleFolderIds);
            } else {
                if (!imapCfg.pushEnabled()) {
                    foreach(const QMailFolderId &id, _monitored.keys()) {
                        IdleProtocol *protocol = _monitored.take(id);
                        protocol->close();
                        delete protocol;
                    }
                }
                emit updateStatus( tr("Logging in" ) );
                if (password.isEmpty())
                {
                    _sendLogin = true;
                    ssoProcessLogin();
                }
                else
                    _protocol.sendLogin(_config, password);
            }
            break;
        }
        
        case IMAP_Idle_Continuation:
        {
            emit updateStatus( tr("Logging in" ) );
            if (password.isEmpty())
            {
                _sendLogin = true;
                ssoProcessLogin();
            }
            else
                _protocol.sendLogin(_config, password);
            break;
        }
        
        case IMAP_StartTLS:
        {
            // Check capabilities for encrypted mode
            _protocol.sendCapability();
            break;
        }

        case IMAP_Login:
        {
            // After logging in server capabilities reported may change  so we need to request 
            // capabilities again, unless already received in an unsolicited response
            if (!_protocol.receivedCapabilities()) {
                emit updateStatus( tr("Checking capabilities" ) );
                _protocol.sendCapability();
                return;
            }

            // Now that we know the capabilities, check for Reference and idle support
            QMailAccount account(_config.id());
            ImapConfiguration imapCfg(_config);
            bool supportsReferences(_protocol.capabilities().contains("URLAUTH", Qt::CaseInsensitive) &&
                                    _protocol.capabilities().contains("CATENATE", Qt::CaseInsensitive) && 
                                    // No FWOD support for IMAPS
                                    (static_cast<QMailTransport::EncryptType>(imapCfg.mailEncryption()) != QMailTransport::Encrypt_SSL));

            if (((account.status() & QMailAccount::CanReferenceExternalData) && !supportsReferences) ||
                (!(account.status() & QMailAccount::CanReferenceExternalData) && supportsReferences) ||
                (imapCfg.pushCapable() != _protocol.supportsCapability("IDLE")) ||
                (imapCfg.capabilities() != _protocol.capabilities())) {
                account.setStatus(QMailAccount::CanReferenceExternalData, supportsReferences);
                imapCfg.setPushCapable(_protocol.supportsCapability("IDLE"));
                imapCfg.setCapabilities(_protocol.capabilities());
                if (!QMailStore::instance()->updateAccount(&account, &_config)) {
                    qWarning() << "Unable to update account" << account.id() << "to set imap4 configuration";
                }
            }

            bool compressCapable(_protocol.capabilities().contains("COMPRESS=DEFLATE", Qt::CaseInsensitive));
            if (!_protocol.encrypted() && QMFALLOWCOMPRESS && compressCapable && !_protocol.compress()) {
                _protocol.sendCompress(); // MUST not pipeline compress
                return;
            }
            // Server does not support compression, continue with post compress step
            commandTransition(IMAP_Compress, status);
            return;
        }

        case IMAP_Compress:
        {
            // Sent a compress, or logged in and server doesn't support compress
            if (!_protocol.capabilities().contains("QRESYNC", Qt::CaseInsensitive)) {
                _strategyContext->commandTransition(IMAP_Login, status);
            } else {
                if (!_qresyncEnabled) {
                    _protocol.sendEnable("QRESYNC CONDSTORE");
                    _qresyncEnabled = true;
                }
            }
            break;
        }

        case IMAP_Enable:
        {
            // Equivalent to having just logged in
            _strategyContext->commandTransition(IMAP_Login, status);
            break;
        }

        case IMAP_Noop:
        {
            _inactiveTimer.start(_closeCount ? MaxTimeBeforeNoop : ImapConfiguration(_config).timeTillLogout() % MaxTimeBeforeNoop);
            break;
        }

        case IMAP_Logout:
        {
            // Ensure connection is closed on logout
            _protocol.close();
            return;
        }

        case IMAP_Select:
        case IMAP_Examine:
        case IMAP_QResync:
        {
            if (_protocol.mailbox().isSelected()) {
                const ImapMailboxProperties &properties(_protocol.mailbox());

                // See if we have anything to record about this mailbox
                QMailFolder folder(properties.id);

                bool modified(false);
                if ((folder.serverCount() != properties.exists) || (folder.serverUnreadCount() != properties.unseen)) {
                    folder.setServerCount(properties.exists);
                    folder.setServerUnreadCount(properties.unseen);
                    modified = true;

                    // See how this compares to the local mailstore count
                    updateFolderCountStatus(&folder);
                }
                
                QString supportsForwarded(properties.permanentFlags.contains("$Forwarded", Qt::CaseInsensitive) ? "true" : QString());
                if (folder.customField("qmf-supports-forwarded") != supportsForwarded) {
                    if (supportsForwarded.isEmpty()) {
                        folder.removeCustomField("qmf-supports-forwarded");
                    } else {
                        folder.setCustomField("qmf-supports-forwarded", supportsForwarded);
                    }
                    modified = true;
                }

                if (modified) {
                    if (!QMailStore::instance()->updateFolder(&folder)) {
                        qWarning() << "Unable to update folder" << folder.id() << "to update server counts!";
                    }
                }
            }
            // fall through
        }
        
        default:
        {
            _strategyContext->commandTransition(command, status);
            break;
        }
    }
}

/*  Mailboxes retrieved from the server goes here.  If the INBOX mailbox
    is new, it means it is the first time the account is used.
*/
void ImapClient::mailboxListed(const QString &flags, const QString &path)
{
    QMailFolderId parentId;
    QMailFolderId boxId;

    QMailAccount account(_config.id());

    QString mailboxPath;

    if(_protocol.delimiterUnknown())
        qWarning() << "Delimiter has not yet been discovered, which is essential to know the structure of a mailbox";

    QStringList list = _protocol.flatHierarchy() ? QStringList(path) : path.split(_protocol.delimiter());

    for ( QStringList::Iterator it = list.begin(); it != list.end(); ++it ) {
        
        if (!mailboxPath.isEmpty())
            mailboxPath.append(_protocol.delimiter());
        mailboxPath.append(*it);

        boxId = mailboxId(mailboxPath);
        if (boxId.isValid()) {
            // This element already exists
            if (mailboxPath == path) {
                QMailFolder folder(boxId); 
                QMailFolder folderOriginal(folder); 
                setFolderFlags(&account, &folder, flags, _protocol.capabilities().contains("XLIST"));
                
                if (folder.status() != folderOriginal.status()) {
                    if (!QMailStore::instance()->updateFolder(&folder)) {
                        qWarning() << "Unable to update folder for account:" << folder.parentAccountId() << "path:" << folder.path();
                    }
                }
                
                _strategyContext->mailboxListed(folder, flags);
            }

            parentId = boxId;
        } else {
            // This element needs to be created
            QMailFolder folder(mailboxPath, parentId, _config.id());
            folder.setDisplayName(decodeFolderName(*it));
            folder.setStatus(QMailFolder::SynchronizationEnabled, true);
            folder.setStatus(QMailFolder::Incoming, true);

            // The reported flags pertain to the listed folder only
            QString folderFlags;
            if (mailboxPath == path) {
                folderFlags = flags;
            }

            if(QString::compare(path, "INBOX", Qt::CaseInsensitive) == 0) {
                //don't let inbox be deleted/renamed
                folder.setStatus(QMailFolder::DeletionPermitted, false);
                folder.setStatus(QMailFolder::RenamePermitted, false);
                folderFlags.append(" \\Inbox");
            } else {
                folder.setStatus(QMailFolder::DeletionPermitted, true);
                folder.setStatus(QMailFolder::RenamePermitted, true);
            }

            // Only folders beneath the base folder are relevant
            QString path(folder.path());
            QString baseFolder(_strategyContext->baseFolder());

            if (baseFolder.isEmpty() || 
                (path.startsWith(baseFolder, Qt::CaseInsensitive) && (path.length() == baseFolder.length())) ||
                (path.startsWith(baseFolder + _protocol.delimiter(), Qt::CaseInsensitive))) {
                if (!QMailStore::instance()->addFolder(&folder)) {
                    qWarning() << "Unable to add folder for account:" << folder.parentAccountId() << "path:" << folder.path();
                }
            }
            
            setFolderFlags(&account, &folder, folderFlags, _protocol.capabilities().contains("XLIST")); // requires valid folder.id()
            _strategyContext->mailboxListed(folder, folderFlags);
            
            if (!QMailStore::instance()->updateFolder(&folder)) {
                qWarning() << "Unable to update folder for account:" << folder.parentAccountId() << "path:" << folder.path();
            }

            boxId = mailboxId(mailboxPath);
            parentId = boxId;
        }
    }
}

void ImapClient::messageFetched(QMailMessage& mail, const QString &detachedFilename, bool structureOnly)
{
    if (structureOnly) {
        mail.setParentAccountId(_config.id());

        // Some properties are inherited from the folder
        const ImapMailboxProperties &properties(_protocol.mailbox());

        mail.setParentFolderId(properties.id);

        if (properties.status & QMailFolder::Incoming) {
            mail.setStatus(QMailMessage::Incoming, true); 
        }
        if (properties.status & QMailFolder::Outgoing) {
            mail.setStatus(QMailMessage::Outgoing, true); 
        }
        if (properties.status & QMailFolder::Drafts) {
            mail.setStatus(QMailMessage::Draft, true); 
        }
        if (properties.status & QMailFolder::Sent) {
            mail.setStatus(QMailMessage::Sent, true); 
        }
        if (properties.status & QMailFolder::Trash) {
            mail.setStatus(QMailMessage::Trash, true); 
        }
        if (properties.status & QMailFolder::Junk) {
            mail.setStatus(QMailMessage::Junk, true); 
        }
        mail.setStatus(QMailMessage::CalendarInvitation, mail.hasCalendarInvitation());
    } else {
        // We need to update the message from the existing data
        QMailMessageMetaData existing(mail.serverUid(), _config.id());
        if (existing.id().isValid()) {
            // Record the status fields that may have been updated
            bool replied(mail.status() & QMailMessage::Replied);
            bool readElsewhere(mail.status() & QMailMessage::ReadElsewhere);
            bool importantElsewhere(mail.status() & QMailMessage::ImportantElsewhere);
            bool contentAvailable(mail.status() & QMailMessage::ContentAvailable);
            bool partialContentAvailable(mail.status() & QMailMessage::PartialContentAvailable);

            mail.setId(existing.id());
            mail.setParentAccountId(existing.parentAccountId());
            mail.setParentFolderId(existing.parentFolderId());
            mail.setStatus(existing.status());
            mail.setContent(existing.content());
            mail.setReceivedDate(existing.receivedDate());
            QMailDisconnected::copyPreviousFolder(existing, &mail);
            mail.setInResponseTo(existing.inResponseTo());
            mail.setResponseType(existing.responseType());
            mail.setContentScheme(existing.contentScheme());
            mail.setContentIdentifier(existing.contentIdentifier());
            mail.setCustomFields(existing.customFields());
            mail.setParentThreadId(existing.parentThreadId());

            // Preserve the status flags determined by the protocol
            mail.setStatus(QMailMessage::Replied, replied);
            mail.setStatus(QMailMessage::ReadElsewhere, readElsewhere);
            mail.setStatus(QMailMessage::ImportantElsewhere, importantElsewhere);
            if ((mail.status() & QMailMessage::ContentAvailable) || contentAvailable) {
                mail.setStatus(QMailMessage::ContentAvailable, true);
            }
            if ((mail.status() & QMailMessage::PartialContentAvailable) || partialContentAvailable) {
                mail.setStatus(QMailMessage::PartialContentAvailable, true);
            }
        } else {
            qWarning() << "Unable to find existing message for uid:" << mail.serverUid() << "account:" << _config.id();
        }
    }
    mail.setCustomField("qmf-detached-filename", detachedFilename);

    _classifier.classifyMessage(mail);


    QMailMessage *bufferMessage(new QMailMessage(mail));
    _bufferedMessages.append(bufferMessage);
    if (_strategyContext->messageFetched(*bufferMessage)) {
        removeAllFromBuffer(bufferMessage);
        return;
    }
    QMailMessageBufferFlushCallback *callback = new MessageFlushedWrapper(_strategyContext);
    callbacks << callback;
    QMailMessageBuffer::instance()->setCallback(bufferMessage, callback);
}


void ImapClient::folderCreated(const QString &folder)
{
    mailboxListed(QString(), folder);
    _strategyContext->folderCreated(folder);
}

void ImapClient::folderDeleted(const QMailFolder &folder)
{
    _strategyContext->folderDeleted(folder);
}

void ImapClient::folderRenamed(const QMailFolder &folder, const QString &newPath)
{
    _strategyContext->folderRenamed(folder, newPath);
}

static bool updateParts(QMailMessagePart &part, const QByteArray &bodyData)
{
    static const QByteArray newLine(QMailMessage::CRLF);
    static const QByteArray marker("--");
    static const QByteArray bodyDelimiter(newLine + newLine);

    if (part.multipartType() == QMailMessage::MultipartNone) {
        // The body data is for this part only
        part.setBody(QMailMessageBody::fromData(bodyData, part.contentType(), part.transferEncoding(), QMailMessageBody::AlreadyEncoded));
        part.removeHeaderField("X-qmf-internal-partial-content");
    } else {
        const QByteArray &boundary(part.contentType().boundary());

        // TODO: this code is replicated from qmailmessage.cpp (parseMimeMultipart)

        // Separate the body into parts delimited by the boundary, and update them individually
        QByteArray partDelimiter = marker + boundary;
        QByteArray partTerminator = QByteArray(1, QMailMessage::LineFeed) + partDelimiter + marker;

        int startPos = bodyData.indexOf(partDelimiter, 0);
        if (startPos != -1)
            startPos += partDelimiter.length();

        // Subsequent delimiters include the leading newline
        partDelimiter.prepend(newLine);

        const char *baseAddress = bodyData.constData();
        int partIndex = 0;

        int endPos = bodyData.indexOf(partTerminator, 0);
        if (endPos > 0 && bodyData[endPos - 1] == QMailMessage::CarriageReturn) {
            endPos--;
        }
        while ((startPos != -1) && (startPos < endPos)) {
            // Skip the boundary line
            startPos = bodyData.indexOf(newLine, startPos);

            if ((startPos != -1) && (startPos < endPos)) {
                // Parse the section up to the next boundary marker
                int nextPos = bodyData.indexOf(partDelimiter, startPos);

                if (nextPos > 0 && bodyData[nextPos - 1] == QMailMessage::CarriageReturn) {
                    nextPos--;
                }

                // Find the beginning of the part body
                startPos = bodyData.indexOf(bodyDelimiter, startPos);
                if ((startPos != -1) && (startPos < nextPos)) {
                    startPos += bodyDelimiter.length();

                    QByteArray partBodyData(QByteArray::fromRawData(baseAddress + startPos, (nextPos - startPos)));
                    if (!updateParts(part.partAt(partIndex), partBodyData))
                        return false;

                    ++partIndex;
                }

                if (bodyData[nextPos] == QMailMessage::CarriageReturn) {
                    nextPos++;
                }
                // Move to the next part
                startPos = nextPos + partDelimiter.length();
            }
        }
    }

    return true;
}

class TemporaryFile
{
    enum { BufferSize = 4096 };

    QString _fileName;

public:
    TemporaryFile(const QString &fileName) : _fileName(QMail::tempPath() + fileName) {}

    QString fileName() const { return _fileName; }

    bool write(const QMailMessageBody &body)
    {
        QFile file(_fileName);
        if (!file.open(QIODevice::WriteOnly)) {
            qWarning() << "Unable to open file for writing:" << _fileName;
            return false;
        }

        // We need to write out the data still encoded - since we're deconstructing
        // server-side data, the meaning of the parameter is reversed...
        QMailMessageBody::EncodingFormat outputFormat(QMailMessageBody::Decoded);

        QDataStream out(&file);
        if (!body.toStream(out, outputFormat)) {
            qWarning() << "Unable to write existing body to file:" << _fileName;
            return false;
        }

        file.close();
        return true;
    }

    static bool copyFileData(QFile &srcFile, QFile &dstFile, qint64 maxLength = -1)
    {
        char buffer[BufferSize];

        while (!srcFile.atEnd() && (maxLength != 0)) {
            qint64 readSize = ((maxLength > 0) ? qMin<qint64>(maxLength, BufferSize) : static_cast<qint64>(BufferSize));
            readSize = srcFile.read(buffer, readSize);
            if (readSize == -1) {
                return false;
            }

            if (maxLength > 0) {
                maxLength -= readSize;
            }
            if (dstFile.write(buffer, readSize) != readSize) {
                return false;
            }
        }

        return true;
    }

    bool appendAndReplace(const QString &fileName)
    {
        {
            QFile existingFile(_fileName);
            QFile dataFile(fileName);

            if (!existingFile.exists()) {
                if (!QFile::copy(fileName, _fileName)) {
                    qWarning() << "Unable to copy - fileName:" << fileName << "_fileName:" << _fileName;
                    return false;
                }
            } else if (existingFile.open(QIODevice::Append)) {
                // On windows, this file will be unwriteable if it is open elsewhere
                if (dataFile.open(QIODevice::ReadOnly)) {
                    if (!copyFileData(dataFile, existingFile)) {
                        qWarning() << "Unable to append data to file:" << _fileName;
                        return false;
                    }
                } else {
                    qWarning() << "Unable to open new data for read:" << fileName;
                    return false;
                }
            } else if (existingFile.open(QIODevice::ReadOnly)) {
                if (dataFile.open(QIODevice::WriteOnly)) {
                    qint64 existingLength = QFileInfo(existingFile).size();
                    qint64 dataLength = QFileInfo(dataFile).size();

                    if (!dataFile.resize(existingLength + dataLength)) {
                        qWarning() << "Unable to resize data file:" << fileName;
                        return false;
                    } else {
                        QFile readDataFile(fileName);
                        if (!readDataFile.open(QIODevice::ReadOnly)) {
                            qWarning() << "Unable to reopen data file for read:" << fileName;
                            return false;
                        }

                        // Copy the data to the end of the file
                        dataFile.seek(existingLength);
                        if (!copyFileData(readDataFile, dataFile, dataLength)) {
                            qWarning() << "Unable to copy existing data in file:" << fileName;
                            return false;
                        }
                    }

                    // Copy the existing data before the new data
                    dataFile.seek(0);
                    if (!copyFileData(existingFile, dataFile, existingLength)) {
                        qWarning() << "Unable to copy existing data to file:" << fileName;
                        return false;
                    }
                } else {
                    qWarning() << "Unable to open new data for write:" << fileName;
                    return false;
                }

                // The complete data is now in the new file
                if (!QFile::remove(_fileName)) {
                    qWarning() << "Unable to remove pre-existing:" << _fileName;
                    return false;
                }

                _fileName = fileName;
                return true;
            } else {
                qWarning() << "Unable to open:" << _fileName;
                return false;
            }
        }

        if (!QFile::remove(fileName)) {
            qWarning() << "Unable to remove:" << fileName;
            return false;
        }
        if (!QFile::rename(_fileName, fileName)) {
            qWarning() << "Unable to rename:" << _fileName << fileName;
            return false;
        }

        _fileName = fileName;
        return true;
    }
};

void ImapClient::dataFetched(const QString &uid, const QString &section, const QString &fileName, int size)
{
    static const QString tempDir = QMail::tempPath();

    QMailMessage *mail;
    bool inBuffer = false;

    foreach (QMailMessage *msg, _bufferedMessages) {
        if (msg->serverUid() == uid) {
            mail = msg;
            inBuffer = true;
            break;
        }
    }
    if (!inBuffer) {
        mail = new QMailMessage(uid, _config.id());
    }

    detachedTempFiles.insertMulti(mail->id(), fileName);

    if (mail->id().isValid()) {
        if (section.isEmpty()) {
            // This is the body of the message, or a part thereof
            uint existingSize = 0;
            if (mail->hasBody()) {
                existingSize = mail->body().length();

                // Write the existing data to a temporary file
                TemporaryFile tempFile("mail-" + uid + "-body");
                if (!tempFile.write(mail->body())) {
                    qWarning() << "Unable to write existing body to file:" << tempFile.fileName();
                    return;
                }

                if (!tempFile.appendAndReplace(fileName)) {
                    qWarning() << "Unable to append data to existing body file:" << tempFile.fileName();
                    return;
                } else {
                    // The appended content file is now named 'fileName'
                }
            }

            // Set the content into the mail
            mail->setBody(QMailMessageBody::fromFile(fileName, mail->contentType(), mail->transferEncoding(), QMailMessageBody::AlreadyEncoded));
            mail->setStatus(QMailMessage::PartialContentAvailable, true);

            const uint totalSize(existingSize + size);
            if (totalSize >= mail->contentSize()) {
                // We have all the data for this message body
                mail->setStatus(QMailMessage::ContentAvailable, true);
            } 

        } else {
            // This is data for a sub-part of the message
            QMailMessagePart::Location partLocation(section);
            if (!partLocation.isValid(false)) {
                qWarning() << "Unable to locate part for invalid section:" << section;
                return;
            } else if (!mail->contains(partLocation)) {
                qWarning() << "Unable to update invalid part for section:" << section;
                return;
            }

            QMailMessagePart &part = mail->partAt(partLocation);

            int existingSize = 0;
            if (part.hasBody()) {
                existingSize = part.body().length();

                // Write the existing data to a temporary file
                TemporaryFile tempFile("mail-" + uid + "-part-" + section);
                if (!tempFile.write(part.body())) {
                    qWarning() << "Unable to write existing body to file:" << tempFile.fileName();
                    return;
                }

                if (!tempFile.appendAndReplace(fileName)) {
                    qWarning() << "Unable to append data to existing body file:" << tempFile.fileName();
                    return;
                } else {
                    // The appended content file is now named 'fileName'
                }
            } 

            if (part.multipartType() == QMailMessage::MultipartNone) {
                // The body data is for this part only
                part.setBody(QMailMessageBody::fromFile(fileName, part.contentType(), part.transferEncoding(), QMailMessageBody::AlreadyEncoded));

                const int totalSize(existingSize + size);
                if (totalSize >= part.contentDisposition().size()) {
                    // We have all the data for this part
                    part.removeHeaderField("X-qmf-internal-partial-content");
                } else {
                    // We only have a portion of the part data
                    part.setHeaderField("X-qmf-internal-partial-content", "true");
                }
            } else {
                // Find the part bodies in the retrieved data
                QFile file(fileName);
                if (!file.open(QIODevice::ReadOnly)) {
                    qWarning() << "Unable to read fetched data from:" << fileName << "- error:" << file.error();
                    operationFailed(QMailServiceAction::Status::ErrFrameworkFault, tr("Unable to read fetched data"));
                    return;
                }

                uchar *data = file.map(0, size);
                if (!data) {
                    qWarning() << "Unable to map fetched data from:" << fileName << "- error:" << file.error();
                    operationFailed(QMailServiceAction::Status::ErrFrameworkFault, tr("Unable to map fetched data"));
                    return;
                }

                // Update the part bodies that we have retrieved
                QByteArray bodyData(QByteArray::fromRawData(reinterpret_cast<const char*>(data), size));
                if (!updateParts(part, bodyData)) {
                    operationFailed(QMailServiceAction::Status::ErrFrameworkFault, tr("Unable to update part body"));
                    return;
                }

                // These updates cannot be effected by storing the data file directly
                if (!mail->customField("qmf-detached-filename").isEmpty()) {
                    QFile::remove(mail->customField("qmf-detached-filename"));
                    mail->removeCustomField("qmf-detached-filename");
                }
            }
        }

        if (inBuffer) {
            return;
        }

        _bufferedMessages.append(mail);
        _strategyContext->dataFetched(*mail, uid, section);
        QMailMessageBufferFlushCallback *callback = new DataFlushedWrapper(_strategyContext, uid, section);
        callbacks << callback;
        QMailMessageBuffer::instance()->setCallback(mail, callback);
    } else {
        qWarning() << "Unable to handle dataFetched - uid:" << uid << "section:" << section;
        operationFailed(QMailServiceAction::Status::ErrFrameworkFault, tr("Unable to handle dataFetched without context"));
    }
}

void ImapClient::nonexistentUid(const QString &uid)
{
    _strategyContext->nonexistentUid(uid);
}

void ImapClient::messageStored(const QString &uid)
{
    _strategyContext->messageStored(uid);
}

void ImapClient::messageCopied(const QString &copiedUid, const QString &createdUid)
{
    _strategyContext->messageCopied(copiedUid, createdUid);
}

void ImapClient::messageCreated(const QMailMessageId &id, const QString &uid)
{
    _strategyContext->messageCreated(id, uid);
}

void ImapClient::downloadSize(const QString &uid, int size)
{
    _strategyContext->downloadSize(uid, size);
}

void ImapClient::urlAuthorized(const QString &url)
{
    _strategyContext->urlAuthorized(url);
}

void ImapClient::setAccount(const QMailAccountId &id)
{
    if (_protocol.inUse() && (id != _config.id())) {
        operationFailed(QMailServiceAction::Status::ErrConnectionInUse, tr("Cannot send message; socket in use"));
        return;
    }

    _config = QMailAccountConfiguration(id);

    SSOAccountManager manager;
    Accounts::Account* account = manager->account(static_cast<Accounts::AccountId>(id.toULongLong()));
    Q_ASSERT(account);

    Accounts::ServiceList emailServices = account->enabledServices();
    Q_ASSERT_X (1 == emailServices.count(), Q_FUNC_INFO, "Account must have one active e-mail service");

    account->selectService(emailServices.first());

    quint32 credentialsId = account->valueAsInt("imap4/CredentialsId", 0);

    // For example Google account-ui plugin stores credentialsId by using SSO setCredentialsId(const qint32 id) method.
    // So, if there is no specific credentials for the service than we should check regular credentialsId.
    if (credentialsId == 0)
    {
        credentialsId = account->credentialsId();
    }

    if (password.isEmpty() && !_waitForSSO) {
        deleteSsoIdentity();

        _identity = SignOn::Identity::existingIdentity(credentialsId, this);
        if (_identity) {
            qMailLog(IMAP) << Q_FUNC_INFO << "SSO identity is found for account id: "<< id;
            // FIXME: challenge/response mechanism should be used better
            // another SSO plugin should be used (SASL ?)
            _session = _identity->createSession(ssoMethod);
            Q_ASSERT (_session);
            ENFORCE(connect(_session, SIGNAL(response(SignOn::SessionData)),
                            this, SLOT(ssoResponse(SignOn::SessionData))));
            ENFORCE(connect(_session, SIGNAL(error(SignOn::Error)),
                            this, SLOT(ssoSessionError(SignOn::Error))));
            SessionData data;
            _waitForSSO = true;
            _session->process(data, "password");
        } else {
            _session = 0;
            qMailLog(IMAP) << Q_FUNC_INFO << "SSO identity is not found for account id: "<< id
                           << "accounts configuration will be used";
        }
    }
}

void ImapClient::accountRemoved(const QMailAccountId &accountId)
{
    //removing sso Identity
    if (_config.id() == accountId) {
        removeSsoIdentity();
    }
}

QMailAccountId ImapClient::account() const
{
    return _config.id();
}

void ImapClient::transportError(int code, const QString &msg)
{
    operationFailed(code, msg);
}

void ImapClient::transportError(QMailServiceAction::Status::ErrorCode code, const QString &msg)
{
    operationFailed(code, msg);
}

void ImapClient::closeConnection()
{
    _inactiveTimer.stop();
    if ( _protocol.inUse() ) {
        _protocol.close();
    }
}

void ImapClient::transportStatus(const QString& status)
{
    emit updateStatus(status);
}

void ImapClient::cancelTransfer(QMailServiceAction::Status::ErrorCode code, const QString &text)
{
    operationFailed(code, text);
    if(_session) {
        _session->cancel();
        _waitForSSO = false;
    }
}

void ImapClient::retrieveOperationCompleted()
{
    deactivateConnection();
    
    // This retrieval may have been asynchronous
    emit allMessagesReceived();

    // Or it may have been requested by a waiting client
    emit retrievalCompleted();
}

void ImapClient::deleteSsoIdentity()
{
    if (_identity) {
        Q_ASSERT (_session);
        _identity->destroySession(_session);
        delete _identity;
        _identity = 0;
    }
}

void ImapClient::removeSsoIdentity()
{
    if (_identity) {
        Q_ASSERT (_session);
        _identity->destroySession(_session);
        _identity->remove();
        delete _identity;
        _identity = 0;
    }
}

void ImapClient::deactivateConnection()
{
    int time(ImapConfiguration(_config).timeTillLogout());
    if (_rapidClosing)
        time = 0;
    _closeCount = time / MaxTimeBeforeNoop;
    _inactiveTimer.start(_closeCount ? MaxTimeBeforeNoop : time);
}

void ImapClient::connectionInactive()
{
    Q_ASSERT(_closeCount >= 0);
    if (_closeCount == 0) {
        _rapidClosing = false;
        if ( _protocol.connected()) {
            emit updateStatus( tr("Logging out") );
            _protocol.sendLogout();
            // Client MUST read tagged response, but if connection hangs in logout state newConnection will autoClose.
        } else {
            closeConnection();
        }
    } else {
        --_closeCount;
        _protocol.sendNoop();
    }
}

void ImapClient::operationFailed(int code, const QString &text)
{
    if (_protocol.inUse())
        _protocol.close();

    emit errorOccurred(code, text);
}

void ImapClient::operationFailed(QMailServiceAction::Status::ErrorCode code, const QString &text)
{
    if (_protocol.inUse())
        _protocol.close();

    emit errorOccurred(code, text);
}

QMailFolderId ImapClient::mailboxId(const QString &path) const
{
    QMailFolderIdList folderIds = QMailStore::instance()->queryFolders(QMailFolderKey::parentAccountId(_config.id()) & QMailFolderKey::path(path));
    if (folderIds.count() == 1)
        return folderIds.first();
    
    return QMailFolderId();
}

QMailFolderIdList ImapClient::mailboxIds() const
{
    return QMailStore::instance()->queryFolders(QMailFolderKey::parentAccountId(_config.id()), QMailFolderSortKey::path(Qt::AscendingOrder));
}

QStringList ImapClient::serverUids(const QMailFolderId &folderId) const
{
    // We need to list both the messages in the mailbox, and those moved to the
    // Trash folder which are still in the mailbox as far as the server is concerned
    return serverUids(messagesKey(folderId) | trashKey(folderId));
}

QStringList ImapClient::serverUids(const QMailFolderId &folderId, quint64 messageStatusFilter, bool set) const
{
    QMailMessageKey statusKey(QMailMessageKey::status(messageStatusFilter, QMailDataComparator::Includes));
    return serverUids((messagesKey(folderId) | trashKey(folderId)) & (set ? statusKey : ~statusKey));
}

QStringList ImapClient::serverUids(QMailMessageKey key) const
{
    QStringList uidList;

    foreach (const QMailMessageMetaData& r, QMailStore::instance()->messagesMetaData(key, QMailMessageKey::ServerUid))
        if (!r.serverUid().isEmpty())
            uidList.append(r.serverUid());

    return uidList;
}

QMailMessageKey ImapClient::messagesKey(const QMailFolderId &folderId) const
{
    return (QMailMessageKey::parentAccountId(_config.id()) &
            QMailDisconnected::sourceKey(folderId));
}

QMailMessageKey ImapClient::trashKey(const QMailFolderId &folderId) const
{
    return (QMailMessageKey::parentAccountId(_config.id()) &
            QMailDisconnected::sourceKey(folderId) &
            QMailMessageKey::status(QMailMessage::Trash));
}

QStringList ImapClient::deletedMessages(const QMailFolderId &folderId) const
{
    QStringList serverUidList;

    foreach (const QMailMessageRemovalRecord& r, QMailStore::instance()->messageRemovalRecords(_config.id(), folderId))
        if (!r.serverUid().isEmpty())
            serverUidList.append(r.serverUid());

    return serverUidList;
}

void ImapClient::updateFolderCountStatus(QMailFolder *folder)
{
    // Find the local mailstore count for this folder
    QMailMessageKey folderContent(QMailDisconnected::sourceKey(folder->id()));
    folderContent &= ~QMailMessageKey::status(QMailMessage::Removed);

    uint count = QMailStore::instance()->countMessages(folderContent);
    folder->setStatus(QMailFolder::PartialContent, (count < folder->serverCount()));
}

bool ImapClient::idlesEstablished()
{
    ImapConfiguration imapCfg(_config);
    if (!imapCfg.pushEnabled())
        return true;

    return _idlesEstablished;
}

void ImapClient::idling(const QMailFolderId &id)
{
    if (_waitingForIdle) {
        _waitingForIdleFolderIds.removeOne(id);
        if (_waitingForIdleFolderIds.isEmpty()) {
            _waitingForIdle = false;
            _idlesEstablished = true;
            _idleRetryDelay = InitialIdleRetryDelay;
            commandCompleted(IMAP_Idle_Continuation, OpOk);
        }
    }
}

QMailFolderIdList ImapClient::configurationIdleFolderIds()
{
    ImapConfiguration imapCfg(_config);            
    QMailFolderIdList folderIds;
    if (!imapCfg.pushEnabled())
        return folderIds;
    foreach(const QMailFolderId &idleFolderId, imapCfg.favouriteFolders()) {
        if (idleFolderId.isValid()) {
            folderIds.append(idleFolderId);
        }
    }
    return folderIds;
}

void ImapClient::monitor(const QMailFolderIdList &mailboxIds)
{
    static int count(0);
    
    ImapConfiguration imapCfg(_config);
    if (!_protocol.supportsCapability("IDLE")
        || !imapCfg.pushEnabled()) {
        return;
    }
    
    foreach(const QMailFolderId &id, _monitored.keys()) {
        if (!mailboxIds.contains(id)) {
            IdleProtocol *protocol = _monitored.take(id);
            protocol->close(); // Instead of closing could reuse below in some cases
            delete protocol;
        }
    }
    
    foreach(QMailFolderId id, mailboxIds) {
        if (!_monitored.contains(id)) {
            ++count;
            IdleProtocol *protocol = new IdleProtocol(this, QMailFolder(id), password);
            protocol->setObjectName(QString("I:%1").arg(count));
            _monitored.insert(id, protocol);
            connect(protocol, SIGNAL(idleNewMailNotification(QMailFolderId)),
                    this, SIGNAL(idleNewMailNotification(QMailFolderId)));
            connect(protocol, SIGNAL(idleFlagsChangedNotification(QMailFolderId)),
                    this, SIGNAL(idleFlagsChangedNotification(QMailFolderId)));
            connect(protocol, SIGNAL(openRequest()),
                    this, SLOT(idleOpenRequested()));
            protocol->open(imapCfg);
        }
    }
}

void ImapClient::idleOpenRequested()
{
    if (_protocol.inUse()) { // Setting up new idle connection may be in progress
        qMailLog(IMAP) << _protocol.objectName() 
                       << "IDLE: IMAP IDLE error recovery detected that the primary connection is "
                          "busy. Retrying to establish IDLE state in" 
                       << idleRetryDelay()/2 << "seconds.";
        return;
    }
    _protocol.close();
    foreach(const QMailFolderId &id, _monitored.keys()) {
        IdleProtocol *protocol = _monitored.take(id);
        if (protocol->inUse())
            protocol->close();
        delete protocol;
    }
    _idlesEstablished = false;
    qMailLog(IMAP) << _protocol.objectName() 
                   << "IDLE: IMAP IDLE error recovery trying to establish IDLE state now.";
    emit restartPushEmail();
}

void ImapClient::messageBufferFlushed()
{
    // We know this is now empty
    callbacks.clear();
}

void ImapClient::removeAllFromBuffer(QMailMessage *message)
{
    if (message) {
        QMap<QMailMessageId, QString>::const_iterator i = detachedTempFiles.find(message->id());
	while (i != detachedTempFiles.end() && i.key() == message->id()) {
            if (!(*i).isEmpty() && QFile::exists(*i)) {
                QFile::remove(*i);
            }
            ++i;
        }
        detachedTempFiles.remove(message->id());
    }

    int i = 0;
    while ((i = _bufferedMessages.indexOf(message, i)) != -1) {
        delete _bufferedMessages.at(i);
        _bufferedMessages.remove(i);
    }
}

void ImapClient::ssoResponse(const SignOn::SessionData &sessionData)
{
    if (_waitForSSO) {
        _waitForSSO = false;
        qMailLog(IMAP)  << "Got SSO response";
        password = sessionData.Secret();
        if (loginFailed)
        {
            loginFailed = false;
            commandTransition(IMAP_Init, OpPending);
            return;
        }
        if (_sendLogin)
            ssoSendLogin(sessionData, &_config, &_protocol);
    }
}

void ImapClient::ssoSessionError(const SignOn::Error &code)
{
    // code == SignOn::Error::IdentityOperationCanceled if
    // operation canceled by user
    loginFailed = false;
    if (_waitForSSO) {
        _waitForSSO = false;
        qMailLog(IMAP) <<  "Got SSO error:" << code.type() << code.message();
        operationFailed(QMailSearchAction::Status::ErrLoginFailed, QString("SSO error %1: %2").arg(code.type()).arg(code.message()));
    }
}

void ImapClient::onAccountsUpdated(const QMailAccountIdList &list)
{
    if (list.contains(_config.id())) {

        ImapConfiguration imapCfg1(_config);
        // copying here as the data is shared
        QMailAccountConfiguration config = QMailAccountConfiguration(_config.id());
        ImapConfiguration imapCfg2(config);

        if (!imapCfg1.isValid()) {
            qMailLog(IMAP) << Q_FUNC_INFO << "current config is invalid";
            return;
        }

        if (!imapCfg2.isValid()) {
            qMailLog(IMAP) << Q_FUNC_INFO << "invalid config from db";
            return;
        }
        
        qMailLog(IMAP) << Q_FUNC_INFO << imapCfg1.mailUserName() ;
        // compare config modified by the User
        const bool& notEqual = (imapCfg1.mailUserName() != imapCfg2.mailUserName()) ||
                               (imapCfg1.mailPassword() != imapCfg2.mailPassword()) ||
                               (imapCfg1.mailServer() != imapCfg2.mailServer()) ||
                               (imapCfg1.mailPort() != imapCfg2.mailPort()) ||
                               (imapCfg1.mailEncryption() != imapCfg2.mailEncryption()) ||
                               (imapCfg1.pushEnabled() != imapCfg2.pushEnabled());
        if (notEqual) {
            qMailLog(IMAP) << Q_FUNC_INFO << "Account was modified. Closing connections";

            closeConnection();
            // closing idle connections
            foreach(const QMailFolderId &id, _monitored.keys()) {
                IdleProtocol *protocol = _monitored.take(id);
                protocol->close();
                delete protocol;
            }
            _idlesEstablished = false;
        }

        if (imapCfg1.pushEnabled() != imapCfg2.pushEnabled()) {
            if (imapCfg2.pushEnabled())
                emit restartPushEmail();
        }
    }
}

#include "imapclient.moc"
