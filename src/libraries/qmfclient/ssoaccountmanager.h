#ifndef SSOACCOUNTMANAGER_H
#define SSOACCOUNTMANAGER_H

#include <qglobal.h>
#include "qmailglobal.h"

// Accounts
#include <Accounts/Manager>

/// Accounts::Manager wrapper.
class QMF_EXPORT SSOAccountManager
{
public:
    SSOAccountManager();
    ~SSOAccountManager();

    Accounts::Manager* operator ->() const
    {
        Q_ASSERT (_manager);
        return _manager;
    }

    operator Accounts::Manager*() const
    {
        Q_ASSERT (_manager);
        return _manager;
    }

private:
    Q_DISABLE_COPY (SSOAccountManager);
    static Accounts::Manager* _manager;
    static int _refCount;
};

#endif // SSOACCOUNTMANAGER_H
