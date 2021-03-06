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

#include "imapconfiguration.h"
#include "qmailstore.h"
#include <QStringList>


ImapConfiguration::ImapConfiguration(const QMailAccountConfiguration &config)
    : QMailServiceConfiguration(config, "imap4")
{
}

ImapConfiguration::ImapConfiguration(const QMailAccountConfiguration::ServiceConfiguration &svcCfg)
    : QMailServiceConfiguration(svcCfg)
{
}

QString ImapConfiguration::mailUserName() const
{
    return value("username");
}

QString ImapConfiguration::mailPassword() const
{
    return decodeValue(value("password"));
}

QString ImapConfiguration::mailServer() const
{
    return value("server");
}

int ImapConfiguration::mailPort() const
{
    return value("port", "110").toInt();
}

int ImapConfiguration::mailEncryption() const
{
    return value("encryption", "0").toInt();
}

int ImapConfiguration::mailAuthentication() const
{
    return value("authentication", "0").toInt();
}

bool ImapConfiguration::canDeleteMail() const
{
    return (value("canDelete", "1").toInt() != 0);
}

bool ImapConfiguration::downloadAttachments() const
{
    return (value("downloadAttachments", "1").toInt() != 0);
}

bool ImapConfiguration::isAutoDownload() const
{
    return (value("autoDownload", "0").toInt() != 0);
}

int ImapConfiguration::maxMailSize() const
{
    return value("maxSize", "20").toInt();
}

QString ImapConfiguration::preferredTextSubtype() const
{
    return value("textSubtype");
}

bool ImapConfiguration::pushEnabled() const
{
    return (value("pushEnabled", "0").toInt() != 0);
}

QString ImapConfiguration::baseFolder() const
{
    return value("baseFolder");
}

QStringList ImapConfiguration::pushFolders() const
{
    return value("pushFolders").split(QChar('\x0A'), QString::SkipEmptyParts);
}

QMailFolderIdList ImapConfiguration::favouriteFolders() const
{
    static QMailFolderKey key(QMailFolderKey::status(QMailFolder::Favourite));
    return QMailStore::instance()->queryFolders(key & QMailFolderKey::parentAccountId(id()));
}

int ImapConfiguration::checkInterval() const
{
    return value("checkInterval", "-1").toInt();
}

bool ImapConfiguration::intervalCheckRoamingEnabled() const
{
    return (value("intervalCheckRoamingEnabled", "0").toInt() != 0);
}

QStringList ImapConfiguration::capabilities() const
{
    return value("capabilities").split(QChar(' '), QString::SkipEmptyParts);
}

void ImapConfiguration::setCapabilities(const QStringList &s)
{
    setValue("capabilities", QString("") + s.join(QChar(' '))); // can't setValue to null string
}

bool ImapConfiguration::pushCapable() const
{
    return (value("pushCapable", "0").toInt() != 0);
}

void ImapConfiguration::setPushCapable(bool b)
{
    setValue("pushCapable", QString::number(b ? 1 : 0));
}

// how many ms of nothing to do, until imap will log out and close the connection
// Note: starting new connections is very slow, and some operations are done in multiple
// parts. So using no (or very low) timeToLogout is not recommended
int ImapConfiguration::timeTillLogout() const
{
    const int logoutTime(1000);  //one second
    QString t(value("timeTillLogout", QString::number(logoutTime)));

    bool ok;
    int val(t.toInt(&ok));
    if (!ok) {
        qWarning() << "Could not parse timeTillLogout";
        return logoutTime;
    } else {
        return val;
    }
}

void ImapConfiguration::setTimeTillLogout(int milliseconds)
{
    Q_ASSERT(milliseconds >= 0);

    setValue("timeTillLogout", QString::number(milliseconds));
}

// For server based searching, when retrieving messages from server not yet on device, specifies
// the maximum number of messages to retrieve metadata for.
//
// 0 means unlimited.
//
// 0 is the default.
int ImapConfiguration::searchLimit() const
{
    const int defaultLimit = 100;
    QString t(value("searchLimit", QString::number(defaultLimit)));

    bool ok;
    int val(t.toInt(&ok));
    if (!ok) {
        qWarning() << "Could not parse searchLimit";
        return 0;
    } else {
        return val;
    }
}

void ImapConfiguration::setSearchLimit(int limit)
{
    Q_ASSERT(limit >= 0);

    setValue("searchLimit", QString::number(limit));
}

ImapConfigurationEditor::ImapConfigurationEditor(QMailAccountConfiguration *config)
    : ImapConfiguration(*config)
{
}

void ImapConfigurationEditor::setMailUserName(const QString &str)
{
    setValue("username", str);
}

void ImapConfigurationEditor::setMailPassword(const QString &str)
{
    setValue("password", encodeValue(str));
}

void ImapConfigurationEditor::setMailServer(const QString &str)
{
    setValue("server", str);
}

void ImapConfigurationEditor::setMailPort(int i)
{
    setValue("port", QString::number(i));
}

#ifndef QT_NO_OPENSSL

void ImapConfigurationEditor::setMailEncryption(int t)
{
    setValue("encryption", QString::number(t));
}

void ImapConfigurationEditor::setMailAuthentication(int t)
{
    setValue("authentication", QString::number(t));
}

#endif

void ImapConfigurationEditor::setDeleteMail(bool b)
{
    setValue("canDelete", QString::number(b ? 1 : 0));
}

void ImapConfigurationEditor::setAutoDownload(bool b)
{
    setValue("autoDownload", QString::number(b ? 1 : 0));
}

void ImapConfigurationEditor::setMaxMailSize(int i)
{
    setValue("maxSize", QString::number(i));
}

void ImapConfigurationEditor::setPreferredTextSubtype(const QString &str)
{
    setValue("textSubtype", str);
}

void ImapConfigurationEditor::setPushEnabled(bool b)
{
    setValue("pushEnabled", QString::number(b ? 1 : 0));
}

void ImapConfigurationEditor::setBaseFolder(const QString &s)
{
    setValue("baseFolder", s);
}

void ImapConfigurationEditor::setPushFolders(const QStringList &s)
{
    setValue("pushFolders", QString("") + s.join(QChar('\x0A'))); // can't setValue to null string
}

void ImapConfigurationEditor::setCheckInterval(int i)
{
    setValue("checkInterval", QString::number(i));
}

void ImapConfigurationEditor::setIntervalCheckRoamingEnabled(bool b)
{
    setValue("intervalCheckRoamingEnabled", QString::number(b ? 1 : 0));
}
