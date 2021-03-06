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

#ifndef LOCKS_P_H
#define LOCKS_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt Extended API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

// We need slightly different semantics to those of QSystemMutex - all users of
// the qtopiamail library are peers, so no single caller is the owner.  We will
// allow the first library user to create the semaphore, and any subsequent users
// will attach to the same semaphore set.  No-one will close the semaphore set,
// we will rely on process undo to maintain sensible semaphore values as
// clients come and go...

#include <QString>
#include "qmailglobal.h"

class ProcessMutexPrivate;

class ProcessMutex
{
public:
    ProcessMutex(const QString &path, int id = 0);
    ~ProcessMutex();

    void lock();
    void unlock();

private:
    ProcessMutex(const ProcessMutex &);
    const ProcessMutex& operator=(const ProcessMutex &);

    ProcessMutexPrivate* d;
};

class ProcessReadLockPrivate;

class QMF_EXPORT ProcessReadLock
{
public:
    ProcessReadLock(const QString &path, int id = 0);
    ~ProcessReadLock();

    void lock();
    void unlock();

    void wait();

private:
    ProcessReadLock(const ProcessReadLock &);
    const ProcessReadLock& operator=(const ProcessReadLock &);

    ProcessReadLockPrivate* d;
};

#endif
