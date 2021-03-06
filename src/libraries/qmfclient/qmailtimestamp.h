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

#ifndef QMAILTIMESTAMP_H
#define QMAILTIMESTAMP_H

#include "qmailglobal.h"

#include <QDateTime>
#include <QSharedDataPointer>
#include <QString>

class QMailTimeStampPrivate;

class QMF_EXPORT QMailTimeStamp
{
public:
    enum OutputFormat {
        Rfc2822,
        Rfc3501,
        Rfc3339
    };

    static QMailTimeStamp currentDateTime();

    QMailTimeStamp();
    explicit QMailTimeStamp(const QString& timeText);
    explicit QMailTimeStamp(const QDateTime& dateTime);
    QMailTimeStamp(const QMailTimeStamp& other);
    ~QMailTimeStamp();

    QString toString(OutputFormat format = Rfc2822) const;

    QDateTime toLocalTime() const;
    QDateTime toUTC() const;

    bool isNull() const;
    bool isValid() const;

    bool operator== (const QMailTimeStamp& other) const;
    bool operator!= (const QMailTimeStamp& other) const;

    bool operator< (const QMailTimeStamp& other) const;
    bool operator<= (const QMailTimeStamp& other) const;

    bool operator> (const QMailTimeStamp& other) const;
    bool operator>= (const QMailTimeStamp& other) const;

    const QMailTimeStamp& operator=(const QMailTimeStamp& other);

private:
    QSharedDataPointer<QMailTimeStampPrivate> d;
};

#endif
