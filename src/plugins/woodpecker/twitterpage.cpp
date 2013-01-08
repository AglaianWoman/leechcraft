#include "twitterpage.h"
#include "core.h"
#include <interfaces/core/ientitymanager.h>
#include <interfaces/core/icoreproxy.h>
#include "util/util.h"
#include <qjson/parser.h>
#include <QListWidgetItem>
#include "xmlsettingsmanager.h"
#include "twitdelegate.h"

Q_DECLARE_METATYPE (QObject**);

namespace LeechCraft
{
namespace Woodpecker
{

QObject *TwitterPage::S_MultiTabsParent_ = 0;

TwitterPage::TwitterPage (QWidget *parent) : QWidget (parent),
	ui (new Ui::TwitterPage),
	Toolbar_ (new QToolBar)
{
	ui->setupUi (this);
	ui->TwitList_->setItemDelegate(new TwitDelegate(ui->TwitList_));
	
/*	myListWidget->setItemDelegate(new ListDelegate(myListWidget));
	QListWidgetItem *item = new QListWidgetItem();
	item->setData(Qt::DisplayRole, "Title");
	item->setData(Qt::UserRole + 1, "Description");
	myListWidget->addItem(item);
	*/
//	Toolbar_->addAction(ui->actionRefresh);
	interface = new twitterInterface (this);
	connect (interface, SIGNAL (tweetsReady (QList<std::shared_ptr<Tweet> >)),
			 this, SLOT (updateTweetList (QList<std::shared_ptr<Tweet> >)));
	timer = new QTimer (this);
	timer->setInterval (XmlSettingsManager::Instance()->property("timer").toInt() * 1000); // Update twits every 1.5 minutes by default
	connect (timer, SIGNAL (timeout()), interface, SLOT (getHomeFeed()));
	tryToLogin();
	int newSliderPos;

	connect(ui->TwitEdit_, SIGNAL(returnPressed()), ui->TwitButton_, SLOT(click()));
	
	connect((ui->TwitList_->verticalScrollBar()), SIGNAL (valueChanged(int)),
			this, SLOT (scrolledDown(int)));
//    connect(ui->login_Test_, SIGNAL (clicked ()), SLOT(tryToLogin()));
	connect (ui->TwitButton_, SIGNAL (clicked()), SLOT (twit()));
	settings = new QSettings (QCoreApplication::organizationName (),
							  QCoreApplication::applicationName () + "_Woodpecker");
	connect(ui->TwitList_, SIGNAL(clicked()), SLOT(getHomeFeed()));

	actionRetwit_ = new QAction (tr ("Retwit"), ui->TwitList_);
	actionRetwit_->setShortcut(Qt::Key_R + Qt::ALT);
	actionRetwit_->setProperty ("ActionIcon", "edit-redo");
	connect (actionRetwit_, SIGNAL (triggered ()), this, SLOT (retwit()));
	
	actionReply_ = new QAction (tr ("Reply"), ui->TwitList_);
	actionReply_->setShortcut(Qt::Key_A + Qt::ALT);
	actionReply_->setProperty ("ActionIcon", "mail-reply-sender");
	connect (actionReply_, SIGNAL (triggered ()), this, SLOT (reply()));
	
	actionSPAM_ = new QAction (tr ("Report SPAM"), ui->TwitList_);
	actionSPAM_->setProperty ("ActionIcon", "dialog-close");
	connect (actionSPAM_, SIGNAL (triggered ()), this, SLOT (reportSpam()));
	
	connect(ui->TwitList_, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(reply()));
	
	ui->TwitList_->addAction(actionRetwit_);
	ui->TwitList_->addAction(actionReply_);
	
	if ( (! settings->value ("token").isNull()) && (! settings->value ("tokenSecret").isNull()))
	{
		qDebug() << "Have an authorized" << settings->value ("token") << ":" << settings->value ("tokenSecret");
		interface->login (settings->value ("token").toString(), settings->value ("tokenSecret").toString());
		interface->getHomeFeed();
		timer->start();
	}

}

TwitterPage::~TwitterPage()
{
	settings->deleteLater();
	timer->stop();
	timer->deleteLater();
	interface->deleteLater();
}

TabClassInfo TwitterPage::GetTabClassInfo () const
{
	return Core::Instance ().GetTabClass ();
}

QToolBar* TwitterPage::GetToolBar () const
{
	return Toolbar_;
}

QObject* TwitterPage::ParentMultiTabs ()
{
	return S_MultiTabsParent_;
}

QList<QAction*> TwitterPage::GetTabBarContextMenuActions () const
{
	return QList<QAction*> ();
}

QMap<QString, QList<QAction*> > TwitterPage::GetWindowMenus () const
{
	return WindowMenus_;
}

void TwitterPage::SetParentMultiTabs (QObject *parent)
{
	S_MultiTabsParent_ = parent;
}

void TwitterPage::Remove()
{
	emit removeTab (this);
	deleteLater ();
}

void TwitterPage::tryToLogin()
{
	interface->getAccess();
	connect (interface, SIGNAL (authorized (QString, QString)), SLOT (recvdAuth (QString, QString)));
}

void TwitterPage::requestUserTimeline (QString username)
{
	interface->getUserTimeline (username);
}

void TwitterPage::updateTweetList (QList< std::shared_ptr< Tweet > > twits)
{
	std::shared_ptr<Tweet> twit;
	std::shared_ptr<Tweet> firstNewTwit;
	int i;

	if (twits.isEmpty()) return; // if we have no tweets to parse
	
	firstNewTwit = twits.first();

	if (screenTwits.length() && (twits.last()->id() == screenTwits.first()->id())) // if we should prepend
		for (auto i = twits.end()-2; i >= twits.begin(); i--)
			screenTwits.insert(0, *i);
	else
	{
		// Now we'd find firstNewTwit in twitList

		for (i = 0; i < screenTwits.length(); i++)
			if ( (screenTwits.at (i)->id()) == firstNewTwit->id()) break;

		int insertionShift = screenTwits.length() - i;    // We've already got insertionShift twits to our list

		for (i = 0; i < insertionShift; i++)
			twits.removeFirst();

		if (XmlSettingsManager::Instance ()->property ("notify").toBool())
		{
			if (twits.length() == 1)			// We can notify the only twit
			{
				Entity notification = Util::MakeNotification (twits.first()->author()->username() , twits.first()->text() , PInfo_);
				emit gotEntity(notification);
				Core::Instance().GetProxy()->GetEntityManager()->HandleEntity(notification);
			}
			else if (!twits.isEmpty()) {
				Entity notification = Util::MakeNotification (tr ("Woodpecker") , tr ( "%1 new twit(s)" ).arg(twits.length()) , PInfo_);
				emit gotEntity(notification);
				Core::Instance().GetProxy()->GetEntityManager()->HandleEntity(notification);
			}
		}
		
		screenTwits.append (twits);
	}
	ui->TwitList_->clear();

	Q_FOREACH (twit, screenTwits)
	{
		QListWidgetItem *tmpitem = new QListWidgetItem();
		
		tmpitem->setData(Qt::DisplayRole, "Title");
		tmpitem->setData(Qt::UserRole, twit->id());
		tmpitem->setData(Qt::UserRole + 1, twit->author()->username());
		tmpitem->setData(Qt::UserRole + 2, twit->dateTime().toLocalTime().toString());
		
		tmpitem->setText (twit->text().replace(QChar('\n'),QChar(' ')));
		if (twit->author()->avatar.isNull())
			tmpitem->setData(Qt::DecorationRole, QIcon (":/resources/images/woodpecker.svg"));
		else
			tmpitem->setData(Qt::DecorationRole, twit->author()->avatar);
		ui->TwitList_->insertItem (0, tmpitem);
		ui->TwitList_->updateGeometry();
		
	}
//	QTimer::singleShot(1000, ui->TwitList_, SLOT(update()));
	
	ui->TwitList_->update();
	ui->TwitList_->setEnabled(true);
}

void TwitterPage::recvdAuth (QString token, QString tokenSecret)
{
	settings->setValue ("token", token);
	settings->setValue ("tokenSecret", tokenSecret);
	interface->getHomeFeed();
	timer->start();
}

void TwitterPage::twit()
{
	interface->sendTweet (ui->TwitEdit_->text());
	ui->TwitEdit_->clear();
}

void TwitterPage::retwit()
{
	const auto& idx = ui->TwitList_->currentItem();
	auto twitid = idx->data(Qt::UserRole);
	interface->retweet(twitid.toULongLong());
}

void TwitterPage::sendReply()
{
	const auto& idx = ui->TwitList_->currentItem();
	auto twitid = idx->data(Qt::UserRole);
	interface->reply(twitid.toULongLong(), ui->TwitEdit_->text());
	ui->TwitEdit_->clear();
	disconnect (ui->TwitButton_, SIGNAL(clicked()), 0, 0);
	connect (ui->TwitButton_, SIGNAL (clicked()), SLOT (twit()));
}

void TwitterPage::reply()
{
	const auto& idx = ui->TwitList_->currentItem();
	const auto twitid = idx->data(Qt::UserRole).toULongLong();
	auto replyto = std::find_if (screenTwits.begin (), screenTwits.end (), 
			  [twitid] 
			  (decltype (screenTwits.front ()) tweet) 
			  { return tweet->id() == twitid; });
	ui->TwitEdit_->setText(QString("@").append((*replyto)->author()->username()).append(" "));
	disconnect (ui->TwitButton_, SIGNAL(clicked()), 0, 0);
	connect (ui->TwitButton_, SIGNAL (clicked()), SLOT (sendReply()));
	ui->TwitEdit_->setFocus();
}

void TwitterPage::reply(QListWidgetItem* idx)
{
	const auto twitid = idx->data(Qt::UserRole).toULongLong();
	auto replyto = std::find_if (screenTwits.begin (), screenTwits.end (), 
			  [twitid] 
			  (decltype (screenTwits.front ()) tweet) 
			  { return tweet->id() == twitid; });
	ui->TwitEdit_->setText(QString("@").append((*replyto)->author()->username()).append(" "));
	disconnect (ui->TwitButton_, SIGNAL(clicked()), 0, 0);
	connect (ui->TwitButton_, SIGNAL (clicked()), SLOT (sendReply()));
	ui->TwitEdit_->setFocus();
}


void TwitterPage::scrolledDown (int sliderPos)
{
	if (sliderPos == ui->TwitList_->verticalScrollBar()->maximum())
	{
		ui->TwitList_->verticalScrollBar()->setSliderPosition(ui->TwitList_->verticalScrollBar()->maximum()-1);
		ui->TwitList_->setEnabled(false);
		if (not screenTwits.empty())
			interface->getMoreTweets(QString("%1").arg((*(screenTwits.begin()))->id()));
	}
}

void TwitterPage::on_TwitList__customContextMenuRequested(const QPoint& pos)
	{
		qDebug() << "MENUSLOT";
		const auto& idx = ui->TwitList_->indexAt (pos);
		if (!idx.isValid ())
			return;

		auto menu = new QMenu (ui->TwitList_);
		menu->addAction (actionRetwit_);
		menu->addAction (actionReply_);
		menu->addSeparator();
		menu->addAction (actionSPAM_);
/*		if (idx.data (Player::Role::IsAlbum).toBool ())
			menu->addAction (ActionShowAlbumArt_);
		else
		{
			menu->addAction (ActionStopAfterSelected_);
			menu->addAction (ActionShowTrackProps_);
		}
*/
		menu->setAttribute (Qt::WA_DeleteOnClose);

		menu->exec (ui->TwitList_->viewport ()->mapToGlobal (pos));
	}

void TwitterPage::reportSpam()
{
	const auto& idx = ui->TwitList_->currentItem();
	const auto twitid = idx->data(Qt::UserRole).toULongLong();
	
	auto spamTwit = std::find_if (screenTwits.begin (), screenTwits.end (), 
			  [twitid] 
			  (decltype (screenTwits.front ()) tweet) 
			  { return tweet->id() == twitid; });
	interface->reportSPAM((*spamTwit)->author()->username());
}

void TwitterPage::updateTweetList()
{
	ui->TwitList_->setEnabled(false);
	ui->TwitList_->clear();

	Q_FOREACH (auto twit, screenTwits)
	{
		QListWidgetItem *tmpitem = new QListWidgetItem();
		
		tmpitem->setData(Qt::DisplayRole, "Title");
		tmpitem->setData(Qt::UserRole, twit->id());
		tmpitem->setData(Qt::UserRole + 1, twit->author()->username());
		tmpitem->setData(Qt::UserRole + 2, twit->dateTime().toLocalTime().toString());
		
		tmpitem->setText (twit->text().replace(QChar('\n'),QChar(' ')));
		if (twit->author()->avatar.isNull())
			tmpitem->setData(Qt::DecorationRole, QIcon (":/resources/images/woodpecker.svg"));
		else
			tmpitem->setData(Qt::DecorationRole, twit->author()->avatar);
		ui->TwitList_->insertItem (0, tmpitem);
		ui->TwitList_->updateGeometry();
		
	}
//	QTimer::singleShot(1000, ui->TwitList_, SLOT(update()));
	
	ui->TwitList_->update();
	ui->TwitList_->setEnabled(true);
}

}
}
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
