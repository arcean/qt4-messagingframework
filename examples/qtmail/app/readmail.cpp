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

#include "readmail.h"
#include <qmailviewer.h>
#include <qmaillog.h>
#include <qlabel.h>
#include <qimage.h>
#include <qaction.h>
#include <qfile.h>
#include <qtextbrowser.h>
#include <qtextstream.h>
#include <qcursor.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qregexp.h>
#include <qstackedwidget.h>
#include <qmessagebox.h>
#include <qboxlayout.h>
#include <qevent.h>
#include <qimagereader.h>
#include <qalgorithms.h>
#include <qmailaccount.h>
#include <qmailcomposer.h>
#include <qmailfolder.h>
#include <qmailstore.h>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QMenuBar>
#include <QPalette>
#include <QApplication>
#include <QProcess>

class SaveContactDialog : public QDialog
{
    Q_OBJECT

public:
    enum Selection { None = 0, Create, Existing };

    SaveContactDialog(const QMailAddress &address, QWidget *parent = 0)
        : QDialog(parent),
          sel(None),
          createButton(new QPushButton(tr("Create new contact"))),
          existingButton(new QPushButton(tr("Add to existing")))
    {
        setObjectName("SaveContactDialog");
        setModal(true);

        setWindowTitle(tr("Save"));

        connect(createButton, SIGNAL(clicked()), this, SLOT(buttonClicked()));
        connect(existingButton, SIGNAL(clicked()), this, SLOT(buttonClicked()));

        QString text = tr("Saving '%1' to Contacts.", "%1=name/address/number").arg(address.address());
        text += '\n';
        text += tr("Create new contact or add to an existing contact?");

        QPlainTextEdit* textEdit = new QPlainTextEdit(text, this);
        textEdit->setReadOnly(true);
        textEdit->setFocusPolicy(Qt::NoFocus);
        textEdit->setFrameStyle(QFrame::NoFrame);
        textEdit->viewport()->setBackgroundRole(QPalette::Window);

        QVBoxLayout *vbl = new QVBoxLayout;
        vbl->setSpacing(0);
        vbl->setContentsMargins(0, 0, 0, 0);
        vbl->addWidget(textEdit);
        vbl->addWidget(createButton);
        vbl->addWidget(existingButton);
        setLayout(vbl);
    }

    Selection selection() const { return sel; }

protected slots:
    void buttonClicked()
    {
        if (sender() == createButton) {
            sel = Create;
        } else if (sender() == existingButton) {
            sel = Existing;
        } else {
            reject();
            return;
        }

        accept();
    }

private:
    Selection sel;
    QPushButton *createButton;
    QPushButton *existingButton;
};


ReadMail::ReadMail( QWidget* parent, Qt::WFlags fl )
    : QFrame(parent, fl),
      sending(false),
      receiving(false),
      firstRead(false),
      hasNext(false),
      hasPrevious(false),
      modelUpdatePending(false)
{
    init();
}

void ReadMail::init()
{
    getThisMailButton = new QAction( QIcon(":icon/getmail"), tr("Get message"), this );
    connect(getThisMailButton, SIGNAL(triggered()), this, SLOT(getThisMail()) );
    getThisMailButton->setWhatsThis( tr("Retrieve this message from the server.  You can use this option to retrieve individual messages that would normally not be automatically downloaded.") );

    views = new QStackedWidget(this);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0,0,0,0);
    layout->setSpacing(0);
    layout->addWidget(views);

    setFrameStyle(QFrame::StyledPanel| QFrame::Sunken);

    // Update the view if the displayed message's content changes
    connect(QMailStore::instance(), SIGNAL(messageContentsModified(QMailMessageIdList)), 
            this, SLOT(messageContentsModified(QMailMessageIdList)));

    // Remove the view if the message is removed
    connect(QMailStore::instance(), SIGNAL(messagesRemoved(QMailMessageIdList)), 
            this, SLOT(messagesRemoved(QMailMessageIdList)));
}

QMailMessageId ReadMail::displayedMessage() const 
{
    return mail.id();
}

bool ReadMail::handleIncomingMessages(const QMailMessageIdList &list) const
{
    if (QMailViewerInterface* viewer = currentViewer()) {
        if (viewer->handleIncomingMessages(list)) {
            // Mark each of these messages as read
            quint64 status(QMailMessage::Read);
            return QMailStore::instance()->updateMessagesMetaData(QMailMessageKey::id(list), status, true);
        }
    }

    return false;
}

bool ReadMail::handleOutgoingMessages(const QMailMessageIdList &list) const
{
    if (QMailViewerInterface* viewer = currentViewer())
        return viewer->handleOutgoingMessages(list);

    return false;
}

