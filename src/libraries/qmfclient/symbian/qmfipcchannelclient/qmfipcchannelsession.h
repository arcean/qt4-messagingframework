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

#ifndef QMFIPCCHANNELSESSION_H
#define QMFIPCCHANNELSESSION_H

#include <e32base.h>

// Constants
// Number of message slots to reserve for this client server session.
// Only one asynchronous request can be outstanding
// and one synchronous request in progress.
const TUint KDefaultMessageSlots = 2;

const TUid KServerUid3 = { 0x2003A67B }; // Server UID

_LIT(KQMFIPCChannelServerFilename, "QMFIPCChannelServer");

#ifdef __WINS__
static const TUint KServerMinHeapSize =  0x1000;  //  4K
static const TUint KServerMaxHeapSize = 0x10000;  // 64K
#endif

class RQMFIPCChannelSession : public RSessionBase
{
public:
    RQMFIPCChannelSession();

    TInt Connect();
    TVersion Version() const;

    TBool CreateChannel(const TDesC& aChannelName);
    TBool WaitForIncomingConnection(TPckgBuf<TUint>& aNewConnectionIdPckgBuf,
                                    TRequestStatus& aStatus);
    TBool DestroyChannel(const TDesC& aChannelName);
    TBool ChannelExists(const TDesC& aChannelName);
    TBool ConnectClientToChannel(const TDesC& aChannelName,
                                 TPckgBuf<TUint>& aNewConnectionIdPckgBuf,
                                 TRequestStatus& aStatus);
    TBool ConnectServerToChannel(TUint aConnectionId);
    TBool ListenChannel(TPckgBuf<TInt>& aIncomingMessageSizePckgBug,
                        TRequestStatus& aStatus);
    TBool DisconnectFromChannel();
    TBool SendData(const TDesC8& aData);
    TBool ReceiveData(RBuf8& aData);

    void Cancel();
};

#endif // QMFIPCCHANNELSESSION

// End of File
