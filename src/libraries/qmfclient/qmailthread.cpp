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

#include "qmailthread_p.h"
#include "qmailstore.h"
#include "qmaillog.h"


/*!
    \class QMailThread

    \preliminary
    \brief The QMailThread class represents a thread of mail messages in the mail store.
    \ingroup messaginglibrary

    QMailThread represents a thread (also known as conversation) of mail messages.

    \sa QMailMessage, QMailStore::thread()
*/

/*!
  Constructor that creates an empty and invalid \c QMailThread.
  An empty thread is one which has no id or messages account.
  An invalid thread does not exist in the database and has an invalid id.
*/

QMailThread::QMailThread()
    : QPrivatelyImplemented<QMailThreadPrivate>(new QMailThreadPrivate())
{
}

/*!
  Constructor that creates a QMailThread by loading the data from the message store as
  specified by the QMailThreadId \a id. If the thread does not exist in the message store,
  then this constructor will create an empty and invalid QMailThread.
*/

QMailThread::QMailThread(const QMailThreadId& id)
  : QPrivatelyImplemented<QMailThreadPrivate>(NULL)
{
    *this = QMailStore::instance()->thread(id);
}

/*!
  Returns the \c ID of the \c QMailThread object. A \c QMailThread with an invalid ID
  is one which does not yet exist on the message store.
*/

QMailThreadId QMailThread::id() const
{
    return d->id;
}

/*!
  Sets the ID of this thread to \a id
*/

void QMailThread::setId(const QMailThreadId& id)
{
    d->id = id;
}

/*!
  Sets the parent account ID to \a id.
*/
void QMailThread::setParentAccountId(const QMailAccountId &id)
{
    d->parentAccountId = id;
}

/*!
  Gets the parent account for this thread
*/
QMailAccountId QMailThread::parentAccountId() const
{
    return d->parentAccountId;
}

/*!
  Gets the serverUid of the thread.
*/
QString QMailThread::serverUid() const
{
    return d->serverUid;
}

/*!
  Sets the serverUid of the thread to \a serverUid.
*/
void QMailThread::setServerUid(const QString& serverUid)
{
    d->serverUid = serverUid;
}

/*!
  Gets the unread count of the thread.
*/
uint QMailThread::unreadCount() const
{
    return d->unreadCount;
}

/*!
  Gets the count of the thread.
*/
uint QMailThread::messageCount() const
{
    return d->messageCount;
}

/*!
  Sets the count of the thread to \a value.
*/
void QMailThread::setMessageCount(uint value)
{
    d->messageCount = value;
}

/*!
  Sets the unread count of the thread to \a value.
*/
void QMailThread::setUnreadCount(uint value)
{
    d->unreadCount = value;
}


QString QMailThread::subject() const
{
    return d->subject;
}

void QMailThread::setSubject(const QString &value)
{
    d->subject = value;
}


QMailAddressList QMailThread::senders() const
{
    return QMailAddress::fromStringList(d->senders);
}

void QMailThread::setSenders(const QMailAddressList &value)
{
    d->senders.clear();
    d->senders = QMailAddress::toStringList(value).join(",");
}

void QMailThread::addSender(const QMailAddress &value)
{
    d->senders.prepend(value.address());
}

QString QMailThread::preview() const
{
    return d->preview;
}

void QMailThread::setPreview(const QString &value)
{
    d->preview = value;
}

QMailTimeStamp QMailThread::lastDate() const
{
    return d->lastDate;
}

void QMailThread::setLastDate(const QMailTimeStamp &value)
{
    d->lastDate = value;
}


QMailTimeStamp QMailThread::startedDate() const
{
    return d->startedDate;
}

void QMailThread::setStartedDate(const QMailTimeStamp &value)
{
    d->startedDate = value;
}


quint64 QMailThread::status() const
{
    return d->status;
}

void QMailThread::setStatus(quint64 value)
{
    d->status = value;
}