/*  We need to be careful here. Don't allow clicking on any links
    to automatically install anything.  If we want that, we need to
    make sure that the mail doesn't contain mailicious link encoding
*/
void ReadMail::linkClicked(const QUrl &lnk)
{
    QString str = lnk.toString();
    QRegExp commandPattern("(\\w+);(.+)");

    if (commandPattern.exactMatch(str)) {
        QString command = commandPattern.cap(1);
        QString param = commandPattern.cap(2);

        if (command == "attachment") {
            if (param.startsWith("scrollto;")) {
                if (QMailViewerInterface* viewer = currentViewer())
                    viewer->scrollToAnchor(param.mid(9));
            }
        } else if (command == "dial") {
            //dialNumber(param);
        } else if (command == "message") {
            emit sendMessageTo(QMailAddress(param), QMailMessage::Sms);
        } else if (command == "store") {
            //storeContact(QMailAddress(param), mail.messageType());
        } else if (command == "contact") {
            //displayContact(QUniqueId(param));
        }
    } else if (str.startsWith("mailto:")) {
        // strip leading 'mailto:'
        emit sendMessageTo( QMailAddress(str.mid(7)), mail.messageType() );
    } else if (mail.messageType() == QMailMessage::System && str.startsWith(QLatin1String("qtopiaservice:"))) {
        // TODO: This relevant anymore?
        int commandPos  = str.indexOf( QLatin1String( "::" ) ) + 2;
        int argPos      = str.indexOf( '?' ) + 1;
        QString service = str.mid( 14, commandPos - 16 );
        QString command;
        QStringList args;

        if (argPos > 0) {
            command = str.mid( commandPos, argPos - commandPos - 1 );
            args    = str.mid( argPos ).split( ',' );
        } else {
            command = str.mid( commandPos );
        }
    } else {
        // Try opening this link via a service
        QProcess launcher;
        launcher.start("xdg-open", QStringList() << str);
        if (!launcher.waitForStarted()) {
            qWarning() << "Unable to execute URL:" << str;
        } else if (!launcher.waitForFinished()) {
            qWarning() << "Unable to wait for execution of URL:" << str;
        }
    }
}

QString ReadMail::displayName(const QMailMessage& mail)
{
    const bool outgoing(mail.status() & QMailMessage::Outgoing);

    QString name;
    if (outgoing) {
        if (!mail.to().isEmpty())
              name = mail.to().first().toString();
    } else {
          name = mail.from().toString();
    }

    if (name.isEmpty()) {
        // Use the type of this message as the title
        QString key(QMailComposerFactory::defaultKey(mail.messageType()));
        if (!key.isEmpty())
            name = QMailComposerFactory::displayName(key, mail.messageType());
        else
            name = tr("Message");

        if (!name.isEmpty())
            name[0] = name[0].toUpper();
    }

    if (outgoing)
        name.prepend(tr("To:") + ' ');

    return name;
}

void ReadMail::updateView(QMailViewerFactory::PresentationType type)
{
    if ( !mail.id().isValid() )
        return;

    if (type == QMailViewerFactory::AnyPresentation) {
        type = QMailViewerFactory::StandardPresentation;
        if (mail.messageType() == QMailMessage::Instant) {
            type = QMailViewerFactory::ConversationPresentation;
        }
    }

    QMailMessage::ContentType content(mail.content());
    if ((content == QMailMessage::NoContent) || (content == QMailMessage::UnknownContent)) {
        // Attempt to show the message as text, from the subject if necessary
        content = QMailMessage::PlainTextContent;
    }

    QMailViewerInterface* view = viewer(content, type);
    if (!view) {
        qMailLog(Messaging) << "Unable to view message" << mail.id() << "with content:" << content;
        return;
    }

    // Mark message as read before showing viewer, to avoid reload on change notification
    updateReadStatus();

    view->clear();

    if (mail.messageType() != QMailMessage::System) {
        initImages(view);
    }

    view->setMessage(mail);

    QAction* separator = new QAction(this); separator->setSeparator(true);
    view->addActions(QList<QAction*>() << getThisMailButton << separator << this->actions());
    switchView(view, displayName(mail));
}

void ReadMail::keyPressEvent(QKeyEvent *e)
{
    switch( e->key() ) {
        default:
            QWidget::keyPressEvent( e );
    }
}

//update view with current EmailListItem (item)
void ReadMail::displayMessage(const QMailMessageId& id, QMailViewerFactory::PresentationType type, bool nextAvailable, bool previousAvailable)
{
    if (!id.isValid())
        return;

    hasNext = nextAvailable;
    hasPrevious = previousAvailable;

    showMessage(id, type);

    updateButtons();

    //report currently viewed mail so that it will be
    //placed first in the queue of new mails to download.
    emit viewingMail(mail);
}

