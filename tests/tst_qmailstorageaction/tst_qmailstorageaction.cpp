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

#include <QObject>
#include <QTest>
#include <qmailserviceaction.h>
#include <qmailserviceaction_p.h>
#include <qmailaccount.h>
#include <qmaildisconnected.h>
#include <qmailnamespace.h>

static bool isMessageServerRunning()
{
    QString lockfile = "messageserver-instance.lock";
    int lockid = QMail::fileLock(lockfile);
    if (lockid == -1)
        return true;

    QMail::fileUnlock(lockid);
    return false;
}

class tst_QMailStorageAction : public QObject
{
    Q_OBJECT

public:
    tst_QMailStorageAction() {}
    virtual ~tst_QMailStorageAction() {}

private slots:
    void initTestCase();
    void cleanupTestCase();

    void test_storageaction_add();
    void test_storageaction_update_message();
    void test_storageaction_update_messagemetadata();
    void test_storageaction_movetostandardfolder();
    void test_storageaction_restoretopreviousfolder();
    void test_storageaction_movetofolder();
    void test_storageaction_flagMessages();
    void test_storageaction_rollBackUpdates();
    void test_storageaction_deleteMessages();
    void test_storageaction_discardMessages();

private:
    QMailAccountId accountId1, accountId2, accountId3, accountId4;
    QMailFolderId inboxId1, inboxId2, inboxId3, trashId1, trashId2, savedId1, savedId2, archivedId1, archivedId2;
    QMailMessageId smsMessage, inboxMessage1, archivedMessage1, inboxMessage2, savedMessage2;

    QSet<QMailAccountId> noAccounts, verifiedAccounts, unverifiedAccounts, allAccounts;
    QSet<QMailFolderId> noFolders, allFolders, standardFolders;
    QSet<QMailMessageId> noMessages, allMessages, allEmailMessages;
};

QTEST_MAIN(tst_QMailStorageAction)

#include "tst_qmailstorageaction.moc"

