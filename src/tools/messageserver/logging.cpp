/*******************************************************************
** This file is part of messageserver
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
**
** Contact: Vitaly Repin <vitaly.repin@nokia.com>
**
** This library is free software; you can redistribute it and/or
** modify it under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation
** and appearing in the file LICENSE.LGPL included in the packaging
** of this file.
*******************************************************************/

#include "logging.h"
#include <QStringList>
#include <QSettings>


void createDefaultLogConfig(const QString& organization, const QString& application)
{
    QSettings settings(organization, application);
    if(settings.allKeys().isEmpty())
    {
        settings.beginGroup("Syslog");
        settings.setValue("Enabled",0);
        settings.endGroup();

        settings.beginGroup("FileLog");
        settings.setValue("Path","/tmp/messageserver.log");
        settings.setValue("Enabled",0);
        settings.endGroup();

        settings.beginGroup("StdStreamLog");
        settings.setValue("Enabled",0);
        settings.endGroup();

        settings.beginGroup("Log categories");
        settings.setValue("IMAP",0);
        settings.setValue("Messaging",1);
        settings.setValue("POP",0);
        settings.setValue("SMTP",0);
        settings.endGroup();

        settings.sync();
    }
}

