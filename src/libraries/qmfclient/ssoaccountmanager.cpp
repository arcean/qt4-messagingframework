#include "ssoaccountmanager.h"

using namespace Accounts;

Manager* SSOAccountManager::_manager = 0;
int SSOAccountManager::_refCount = 0;

SSOAccountManager::SSOAccountManager()
{
    if (!_manager)
    {
        Q_ASSERT(!_refCount);
        _manager = new Manager("e-mail");
        _manager->setAbortOnTimeout(true);
    }

    ++_refCount;
}

SSOAccountManager::~SSOAccountManager()
{
    if (--_refCount == 0)
    {
        delete _manager;
        _manager = 0;
    }
}
