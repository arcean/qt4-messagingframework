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

#ifndef QMAILACCOUNTCONFIG_H
#define QMAILACCOUNTCONFIG_H

#include "qmailid.h"
#include "qmailglobal.h"
#include <QMap>
#include <QObject>
#include <QSharedData>
#include <QString>

class QMailAccountConfigurationPrivate;

class QMF_EXPORT QMailAccountConfiguration
{
    class ConfigurationValues;
    class ServiceConfigurationPrivate;

public:
    class QMF_EXPORT ServiceConfiguration
    {
    public:
        ServiceConfiguration();
        ServiceConfiguration(const ServiceConfiguration &other);

        ~ServiceConfiguration();

        QString service() const;
        QMailAccountId id() const;

        QString value(const QString &name) const;
        void setValue(const QString &name, const QString &value);

        void removeValue(const QString &name);

        const QMap<QString, QString> &values() const;

        const ServiceConfiguration &operator=(const ServiceConfiguration &other);

    private:
        friend class QMailAccountConfiguration;
        friend class QMailAccountConfigurationPrivate;

        ServiceConfiguration(QMailAccountConfigurationPrivate *, const QString *, ConfigurationValues *);

        ServiceConfigurationPrivate *d;
    };

    QMailAccountConfiguration();
    explicit QMailAccountConfiguration(const QMailAccountId& id);
    QMailAccountConfiguration(const QMailAccountConfiguration &other);

    ~QMailAccountConfiguration();

    const QMailAccountConfiguration &operator=(const QMailAccountConfiguration &other);

    void setId(const QMailAccountId &id);
    QMailAccountId id() const;

    ServiceConfiguration &serviceConfiguration(const QString &service);
    const ServiceConfiguration &serviceConfiguration(const QString &service) const;

    bool addServiceConfiguration(const QString &service);
    bool removeServiceConfiguration(const QString &service);

    QStringList services() const;

private:
    friend class QMailAccountConfigurationPrivate;
    friend class QMailStorePrivate;

    bool modified() const;
    void setModified(bool set);

    QSharedDataPointer<QMailAccountConfigurationPrivate> d;
};

#endif