void tst_QMailStorageAction::initTestCase()
{
    if (!isMessageServerRunning()) {
        qWarning() << "tst_QMailStorageAction requires messageserver to be running";
        QVERIFY(isMessageServerRunning());
        exit(1);
    }
    
    // Instantiate the store to initialise the values of the status flags and create the standard folders
    QMailStore::instance();

    // Create the data set we will test our keys upon
    standardFolders << QMailFolderId(QMailFolder::LocalStorageFolderId);

    {
        QMailAccount account;
        account.setName("Account 1");
        account.setMessageType(QMailMessage::Email);
        account.setFromAddress(QMailAddress("Account 1", "account1@example.org"));
        account.setStatus(QMailAccount::SynchronizationEnabled, false);
        account.setStatus(QMailAccount::Synchronized, false);
        account.setStatus(QMailAccount::MessageSource, true);
        account.setStatus(QMailAccount::CanRetrieve, true);
        account.setStatus(QMailAccount::MessageSink, true);
        account.setStatus(QMailAccount::CanTransmit, true);
        account.setCustomField("verified", "true");
        account.setCustomField("question", "What is your dog's name?");
        account.setCustomField("answer", "Fido");

        QMailAccountConfiguration config;
        config.addServiceConfiguration("imap4");
        if (QMailAccountConfiguration::ServiceConfiguration *svcCfg = &config.serviceConfiguration("imap4")) {
            svcCfg->setValue("server", "mail.example.org");
            svcCfg->setValue("username", "account1");
        }
        config.addServiceConfiguration("smtp");
        if (QMailAccountConfiguration::ServiceConfiguration *svcCfg = &config.serviceConfiguration("smtp")) {
            svcCfg->setValue("server", "mail.example.org");
            svcCfg->setValue("username", "account1");
        }

        QVERIFY(QMailStore::instance()->addAccount(&account, &config));
        accountId1 = account.id();
        allAccounts << account.id();
        verifiedAccounts << account.id();
    }

    {
        QMailAccount account;
        account.setName("Account 2");
        account.setMessageType(QMailMessage::Instant);
        account.setFromAddress(QMailAddress("Account 2", "account2@example.org"));
        account.setStatus(QMailAccount::SynchronizationEnabled, true);
        account.setStatus(QMailAccount::Synchronized, false);
        account.setStatus(QMailAccount::MessageSource, true);
        account.setStatus(QMailAccount::CanRetrieve, true);
        account.setCustomField("verified", "true");
        account.setCustomField("question", "What is your dog's name?");
        account.setCustomField("answer", "Lassie");

        QMailAccountConfiguration config;
        config.addServiceConfiguration("imap4");
        if (QMailAccountConfiguration::ServiceConfiguration *svcCfg = &config.serviceConfiguration("imap4")) {
            svcCfg->setValue("server", "imap.example.org");
            svcCfg->setValue("username", "account2");
        }

        QVERIFY(QMailStore::instance()->addAccount(&account, &config));
        accountId2 = account.id();
        allAccounts << account.id();
        verifiedAccounts << account.id();
    }

    {
        QMailAccount account;
        account.setName("Account 3");
        account.setMessageType(QMailMessage::None);
        account.setFromAddress(QMailAddress("Account 3", "account3@test"));
        account.setCustomField("verified", "false");

        QVERIFY(QMailStore::instance()->addAccount(&account, 0));
        accountId3 = account.id();
        allAccounts << account.id();
        unverifiedAccounts << account.id();
    }

    {
        QMailAccount account;
        account.setName("Account 4");
        account.setMessageType(QMailMessage::None);
        account.setFromAddress(QMailAddress("Account 4", "account4@test"));
        account.setCustomField("verified", "false");

        QVERIFY(QMailStore::instance()->addAccount(&account, 0));
        accountId4 = account.id();
        allAccounts << account.id();
        unverifiedAccounts << account.id();
    }

    QVERIFY(!QMailDisconnected::updatesOutstanding(accountId1));
    QVERIFY(!QMailDisconnected::updatesOutstanding(accountId2));
    QVERIFY(!QMailDisconnected::updatesOutstanding(accountId3));
    QVERIFY(!QMailDisconnected::updatesOutstanding(accountId4));

    {
        QMailFolder folder;
        folder.setPath("Inbox");
        folder.setDisplayName("Inbox");
        folder.setParentFolderId(QMailFolderId());
        folder.setParentAccountId(accountId1);
        folder.setStatus(QMailFolder::SynchronizationEnabled, true);
        folder.setStatus(QMailFolder::Synchronized, false);
        folder.setCustomField("uidValidity", "abcdefg");
        folder.setCustomField("uidNext", "1");

        QVERIFY(QMailStore::instance()->addFolder(&folder));
        inboxId1 = folder.id();
        allFolders << folder.id();
    }

    {
        QMailFolder folder;
        folder.setPath("Inbox/Saved");
        folder.setDisplayName("Saved");
        folder.setParentFolderId(inboxId1);
        folder.setParentAccountId(accountId1);
        folder.setStatus(QMailFolder::SynchronizationEnabled, true);
        folder.setStatus(QMailFolder::Synchronized, true);
        folder.setCustomField("uidValidity", "hijklmnop");
        folder.setCustomField("uidNext", "11");

        QVERIFY(QMailStore::instance()->addFolder(&folder));
        savedId1 = folder.id();
        allFolders << folder.id();
    }

    {
        QMailFolder folder;
        folder.setPath("Inbox/Saved/Archived");
        folder.setDisplayName("Archived");
        folder.setParentFolderId(savedId1);
        folder.setParentAccountId(accountId1);
        folder.setStatus(QMailFolder::SynchronizationEnabled, false);
        folder.setStatus(QMailFolder::Synchronized, false);
        folder.setCustomField("archived", "true");
        folder.setCustomField("uidNext", "111");

        QVERIFY(QMailStore::instance()->addFolder(&folder));
        archivedId1 = folder.id();
        allFolders << folder.id();
    }

    {
        QMailFolder folder;
        folder.setPath("Inbox");
        folder.setDisplayName("Inbox");
        folder.setParentFolderId(QMailFolderId());
        folder.setParentAccountId(accountId2);
        folder.setStatus(QMailFolder::SynchronizationEnabled, false);
        folder.setStatus(QMailFolder::Synchronized, false);
        folder.setCustomField("uidValidity", "qrstuv");
        folder.setCustomField("uidNext", "1");

        QVERIFY(QMailStore::instance()->addFolder(&folder));
        inboxId2 = folder.id();
        allFolders << folder.id();
    }

    {
        QMailFolder folder;
        folder.setPath("Trash");
        folder.setDisplayName("Trash");
        folder.setParentFolderId(QMailFolderId());
        folder.setParentAccountId(accountId2);
        folder.setStatus(QMailFolder::SynchronizationEnabled, false);
        folder.setStatus(QMailFolder::Synchronized, false);
        folder.setStatus(QMailFolder::TrashFolder, true);
        folder.setCustomField("uidValidity", "qrstux");
        folder.setCustomField("uidNext", "1");

        QVERIFY(QMailStore::instance()->addFolder(&folder));
        trashId1 = folder.id();
        allFolders << folder.id();
        
        QMailAccount account(accountId2);
        account.setStandardFolder(QMailFolder::TrashFolder, trashId1);
        QVERIFY(QMailStore::instance()->updateAccount(&account));
    }

    {
        QMailFolder folder;
        folder.setPath("Inbox/Saved");
        folder.setDisplayName("Saved");
        folder.setParentFolderId(inboxId2);
        folder.setParentAccountId(accountId2);
        folder.setStatus(QMailFolder::SynchronizationEnabled, false);
        folder.setStatus(QMailFolder::Synchronized, true);
        folder.setCustomField("uidValidity", "wxyz");
        folder.setCustomField("uidNext", "11");

        QVERIFY(QMailStore::instance()->addFolder(&folder));
        savedId2 = folder.id();
        allFolders << folder.id();
    }

    {
        QMailFolder folder;
        folder.setPath("Inbox/Saved/Archived");
        folder.setDisplayName("Archived");
        folder.setParentFolderId(savedId2);
        folder.setParentAccountId(accountId2);
        folder.setStatus(QMailFolder::SynchronizationEnabled, false);
        folder.setStatus(QMailFolder::Synchronized, false);
        folder.setCustomField("archived", "true");
        folder.setCustomField("uidNext", "111");

        QVERIFY(QMailStore::instance()->addFolder(&folder));
        archivedId2 = folder.id();
        allFolders << folder.id();
    }

    {
        QMailFolder folder;
        folder.setPath("Inbox");
        folder.setDisplayName("Inbox");
        folder.setParentFolderId(QMailFolderId());
        folder.setParentAccountId(accountId3);
        folder.setStatus(QMailFolder::SynchronizationEnabled, false);
        folder.setStatus(QMailFolder::Synchronized, false);
        folder.setCustomField("uidValidity", "qrstua");
        folder.setCustomField("uidNext", "1");

        QVERIFY(QMailStore::instance()->addFolder(&folder));
        inboxId3 = folder.id();
        allFolders << folder.id();
    }

    {
        QMailFolder folder;
        folder.setPath("Trash");
        folder.setDisplayName("Trash");
        folder.setParentFolderId(QMailFolderId());
        folder.setParentAccountId(accountId3);
        folder.setStatus(QMailFolder::SynchronizationEnabled, false);
        folder.setStatus(QMailFolder::Synchronized, false);
        folder.setStatus(QMailFolder::TrashFolder, true);
        folder.setCustomField("uidValidity", "qrstub");
        folder.setCustomField("uidNext", "1");

        QVERIFY(QMailStore::instance()->addFolder(&folder));
        trashId2 = folder.id();
        allFolders << folder.id();
        
        QMailAccount account(accountId3);
        account.setStandardFolder(QMailFolder::TrashFolder, trashId2);
        QVERIFY(QMailStore::instance()->updateAccount(&account));
    }

    {
        QMailMessage message;
        message.setMessageType(QMailMessage::Sms);
        message.setParentAccountId(accountId4);
        message.setParentFolderId(QMailFolder::LocalStorageFolderId);
        message.setFrom(QMailAddress("0404404040"));
        message.setTo(QMailAddress("0404040404"));
        message.setSubject("Where are you?");
        message.setDate(QMailTimeStamp(QDateTime(QDate::currentDate())));
        message.setReceivedDate(QMailTimeStamp(QDateTime(QDate::currentDate())));
        message.setStatus(QMailMessage::Incoming, true);
        message.setStatus(QMailMessage::New, false);
        message.setStatus(QMailMessage::Read, true);
        message.setServerUid("sim:12345");
        message.setSize(1 * 1024);
        message.setContent(QMailMessage::PlainTextContent);
        message.setCustomField("present", "true");
        message.setCustomField("todo", "true");

        QVERIFY(QMailStore::instance()->addMessage(&message));
        smsMessage = message.id();
        allMessages << message.id();
    }

    {
        QMailMessage message;
        message.setMessageType(QMailMessage::Email);
        message.setParentAccountId(accountId1);
        message.setParentFolderId(inboxId1);
        message.setFrom(QMailAddress("account2@example.org"));
        message.setTo(QMailAddress("account1@example.org"));
        message.setSubject("inboxMessage1");
        message.setDate(QMailTimeStamp(QDateTime(QDate::currentDate())));
        message.setReceivedDate(QMailTimeStamp(QDateTime(QDate::currentDate())));
        message.setStatus(QMailMessage::Incoming, true);
        message.setStatus(QMailMessage::New, true);
        message.setStatus(QMailMessage::Read, false);
        message.setServerUid("inboxMessage1");
        message.setSize(5 * 1024);
        message.setContent(QMailMessage::PlainTextContent);
        message.setCustomField("present", "true");

        QVERIFY(QMailStore::instance()->addMessage(&message));
        inboxMessage1 = message.id();
        allMessages << message.id();
        allEmailMessages << message.id();
    }

    {
        QMailMessage message;
        message.setMessageType(QMailMessage::Email);
        message.setParentAccountId(accountId1);
        message.setParentFolderId(inboxId1);
        message.setFrom(QMailAddress("account1@example.org"));
        message.setTo(QMailAddress("fred@example.net"));
        message.setSubject("archivedMessage1");
        message.setDate(QMailTimeStamp(QDateTime(QDate::currentDate().addDays(-1))));
        message.setReceivedDate(QMailTimeStamp(QDateTime(QDate::currentDate().addDays(-1))));
        message.setStatus(QMailMessage::Outgoing, true);
        message.setStatus(QMailMessage::New, false);
        message.setStatus(QMailMessage::Sent, true);
        message.setServerUid("archivedMessage1");
        message.setSize(15 * 1024);
        message.setContent(QMailMessage::VideoContent);
        message.setCustomField("present", "true");

        QVERIFY(QMailStore::instance()->addMessage(&message));

        message.setPreviousParentFolderId(message.parentFolderId());
        message.setParentFolderId(archivedId1);
        QVERIFY(QMailStore::instance()->updateMessage(&message));

        archivedMessage1 = message.id();
        allMessages << message.id();
        allEmailMessages << message.id();
    }

    {
        QMailMessage message;
        message.setMessageType(QMailMessage::Email);
        message.setParentAccountId(accountId2);
        message.setParentFolderId(inboxId2);
        message.setFrom(QMailAddress("account1@example.org"));
        message.setTo(QMailAddress("account2@example.org"));
        message.setSubject("Fwd:inboxMessage2");
        message.setDate(QMailTimeStamp(QDateTime(QDate::currentDate())));
        message.setReceivedDate(QMailTimeStamp(QDateTime(QDate::currentDate())));
        message.setStatus(QMailMessage::Incoming, true);
        message.setStatus(QMailMessage::New, true);
        message.setStatus(QMailMessage::Read, true);
        message.setServerUid("inboxMessage2");
        message.setSize(5 * 1024);
        message.setContent(QMailMessage::HtmlContent);
        message.setInResponseTo(inboxMessage1);
        message.setResponseType(QMailMessage::Forward);
        message.setCustomField("present", "true");
        message.setCustomField("todo", "false");

        QVERIFY(QMailStore::instance()->addMessage(&message));
        inboxMessage2 = message.id();
        allMessages << message.id();
        allEmailMessages << message.id();
    }

    {
        QMailMessage message;
        message.setMessageType(QMailMessage::Email);
        message.setParentAccountId(accountId2);
        message.setParentFolderId(inboxId2);
        message.setFrom(QMailAddress("fred@example.net"));
        message.setTo(QMailAddressList() << QMailAddress("account2@example.org") << QMailAddress("testing@test"));
        message.setSubject("Re:savedMessage2");
        message.setDate(QMailTimeStamp(QDateTime(QDate::currentDate().addDays(-7))));
        message.setReceivedDate(QMailTimeStamp(QDateTime(QDate::currentDate().addDays(-7))));
        message.setStatus(QMailMessage::Incoming, true);
        message.setStatus(QMailMessage::New, false);
        message.setStatus(QMailMessage::Read, false);
        message.setServerUid("savedMessage2");
        message.setSize(5 * 1024);
        message.setContent(QMailMessage::HtmlContent);
        message.setInResponseTo(archivedMessage1);
        message.setResponseType(QMailMessage::Reply);
        message.setCustomField("present", "true");

        QVERIFY(QMailStore::instance()->addMessage(&message));

        message.setPreviousParentFolderId(message.parentFolderId());
        message.setParentFolderId(savedId2);
        QVERIFY(QMailStore::instance()->updateMessage(&message));

        savedMessage2 = message.id();
        allMessages << message.id();
        allEmailMessages << message.id();
    }
}


