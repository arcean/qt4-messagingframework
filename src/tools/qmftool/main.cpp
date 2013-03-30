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

#include <QCoreApplication>
#include <QDebug>
#include "qmftool.h"
#include "logging.h"

static const QString myOrganization("Nokia");
static const QString myApplication("Qmftool");

int main(int argc, char **argv)
{
    int ret = ERR_INVALID_OPTIONS;
    QCoreApplication app(argc, argv);
    // This is ~/.config/Nokia/Qmftool.conf
    createDefaultLogConfig(myOrganization, myApplication);
    qMailLoggersRecreate(myOrganization, myApplication, qPrintable(myApplication));
    QStringList args = QCoreApplication::arguments();

    if( args.size() <= 1 )
    {
        printUsageHelp();
    }
    else
    {
        QStringList options = collectArgs(args);
        ret = takeActions( options );
    } 

    switch( ret)
    {
        case NO_ERR:
            break;
        case ERR_NO_MAIL_ACCOUNTS:
            qMailLog(Messaging) << "error code : " << ret << " - no e-mail accounts configured";
            break;
        case ERR_INVALID_OPTIONS:
            qMailLog(Messaging) << "error code : " << ret << " - invalid options";
            break;
        default:
            qMailLog(Messaging) << "error code : " << ret;
    }

    return ret;
}
