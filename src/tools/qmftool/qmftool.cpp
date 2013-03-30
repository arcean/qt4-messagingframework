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

#include <qmailaccount.h>
#include <qmailstore.h>
#include <QDebug>
#include <qmaillog.h>
#include "qmftool.h"

void printUsageHelp()
{
    std::cout << "qmftool : a command line utility" << std::endl;
    std::cout << "Usage : qmftool (--command | -shorthand) [command options]" << std::endl;
    std::cout << "qmftool (--query-accounts | -q) [count] [sink sink_name] [source source_name]: print no. of email accounts" << std::endl;
    std::cout << "qmftool --upgrade-database : a command to upgrade database tables" << std::endl;
    std::cout << "qmftool --help | -h : print this help" << std::endl << std::endl;
    std::cout << "examples: " << std::endl;
    std::cout << "qmftool -q : print total number of defined e-mail accounts " << std::endl; 
    std::cout << "qmftool -q source smtp : print total number of SMTP accounts" << std::endl;
    std::cout << "qmftool -q source imap sink pop3 : print total number of accounts having imap as source and pop3 as sink" << std::endl;
    std::cout << "qmftool error codes :" << std::endl;
    std::cout << "0 : Success" << std::endl;
    std::cout << "254 : No email accounts configured" << std::endl; 
    std::cout << "255 : Invalid options passed to qmftool" << std::endl << std::endl;
    std::cout << "By default, the output goes to the syslog " << std::endl;
    std::cout << "See sections [syslog] and [StdStreamLog] in  ~/.config/Nokia/Qmftool.conf" << std::endl;

}

QStringList collectArgs(const QStringList& args)
{
    QStringList options;
    QString option;

    if( args.isEmpty() || !args[1].startsWith("-") )
    {
        printUsageHelp();
        return QStringList();
    }

    QStringList::const_iterator argsItr = args.constBegin() + 1;
    while( argsItr != args.constEnd() )
    {
        option = *argsItr;
        while( ++argsItr != args.constEnd() && !(*argsItr).startsWith("-") )
        {
            option += " ";
            option += *argsItr;
        }
        options << option;
    }

    return options;
}

int takeActions(const QStringList& options)
{
    int ret = ERR_INVALID_OPTIONS, size = 0;
    QStringList::const_iterator optionsItr, argsItr;
    QStringList optionArgs;
    QMailAccountIdList accountsList;
    bool checkSink = false, checkSource = false;
    QString sinkName, sourceName;
    enum State { UNKNOWN, SINK, SOURCE, COUNT };
    State state = COUNT;

  
    for( optionsItr = options.constBegin(); optionsItr != options.constEnd(); ++optionsItr )
    {
        if ((*optionsItr).startsWith("--upgrade-database"))
        {
            QMailStore::instance();
            ret = NO_ERR;
        } else if( (*optionsItr).startsWith("--query-accounts") || (*optionsItr).startsWith("-q") ) {
            optionArgs = (*optionsItr).split(" ");
            if( optionArgs.size() > 1 ) 
            { // parse option arguments
                for( argsItr = optionArgs.constBegin() + 1; argsItr != optionArgs.constEnd(); ++argsItr )
                {
                    switch (state)
                    {
                    case SINK:
                        sinkName = *argsItr;
                        checkSink = true;
                        state = COUNT;
                        break;
                    case SOURCE:
                        sourceName = *argsItr;
                        checkSource = true;
                        state = COUNT;
                        break;
                    case COUNT:
                        if(*argsItr == "sink" )
                            state = SINK;
                        else if(*argsItr == "source" )
                            state = SOURCE;
                        else if (*argsItr == "count" )
                            state = COUNT;
                        else
                            state = UNKNOWN;
                        break;
                    default:
                        break;
                    }
                }  // for
            } // parse option arguments

            if(state == UNKNOWN )
            { // parse error
                ret = ERR_INVALID_OPTIONS;
                printUsageHelp();
                break;
            }

            // ok, now count the accounts number
            accountsList = QMailStore::instance()->queryAccounts();
            if( checkSink || checkSource )
            {
                bool foundSink, foundSource;
                foreach(QMailAccountId id, accountsList)
                {
                    QMailAccount account(id);
                    if(checkSink)
                        foundSink = (account.messageSinks().contains(sinkName));
                    if(checkSource)
                        foundSource = (account.messageSources().contains(sourceName));

                    // if both sink and source are required, so both must be found
                    if(checkSink && checkSource)
                        size += (foundSink && foundSource);
                    else
                        size += (foundSink || foundSource); 
                }
            }
            else // just return size
                size = accountsList.size();

            qMailLog(Messaging) << "No. of configured email accounts : " << size;
            ret = size ? NO_ERR : ERR_NO_MAIL_ACCOUNTS;
        }
        else
        { 
            ret = NO_ERR;
            printUsageHelp();
            break;
        }
    }

    return ret;
}