void ReadMail::buildMenu(const QString &mailbox)
{
    Q_UNUSED(mailbox);
}

void ReadMail::messageContentsModified(const QMailMessageIdList& list)
{
    if (!mail.id().isValid())
        return;

    if (list.contains(mail.id())) {
        loadMessage(mail.id());
        if (QMailViewerInterface* viewer = currentViewer())
            viewer->setMessage(mail);
        return;
    }

    updateButtons();
}

void ReadMail::messagesRemoved(const QMailMessageIdList& list)
{
    if (!mail.id().isValid())
        return;

    if (list.contains(mail.id())) {
        mail = QMailMessage();

        if (QMailViewerInterface* viewer = currentViewer())
            viewer->clear();
    }
}

void ReadMail::showMessage(const QMailMessageId& id, QMailViewerFactory::PresentationType type)
{
    loadMessage(id);

    updateView(type);
}

void ReadMail::messageChanged(const QMailMessageId &id)
{
    // Ignore updates from viewers that aren't currently top of the stack
    if (sender() == static_cast<QObject*>(currentViewer())) {
        loadMessage(id);
    }
}

void ReadMail::loadMessage(const QMailMessageId &id)
{
    mail = QMailMessage(id);
    updateButtons();
}

void ReadMail::updateButtons()
{
    if (!mail.id().isValid())
        return;

    bool incoming(mail.status() & QMailMessage::Incoming);
    bool downloaded(mail.status() & QMailMessage::ContentAvailable);
    bool removed(mail.status() & QMailMessage::Removed);
    bool messageReceived(downloaded || receiving);

    getThisMailButton->setVisible( !messageReceived && !removed && incoming );
}

void ReadMail::respondToMessage(QMailMessage::ResponseType type)
{
    emit responseRequested(mail, type);
}

void ReadMail::respondToMessagePart(const QMailMessagePart::Location &partLocation, QMailMessage::ResponseType type)
{
    emit responseRequested(partLocation, type);
}

void ReadMail::getThisMail()
{
    emit getMailRequested(mail);
}

void ReadMail::retrieveMessagePortion(uint bytes)
{
    emit retrieveMessagePortion(mail, bytes);
}

void ReadMail::retrieveMessagePart(const QMailMessagePart &part)
{
    QMailMessagePart::Location location(part.location());
    location.setContainingMessageId(mail.id());
    emit retrieveMessagePart(location);
}

void ReadMail::retrieveMessagePartPortion(const QMailMessagePart &part, uint bytes)
{
    QMailMessagePart::Location location(part.location());
    location.setContainingMessageId(mail.id());
    emit retrieveMessagePartPortion(location, bytes);
}

void ReadMail::setSendingInProgress(bool on)
{
    sending = on;
    if ( isVisible() )
        updateButtons();
}

void ReadMail::setRetrievalInProgress(bool on)
{
    receiving = on;
    if ( isVisible() )
        updateButtons();
}

void ReadMail::initImages(QMailViewerInterface* view)
{
    static bool initialized = false;
    static QMap<QUrl, QVariant> resourceMap;

    if (!initialized) {
    // Add the predefined smiley images for EMS messages.
        resourceMap.insert( QUrl( "x-sms-predefined:ironic" ),
                            QVariant( QImage( ":image/smiley/ironic" ) ) );
        resourceMap.insert( QUrl( "x-sms-predefined:glad" ),
                            QVariant( QImage( ":image/smiley/glad" ) ) );
        resourceMap.insert( QUrl( "x-sms-predefined:skeptical" ),
                            QVariant( QImage( ":image/smiley/skeptical" ) ) );
        resourceMap.insert( QUrl( "x-sms-predefined:sad" ),
                            QVariant( QImage( ":image/smiley/sad" ) ) );
        resourceMap.insert( QUrl( "x-sms-predefined:wow" ),
                            QVariant( QImage( ":image/smiley/wow" ) ) );
        resourceMap.insert( QUrl( "x-sms-predefined:crying" ),
                            QVariant( QImage( ":image/smiley/crying" ) ) );
        resourceMap.insert( QUrl( "x-sms-predefined:winking" ),
                            QVariant( QImage( ":image/smiley/winking" ) ) );
        resourceMap.insert( QUrl( "x-sms-predefined:laughing" ),
                            QVariant( QImage( ":image/smiley/laughing" ) ) );
        resourceMap.insert( QUrl( "x-sms-predefined:indifferent" ),
                            QVariant( QImage( ":image/smiley/indifferent" ) ) );
        resourceMap.insert( QUrl( "x-sms-predefined:kissing" ),
                            QVariant( QImage( ":image/smiley/kissing" ) ) );
        resourceMap.insert( QUrl( "x-sms-predefined:confused" ),
                            QVariant( QImage( ":image/smiley/confused" ) ) );
        resourceMap.insert( QUrl( "x-sms-predefined:tongue" ),
                            QVariant( QImage( ":image/smiley/tongue" ) ) );
        resourceMap.insert( QUrl( "x-sms-predefined:angry" ),
                            QVariant( QImage( ":image/smiley/angry" ) ) );
        resourceMap.insert( QUrl( "x-sms-predefined:glasses" ),
                            QVariant( QImage( ":image/smiley/glasses" ) ) );
        resourceMap.insert( QUrl( "x-sms-predefined:devil" ),
                            QVariant( QImage( ":image/smiley/devil" ) ) );
        initialized = true;
    }

    QMap<QUrl, QVariant>::iterator it = resourceMap.begin(), end = resourceMap.end();
    for ( ; it != end; ++it)
        view->setResource( it.key(), it.value() );
}