void tst_QMailStorageAction::cleanupTestCase()
{
    QMailStore::instance()->removeAccounts(QMailAccountKey::customField("verified"));
    QMailStore::instance()->removeMessages(QMailMessageKey::customField("present"));
    QMailStore::instance()->removeFolders(QMailFolderKey::customField("uidNext"));
}

void tst_QMailStorageAction::test_storageaction_add()
{
    QMailStorageAction action;
    QMailMessageId saved3id;
    QMailMessage message;
    QMailMessageList messages;
    
    message.setMessageType(QMailMessage::Email);
    message.setParentAccountId(accountId2);
    message.setParentFolderId(inboxId2);
    message.setFrom(QMailAddress("wilma@example.net"));
    message.setTo(QMailAddressList() << QMailAddress("account2@example.org") << QMailAddress("testing@test"));
    message.setSubject("Simple test message");
    message.setDate(QMailTimeStamp(QDateTime(QDate::currentDate().addDays(-100))));
    message.setReceivedDate(QMailTimeStamp(QDateTime(QDate::currentDate().addDays(-100))));
    message.setStatus(QMailMessage::Incoming, true);
    message.setStatus(QMailMessage::New, false);
    message.setStatus(QMailMessage::Read, false);
    message.setServerUid("savedMessage3");
    message.setSize(5 * 1024);
    message.setContent(QMailMessage::HtmlContent);
    message.setCustomField("present", "true");

    messages << message;
    action.addMessages(messages);
        
    int i = 0;
    while (action.isRunning() && i++ < 1000)
        QTest::qWait(10);
    QTest::qWait(0);

    QVERIFY(action.status().errorCode == QMailServiceAction::Status::ErrNoError);
    QVERIFY(action.activity() == QMailServiceAction::Successful);
    
    QMailMessageKey savedMessage3Key(QMailMessageKey::serverUid("savedMessage3"));
    QVERIFY(QMailStore::instance()->countMessages(savedMessage3Key) == 1);
    message = QMailStore::instance()->message("savedMessage3", accountId2);
    saved3id = message.id();
    QVERIFY(saved3id.isValid());
}

