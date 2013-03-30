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

#ifndef SMTPCLIENT_H
#define SMTPCLIENT_H

#include <qstring.h>
#include <qobject.h>
#include <qstringlist.h>
#include <qlist.h>
#include <qmailaccountconfiguration.h>
#include <qmailmessage.h>
#include <qmailmessageserver.h>
#include <qmailtransport.h>

// SSO
#include <SignOn/Identity>
#include <SignOn/SessionData>

class QTemporaryFile;

struct RawEmail
{
    QString from;
    QStringList to;
    QMailMessage mail;
};

using namespace SignOn;

class SmtpClient : public QObject
{
    Q_OBJECT

public:
    SmtpClient(QObject* parent);
    ~SmtpClient();

    QMailMessage::MessageType messageType() const;

    void setAccount(const QMailAccountId &accountId);
    QMailAccountId account() const;

    void removeSsoIdentity(const QMailAccountId &accountId);

    void newConnection();
    void cancelTransfer(QMailServiceAction::Status::ErrorCode code, const QString &text);

    QMailServiceAction::Status::ErrorCode addMail(const QMailMessage& mail);

signals:
    void errorOccurred(int, const QString &);
    void errorOccurred(const QMailServiceAction::Status &, const QString &);
    void updateStatus(const QString &);

    void progressChanged(uint, uint);
    void messageTransmitted(const QMailMessageId&);
    void sendCompleted();

protected slots:
    void connected(QMailTransport::EncryptType encryptType);
    void transportError(int, QString msg);
    void readyRead();
    void sent(qint64);
    void accountsUpdated(const QMailAccountIdList&);

private slots:
    void sendMoreData(qint64);
    void authExpired();
    void ssoResponse(const SignOn::SessionData &sessionData);
    void ssoSessionError(const SignOn::Error &code);

private:
    void deleteSsoIdentity();
    void sendCommand(const char *data, int len = -1);
    void sendCommand(const QString &cmd);
    void sendCommand(const QByteArray &cmd);
    void sendCommands(const QStringList &cmds);
    void incomingData();
    void nextAction(const QString &response);
    void messageProcessed(const QMailMessageId &id);

    void operationFailed(int code, const QString &text);
    void operationFailed(QMailServiceAction::Status::ErrorCode code, const QString &text);
    void stopTransferring();

private:
    enum TransferStatus
    {
        Init, Helo, Extension, StartTLS, TLS, Connected, SignOnSession,  Authenticating, Authenticated,
        MetaData, From, Recv, MRcv, PrepareData, Data, Body, Chunk, ChunkSent, Sent, Quit, Done
    };

    QMailAccountConfiguration config;
    TransferStatus status;
    QList<RawEmail> mailList;
    QList<RawEmail>::Iterator mailItr;
    QList<QMailMessage::MessageChunk> mailChunks;
    QMailMessageId sendingId;
    uint messageLength;
    uint sentLength;
    bool sending, success;
    int outstandingResponses;
    QStringList::Iterator it;
    QMailTransport *transport;

    // SendMap maps id -> (units) to be sent
    typedef QMap<QMailMessageId, uint> SendMap;
    SendMap sendSize;
    uint progressSendSize;
    uint totalSendSize;

    QStringList capabilities;
    quint32 addressComponent;
    QByteArray domainName;

    QTemporaryFile *temporaryFile;
    qint64 waitingForBytes;
    bool linestart;

    QString bufferedResponse;
    bool notUsingAuth;

    QTimer *authTimeout;

    Identity* identity;
    AuthSession *session;
    bool handleSSOerrors;
    bool loginFailed;
    bool WaitForSso;
    QString password;
};

#endif