void ReadMail::switchView(QMailViewerInterface* viewer, const QString& title)
{
    if (currentViewer() == viewer)
        currentView.pop();

    lastTitle = title;

    views->setCurrentWidget(viewer->widget());

    currentView.push(qMakePair(viewer, title));
}

QMailViewerInterface* ReadMail::currentViewer() const 
{
    if (currentView.isEmpty())
        return 0;

    return currentView.top().first;
}

QMailViewerInterface* ReadMail::viewer(QMailMessage::ContentType content, QMailViewerFactory::PresentationType type)
{
    ViewerMap::iterator it = contentViews.find(qMakePair(content, type));
    if (it == contentViews.end()) {
        QString key = QMailViewerFactory::defaultKey(content, type);
        if (key.isEmpty())
            return 0;

        QMailViewerInterface* view = QMailViewerFactory::create(key, views);

        if (contentViews.values().contains(view)) {
            // We already have this view created
        } else {
            view->setObjectName("read-message");
            view->widget()->setWhatsThis(tr("This view displays the contents of the message."));
            connect(view, SIGNAL(respondToMessage(QMailMessage::ResponseType)), this, SLOT(respondToMessage(QMailMessage::ResponseType)));
            connect(view, SIGNAL(respondToMessagePart(QMailMessagePart::Location, QMailMessage::ResponseType)), this, SLOT(respondToMessagePart(QMailMessagePart::Location, QMailMessage::ResponseType)));
            connect(view, SIGNAL(anchorClicked(QUrl)), this, SLOT(linkClicked(QUrl)));
            connect(view, SIGNAL(messageChanged(QMailMessageId)), this, SLOT(messageChanged(QMailMessageId)));
            connect(view, SIGNAL(viewMessage(QMailMessageId,QMailViewerFactory::PresentationType)), this, SIGNAL(viewMessage(QMailMessageId,QMailViewerFactory::PresentationType)));
            connect(view, SIGNAL(sendMessage(QMailMessage&)), this, SIGNAL(sendMessage(QMailMessage&)));
            connect(view, SIGNAL(retrieveMessage()), getThisMailButton, SLOT(trigger()));
            connect(view, SIGNAL(retrieveMessagePortion(uint)), this, SLOT(retrieveMessagePortion(uint)));
            connect(view, SIGNAL(retrieveMessagePart(QMailMessagePart)), this, SLOT(retrieveMessagePart(QMailMessagePart)));
            connect(view, SIGNAL(retrieveMessagePartPortion(QMailMessagePart, uint)), this, SLOT(retrieveMessagePartPortion(QMailMessagePart, uint)));

            view->widget()->setGeometry(geometry());
            views->addWidget(view->widget());
        }

        it = contentViews.insert(qMakePair(content, type), view);
    }

    return it.value();
}

void ReadMail::updateReadStatus()
{
    if (mail.id().isValid()) {
        quint64 setMask(0);
        quint64 unsetMask(0);

        if (mail.status() & QMailMessage::New) {
            // This message is no longer new
            unsetMask |= QMailMessage::New;
        }

        if (mail.status() & QMailMessage::PartialContentAvailable) {
            if (!(mail.status() & QMailMessage::Read)) {
                // Do not mark as read unless it has been at least partially downloaded
                setMask |= QMailMessage::Read;
            }
        }

        if (setMask || unsetMask) {
            emit flagMessage(mail.id(), setMask, unsetMask);
        }
    }
}

