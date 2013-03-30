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

#ifndef _LOGGING_H_
#define _LOGGING_H_

#include <qmaillog.h>


/*!
 *\brief Crates default log configuration file
 *
 * Checks the key "Syslog/Enabled" and if not foud, creates a default settings file
 * with the following conent:
 *
 * [Syslog]
 * Enabled = 1
 * 
 * [FileLog]
 * Path = "/tmp/messageserver.log"
 * Enabled = 0
 *
 * [StdStreamLog]
 * StdErrEnabled = 0
 *
 * [Log categories]
 * IMAP = 0
 * Messaging = 1
 * POP = 0
 * SMTP = 0
 *
 * \param organization Organization (as per QSettings API)
 * \param application  Application (as per QSettings API)
*/
 
void createDefaultLogConfig(const QString& organization, const QString& application);

#endif
