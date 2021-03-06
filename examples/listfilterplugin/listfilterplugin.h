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

#ifndef LISTFILTERPLUGIN_H
#define LISTFILTERPLUGIN_H

#include <qmailid.h>
#include <qmailcontentmanager.h>

class ListFilterPlugin : public QMailContentManagerPlugin
{
    Q_OBJECT
public:
    struct ListContentManager : public QMailContentManager
    {
        virtual QMailStore::ErrorCode add(QMailMessage *message, DurabilityRequirement);
        virtual QMailStore::ErrorCode update(QMailMessage *, DurabilityRequirement) { return QMailStore::NoError; }

        virtual QMailStore::ErrorCode ensureDurability() { return QMailStore::NoError; }

        virtual QMailStore::ErrorCode remove(const QString &) { return QMailStore::NoError; }
        virtual QMailStore::ErrorCode remove(const QList<QString> &) { return QMailStore::NoError; }
        virtual QMailStore::ErrorCode load(const QString &, QMailMessage *) { return QMailStore::NoError; }

        virtual bool init() { return true; }
        virtual void clearContent() { }
        virtual ManagerRole role() const { return FilterRole; }
    } manager;

    virtual QString key() const;
    virtual ListContentManager *create();
};

#endif