void tst_QMailStorageAction::test_storageaction_update_message()
{
    QMailStorageAction action;
    QMailMessageId saved3id;
    QMailMessageList messages;
    QMailMessage message;
    
    message = QMailStore::instance()->message("savedMessage3", accountId2);
    saved3id = message.id();
    QVERIFY(saved3id.isValid());
        
    QString subject("Updated simple test message");
    message.setSubject(subject);
    messages << message;
    action.updateMessages(messages);
        
    int i = 0;
    while (action.isRunning() && i++ < 1000)
        QTest::qWait(10);
    QTest::qWait(0);

    QVERIFY(action.status().errorCode == QMailServiceAction::Status::ErrNoError);
    QVERIFY(action.activity() == QMailServiceAction::Successful);
    
    QMailMessage saved3(saved3id);
    QVERIFY(saved3.subject() == subject);
}

void tst_QMailStorageAction::test_storageaction_update_messagemetadata()
{
    QMailStorageAction action;
    QMailMessageId saved3id;
    QMailMessageMetaDataList metadatalist;
    QMailMessageMetaData metadata;

    metadata = QMailStore::instance()->message("savedMessage3", accountId2);
    saved3id = metadata.id();
    QVERIFY((metadata.status() & QMailMessage::Read) == false);
    QVERIFY(saved3id.isValid());

    QString subject("Updated again simple test message");
    metadata.setSubject(subject);
    metadata.setStatus(QMailMessage::Read, true);
    metadatalist << metadata;
    action.updateMessages(metadatalist);

    int i = 0;
    while (action.isRunning() && i++ < 1000)
        QTest::qWait(10);
    QTest::qWait(0);

    QVERIFY(action.status().errorCode == QMailServiceAction::Status::ErrNoError);
    QVERIFY(action.activity() == QMailServiceAction::Successful);
    
    QMailMessage saved3(saved3id);
    QVERIFY(saved3.subject() == subject);
    QVERIFY(saved3.status() & QMailMessage::Read);
}

void tst_QMailStorageAction::test_storageaction_movetostandardfolder()
{
    QMailStorageAction action;
    QMailMessageIdList list;

    list = QMailStore::instance()->queryMessages(QMailMessageKey::serverUid("savedMessage3"));
    QVERIFY(list.count() == 1);
    int oldTrashCount = QMailStore::instance()->countMessages(QMailMessageKey::parentFolderId(trashId1));
    int oldInboxCount = QMailStore::instance()->countMessages(QMailMessageKey::parentFolderId(inboxId2));

    action.moveToStandardFolder(list, QMailFolder::TrashFolder);

    int i = 0;
    while (action.isRunning() && i++ < 1000)
        QTest::qWait(10);
    QTest::qWait(0);
    
    QVERIFY(action.status().errorCode == QMailServiceAction::Status::ErrNoError);
    QVERIFY(action.activity() == QMailServiceAction::Successful);
    
    QVERIFY(QMailStore::instance()->countMessages(QMailMessageKey::parentFolderId(trashId1)) == (oldTrashCount + 1));
    QVERIFY(QMailStore::instance()->countMessages(QMailMessageKey::parentFolderId(inboxId2)) == (oldInboxCount - 1));

    QMailMessageMetaData metadata("savedMessage3", accountId2);
    QVERIFY(metadata.parentFolderId() == trashId1);
    QVERIFY(metadata.previousParentFolderId() == inboxId2);
    QVERIFY(metadata.status() & QMailMessage::Trash);
}

void tst_QMailStorageAction::test_storageaction_restoretopreviousfolder()
{
    QMailStorageAction action;
    QMailMessageIdList list;

    list = QMailStore::instance()->queryMessages(QMailMessageKey::serverUid("savedMessage3"));
    QVERIFY(list.count() == 1);
    int oldInboxCount = QMailStore::instance()->countMessages(QMailMessageKey::parentFolderId(inboxId2));
    int oldTrashCount = QMailStore::instance()->countMessages(QMailMessageKey::parentFolderId(trashId1));

    action.restoreToPreviousFolder(QMailMessageKey::id(list));

    int i = 0;
    while (action.isRunning() && i++ < 1000)
        QTest::qWait(10);
    QTest::qWait(0);
    
    QVERIFY(action.status().errorCode == QMailServiceAction::Status::ErrNoError);
    QVERIFY(action.activity() == QMailServiceAction::Successful);
    
    QVERIFY(QMailStore::instance()->countMessages(QMailMessageKey::parentFolderId(inboxId2)) == (oldInboxCount + 1));
    QVERIFY(QMailStore::instance()->countMessages(QMailMessageKey::parentFolderId(trashId1)) == (oldTrashCount - 1));

    QMailMessageMetaData metadata("savedMessage3", accountId2);
    QVERIFY(metadata.parentFolderId() == inboxId2);
    QVERIFY(metadata.previousParentFolderId() == QMailFolderId());
}

void tst_QMailStorageAction::test_storageaction_movetofolder()
{
    QMailStorageAction action;
    QMailMessageIdList list;

    list = QMailStore::instance()->queryMessages(QMailMessageKey::serverUid("savedMessage3"));
    QVERIFY(list.count() == 1);
    int oldInboxCount = QMailStore::instance()->countMessages(QMailMessageKey::parentFolderId(inboxId2));
    int oldTrashCount = QMailStore::instance()->countMessages(QMailMessageKey::parentFolderId(trashId1));

    action.moveToFolder(list, trashId1);

    int i = 0;
    while (action.isRunning() && i++ < 1000)
        QTest::qWait(10);
    QTest::qWait(0);
    
    QVERIFY(action.status().errorCode == QMailServiceAction::Status::ErrNoError);
    QVERIFY(action.activity() == QMailServiceAction::Successful);
    
    QVERIFY(QMailStore::instance()->countMessages(QMailMessageKey::parentFolderId(inboxId2)) == (oldInboxCount - 1));
    QVERIFY(QMailStore::instance()->countMessages(QMailMessageKey::parentFolderId(trashId1)) == (oldTrashCount + 1));

    QMailMessageMetaData metadata("savedMessage3", accountId2);
    QVERIFY(metadata.parentFolderId() == trashId1);
    QVERIFY(metadata.previousParentFolderId() == inboxId2);
}

void tst_QMailStorageAction::test_storageaction_flagMessages()
{
    QMailStorageAction action;
    QMailMessageIdList list;

    QMailMessageMetaData metadata("savedMessage3", accountId2);
    QVERIFY(metadata.status() & QMailMessage::Read);
        
    list = QMailStore::instance()->queryMessages(QMailMessageKey::serverUid("savedMessage3"));
    QVERIFY(list.count() == 1);

    action.flagMessages(list, 0, QMailMessage::Read);

    int i = 0;
    while (action.isRunning() && i++ < 1000)
        QTest::qWait(10);
    QTest::qWait(0);

    QVERIFY(action.status().errorCode == QMailServiceAction::Status::ErrNoError);
    QVERIFY(action.activity() == QMailServiceAction::Successful);
    
    QMailMessageMetaData metadataAfter("savedMessage3", accountId2);
    QVERIFY((metadataAfter.status() & QMailMessage::Read) == false);
}

void tst_QMailStorageAction::test_storageaction_rollBackUpdates()
{
    QMailStorageAction action;
    QMailMessageId saved3id;
    QMailMessage message;
    QMailMessageIdList list;
    
    message.setMessageType(QMailMessage::Email);
    message.setParentAccountId(accountId3);
    message.setParentFolderId(inboxId3);
    message.setFrom(QMailAddress("barney@example.net"));
    message.setTo(QMailAddressList() << QMailAddress("account3@example.org") << QMailAddress("testing@test"));
    message.setSubject("Rollback test message");
    message.setDate(QMailTimeStamp(QDateTime(QDate::currentDate().addDays(-98))));
    message.setReceivedDate(QMailTimeStamp(QDateTime(QDate::currentDate().addDays(-98))));
    message.setStatus(QMailMessage::Incoming, true);
    message.setStatus(QMailMessage::New, false);
    message.setStatus(QMailMessage::Read, false);
    message.setServerUid("savedMessage5");
    message.setSize(7 * 1024);
    message.setContent(QMailMessage::HtmlContent);
    message.setCustomField("present", "true");
    
    QVERIFY(QMailStore::instance()->addMessage(&message));

    QMailMessageMetaData metadata("savedMessage5", accountId3);
    QVERIFY(metadata.parentFolderId() == inboxId3);
    QVERIFY(metadata.previousParentFolderId() == QMailFolderId());

    list = QMailStore::instance()->queryMessages(QMailMessageKey::serverUid("savedMessage5"));
    QVERIFY(list.count() == 1);

    QMailDisconnected::moveToFolder(list, trashId2);
    {
        QMailMessageMetaData m("savedMessage5", accountId3);
        QVERIFY(m.parentFolderId() == trashId2);
        QVERIFY(m.previousParentFolderId() == inboxId3);
    }
    action.rollBackUpdates(accountId3);

    int i = 0;
    while (action.isRunning() && i++ < 1000)
        QTest::qWait(10);
    QTest::qWait(0);

    QVERIFY(action.status().errorCode == QMailServiceAction::Status::ErrNoError);
    QVERIFY(action.activity() == QMailServiceAction::Successful);
    
    QMailMessageMetaData metadataAfter("savedMessage5", accountId3);
    QVERIFY(metadataAfter.parentFolderId() == inboxId3);
    QVERIFY(metadataAfter.previousParentFolderId() == QMailFolderId());

    // Can't test QMailDisconnected::upatesOutstanding(accountId3) == false,  because having any 
    // local messages at all causes a folder to be considered as having updates outstanding
}

void tst_QMailStorageAction::test_storageaction_deleteMessages()
{
    QMailStorageAction action;
    QMailMessageIdList list;

    list = QMailStore::instance()->queryMessages(QMailMessageKey::serverUid("savedMessage3"));
    QVERIFY(list.count() == 1);

    action.deleteMessages(list);

    int i = 0;
    while (action.isRunning() && i++ < 1000)
        QTest::qWait(10);
    QTest::qWait(0);
    
    QVERIFY(action.status().errorCode == QMailServiceAction::Status::ErrNoError);
    QVERIFY(action.activity() == QMailServiceAction::Successful);
    
    list = QMailStore::instance()->queryMessages(QMailMessageKey::serverUid("savedMessage3"));
    QVERIFY(list.count() == 0);
}


void tst_QMailStorageAction::test_storageaction_discardMessages()
{
    QMailStorageAction action;
    QMailMessageId saved3id;
    QMailMessage message;
    QMailMessageIdList messages;
    
    message.setMessageType(QMailMessage::Email);
    message.setParentAccountId(accountId2);
    message.setParentFolderId(inboxId2);
    message.setFrom(QMailAddress("barney@example.net"));
    message.setTo(QMailAddressList() << QMailAddress("account2@example.org") << QMailAddress("testing@test"));
    message.setSubject("Another test message");
    message.setDate(QMailTimeStamp(QDateTime(QDate::currentDate().addDays(-99))));
    message.setReceivedDate(QMailTimeStamp(QDateTime(QDate::currentDate().addDays(-99))));
    message.setStatus(QMailMessage::Incoming, true);
    message.setStatus(QMailMessage::New, false);
    message.setStatus(QMailMessage::Read, false);
    message.setServerUid("savedMessage4");
    message.setSize(6 * 1024);
    message.setContent(QMailMessage::HtmlContent);
    message.setCustomField("present", "true");
    
    QVERIFY(QMailStore::instance()->addMessage(&message));
    QMailMessageKey savedMessage4Key(QMailMessageKey::serverUid("savedMessage4"));
    QVERIFY(QMailStore::instance()->countMessages(savedMessage4Key) == 1);

    messages << message.id();
    action.discardMessages(messages);
        
    int i = 0;
    while (action.isRunning() && i++ < 1000)
        QTest::qWait(10);
    QTest::qWait(0);
 
    QVERIFY(action.status().errorCode == QMailServiceAction::Status::ErrNoError);
    QVERIFY(action.activity() == QMailServiceAction::Successful);
    
    QVERIFY(QMailStore::instance()->countMessages(savedMessage4Key) == 0);
}
