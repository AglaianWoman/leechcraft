/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2006-2014  Georg Rudoy
 *
 * Boost Software License - Version 1.0 - August 17th, 2003
 *
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer,
 * must be included in all copies of the Software, in whole or in part, and
 * all derivative works of the Software, unless such copies or derivative
 * works are solely in the form of machine-executable object code generated by
 * a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************/

#include <stdexcept>
#include <numeric>
#include <algorithm>
#include <QtDebug>
#include <QImage>
#include <QDir>
#include <QDesktopServices>
#include <QUrl>
#include <QTimer>
#include <QTextCodec>
#include <QXmlStreamWriter>
#include <QNetworkReply>
#include <interfaces/iwebbrowser.h>
#include <interfaces/core/icoreproxy.h>
#include <interfaces/core/itagsmanager.h>
#include <interfaces/core/ipluginsmanager.h>
#include <interfaces/core/ientitymanager.h>
#include <util/models/mergemodel.h>
#include <util/xpc/util.h>
#include <util/sys/fileremoveguard.h>
#include <util/sys/paths.h>
#include <util/xpc/defaulthookproxy.h>
#include <util/shortcuts/shortcutmanager.h>
#include <util/sll/prelude.h>
#include <util/sll/qtutil.h>
#include <util/sll/visitor.h>
#include <util/sll/either.h>
#include "core.h"
#include "xmlsettingsmanager.h"
#include "parserfactory.h"
#include "rss20parser.h"
#include "rss10parser.h"
#include "rss091parser.h"
#include "atom10parser.h"
#include "atom03parser.h"
#include "channelsmodel.h"
#include "opmlparser.h"
#include "opmlwriter.h"
#include "sqlstoragebackend.h"
#include "jobholderrepresentation.h"
#include "channelsfiltermodel.h"
#include "importopml.h"
#include "addfeed.h"
#include "pluginmanager.h"
#include "dbupdatethread.h"
#include "dbupdatethreadworker.h"
#include "dumbstorage.h"
#include "storagebackendmanager.h"

namespace LeechCraft
{
namespace Aggregator
{
	Core::Core ()
	{
		qRegisterMetaType<IDType_t> ("IDType_t");
		qRegisterMetaType<QList<IDType_t>> ("QList<IDType_t>");
		qRegisterMetaType<QSet<IDType_t>> ("QSet<IDType_t>");
		qRegisterMetaType<QItemSelection> ("QItemSelection");
		qRegisterMetaType<Item> ("Item");
		qRegisterMetaType<ChannelShort> ("ChannelShort");
		qRegisterMetaType<Channel> ("Channel");
		qRegisterMetaType<channels_container_t> ("channels_container_t");
		qRegisterMetaTypeStreamOperators<Feed> ("LeechCraft::Plugins::Aggregator::Feed");
	}

	Core& Core::Instance ()
	{
		static Core core;
		return core;
	}

	void Core::Release ()
	{
		DBUpThread_.reset ();

		delete JobHolderRepresentation_;
		delete ChannelsFilterModel_;
		delete ChannelsModel_;

		StorageBackend_.reset ();

		XmlSettingsManager::Instance ()->Release ();
	}

	void Core::SetProxy (ICoreProxy_ptr proxy)
	{
		Proxy_ = proxy;
	}

	ICoreProxy_ptr Core::GetProxy () const
	{
		return Proxy_;
	}

	void Core::AddPlugin (QObject *plugin)
	{
		PluginManager_->AddPlugin (plugin);
	}

	Util::IDPool<IDType_t>& Core::GetPool (PoolType type)
	{
		return Pools_ [type];
	}

	bool Core::CouldHandle (const Entity& e)
	{
		if (!e.Entity_.canConvert<QUrl> () ||
				!Initialized_)
			return false;

		const auto& url = e.Entity_.toUrl ();

		if (e.Mime_ == "text/x-opml")
		{
			if (url.scheme () != "file" &&
					url.scheme () != "http" &&
					url.scheme () != "https" &&
					url.scheme () != "itpc")
				return false;
		}
		else if (e.Mime_ == "text/xml")
		{
			if (url.scheme () != "http" &&
					url.scheme () != "https")
				return false;

			const auto& pageData = e.Additional_ ["URLData"].toString ();
			QXmlStreamReader xmlReader (pageData);
			if (!xmlReader.readNextStartElement ())
				return false;
			return xmlReader.name () == "rss" || xmlReader.name () == "atom";
		}
		else
		{
			if (url.scheme () == "feed")
				return true;
			if (url.scheme () == "itpc")
				return true;
			if (url.scheme () != "http" &&
					url.scheme () != "https" &&
					url.scheme () != "itpc")
				return false;

			if (e.Mime_ != "application/atom+xml" &&
					e.Mime_ != "application/rss+xml")
				return false;

			const auto& linkRel = e.Additional_ ["LinkRel"].toString ();
			if (!linkRel.isEmpty () &&
					linkRel != "alternate")
				return false;
		}

		return true;
	}

	void Core::Handle (Entity e)
	{
		QUrl url = e.Entity_.toUrl ();
		if (e.Mime_ == "text/x-opml")
		{
			if (url.scheme () == "file")
				StartAddingOPML (url.toLocalFile ());
			else
			{
				const auto& name = Util::GetTemporaryName ();

				const auto& dlEntity = Util::MakeEntity (url,
						name,
						Internal |
							DoNotNotifyUser |
							DoNotSaveInHistory |
							NotPersistent |
							DoNotAnnounceEntity);

				const auto& handleResult = Proxy_->GetEntityManager ()->DelegateEntity (dlEntity);
				if (!handleResult)
				{
					ErrorNotification (tr ("Import error"),
							tr ("Could not find plugin to download OPML %1.")
								.arg (url.toString ()));
					return;
				}

				HandleProvider (handleResult.Handler_, handleResult.ID_);
				PendingOPMLs_ [handleResult.ID_] = PendingOPML { name };
			}

			const auto& s = e.Additional_;
			if (s.contains ("UpdateOnStartup"))
				XmlSettingsManager::Instance ()->setProperty ("UpdateOnStartup",
						s.value ("UpdateOnStartup").toBool ());
			if (s.contains ("UpdateTimeout"))
				XmlSettingsManager::Instance ()->setProperty ("UpdateInterval",
						s.value ("UpdateTimeout").toInt ());
			if (s.contains ("MaxArticles"))
				XmlSettingsManager::Instance ()->setProperty ("ItemsPerChannel",
						s.value ("MaxArticles").toInt ());
			if (s.contains ("MaxAge"))
				XmlSettingsManager::Instance ()->setProperty ("ItemsMaxAge",
						s.value ("MaxAge").toInt ());
		}
		else
		{
			QString str = url.toString ();
			if (str.startsWith ("feed://"))
				str.replace (0, 4, "http");
			else if (str.startsWith ("feed:"))
				str.remove  (0, 5);
			else if (str.startsWith ("itpc://"))
				str.replace (0, 4, "http");

			class AddFeed af { str };
			if (af.exec () == QDialog::Accepted)
				AddFeed (af.GetURL (),
						af.GetTags ());
		}
	}

	void Core::StartAddingOPML (const QString& file)
	{
		ImportOPML importDialog (file);
		if (importDialog.exec () == QDialog::Rejected)
			return;

		AddFromOPML (importDialog.GetFilename (),
				importDialog.GetTags (),
				importDialog.GetMask ());
	}

	bool Core::DoDelayedInit ()
	{
		bool result = true;
		ShortcutMgr_ = new Util::ShortcutManager (Proxy_, this);

		QDir dir = QDir::home ();
		if (!dir.cd (".leechcraft/aggregator") &&
				!dir.mkpath (".leechcraft/aggregator"))
		{
			qCritical () << Q_FUNC_INFO << "could not create necessary "
				"directories for Aggregator";
			result = false;
		}

		ChannelsModel_ = new ChannelsModel ();

		if (!ReinitStorage ())
			result = false;

		ChannelsFilterModel_ = new ChannelsFilterModel ();
		ChannelsFilterModel_->setSourceModel (ChannelsModel_);
		ChannelsFilterModel_->setFilterKeyColumn (0);

		JobHolderRepresentation_ = new JobHolderRepresentation ();

		DBUpThread_ = std::make_shared<DBUpdateThread> (Proxy_);
		DBUpThread_->SetAutoQuit (true);
		DBUpThread_->start (QThread::LowestPriority);
		DBUpThread_->ScheduleImpl (&DBUpdateThreadWorker::WithWorker,
				[this] (DBUpdateThreadWorker *worker)
				{
					connect (worker,
							&DBUpdateThreadWorker::hookGotNewItems,
							this,
							&Core::hookGotNewItems);
				});

		ParserFactory::Instance ().Register (&RSS20Parser::Instance ());
		ParserFactory::Instance ().Register (&Atom10Parser::Instance ());
		ParserFactory::Instance ().Register (&RSS091Parser::Instance ());
		ParserFactory::Instance ().Register (&Atom03Parser::Instance ());
		ParserFactory::Instance ().Register (&RSS10Parser::Instance ());

		JobHolderRepresentation_->setSourceModel (ChannelsModel_);

		CustomUpdateTimer_ = new QTimer (this);
		CustomUpdateTimer_->start (60 * 1000);
		connect (CustomUpdateTimer_,
				&QTimer::timeout,
				this,
				&Core::handleCustomUpdates);

		UpdateTimer_ = new QTimer (this);
		UpdateTimer_->setSingleShot (true);
		connect (UpdateTimer_,
				&QTimer::timeout,
				this,
				&Core::updateFeeds);

		auto now = QDateTime::currentDateTime ();
		auto lastUpdated = XmlSettingsManager::Instance ()->Property ("LastUpdateDateTime", now).toDateTime ();
		if (auto interval = XmlSettingsManager::Instance ()->property ("UpdateInterval").toInt ())
		{
			auto updateDiff = lastUpdated.secsTo (now);
			if (XmlSettingsManager::Instance ()->property ("UpdateOnStartup").toBool () ||
					updateDiff > interval * 60)
				QTimer::singleShot (7000,
						this,
						SLOT (updateFeeds ()));
			else
				UpdateTimer_->start (updateDiff * 1000);
		}

		XmlSettingsManager::Instance ()->RegisterObject ("UpdateInterval", this, "updateIntervalChanged");
		Initialized_ = true;

		PluginManager_ = new PluginManager ();
		PluginManager_->RegisterHookable (this);

		PluginManager_->RegisterHookable (StorageBackend_.get ());

		return result;
	}

	bool Core::ReinitStorage ()
	{
		const auto result = Util::Visit (StorageBackendManager::Instance ().CreatePrimaryStorage (),
				[this] (const StorageBackend_ptr& backend)
				{
					StorageBackend_ = backend;
					return true;
				},
				[this] (const auto& error)
				{
					ErrorNotification (tr ("Storage error"), error.Message_);
					return false;
				});

		if (!result)
			return false;

		Pools_.clear ();
		for (int type = 0; type < PTMAX; ++type)
		{
			Util::IDPool<IDType_t> pool;
			pool.SetID (StorageBackend_->GetHighestID (static_cast<PoolType> (type)) + 1);
			Pools_ [static_cast<PoolType> (type)] = pool;
		}

		return true;
	}

	void Core::AddFeed (const QString& url, const QString& tagString)
	{
		AddFeed (url, Proxy_->GetTagsManager ()->Split (tagString));
	}

	void Core::AddFeed (QString url, const QStringList& tags, const boost::optional<Feed::FeedSettings>& fs)
	{
		const auto& fixedUrl = QUrl::fromUserInput (url);
		url = fixedUrl.toString ();
		if (StorageBackend_->FindFeed (url) != static_cast<IDType_t> (-1))
		{
			ErrorNotification (tr ("Feed addition error"),
					tr ("The feed %1 is already added")
					.arg (url));
			return;
		}

		const auto& name = Util::GetTemporaryName ();
		const auto& e = Util::MakeEntity (fixedUrl,
				name,
				Internal |
					DoNotNotifyUser |
					DoNotSaveInHistory |
					NotPersistent |
					DoNotAnnounceEntity);

		const auto& tagIds = Proxy_->GetTagsManager ()->GetIDs (tags);
		PendingJob pj =
		{
			PendingJob::RFeedAdded,
			url,
			name,
			tagIds,
			fs
		};

		const auto& delegateResult = Proxy_->GetEntityManager ()->DelegateEntity (e);
		if (!delegateResult)
		{
			ErrorNotification (tr ("Plugin error"),
					tr ("Could not find plugin to download feed %1.")
						.arg (url),
					false);
			return;
		}

		HandleProvider (delegateResult.Handler_, delegateResult.ID_);
		PendingJobs_ [delegateResult.ID_] = pj;
	}

	void Core::RenameFeed (const QModelIndex& index, const QString& newName)
	{
		if (!index.isValid ())
			return;

		auto channel = ChannelsModel_->GetChannelForIndex (index);
		channel.DisplayTitle_ = newName;
		StorageBackend_->UpdateChannel (channel);
	}

	Util::ShortcutManager* Core::GetShortcutManager () const
	{
		return ShortcutMgr_;
	}

	ChannelsModel* Core::GetRawChannelsModel () const
	{
		return ChannelsModel_;
	}

	QSortFilterProxyModel* Core::GetChannelsModel () const
	{
		return ChannelsFilterModel_;
	}

	IWebBrowser* Core::GetWebBrowser () const
	{
		return Proxy_->GetPluginsManager ()->GetAllCastableTo<IWebBrowser*> ().value (0);
	}

	void Core::MarkChannelAsRead (const QModelIndex& i)
	{
		MarkChannel (i, false);
	}

	void Core::MarkChannelAsUnread (const QModelIndex& i)
	{
		MarkChannel (i, true);
	}

	Core::ChannelInfo Core::GetChannelInfo (const QModelIndex& i) const
	{
		ChannelShort channel;
		try
		{
			channel = ChannelsModel_->GetChannelForIndex (i);
		}
		catch (const std::exception& e)
		{
			qWarning () << Q_FUNC_INFO
				<< e.what ();
			return ChannelInfo ();
		}
		ChannelInfo ci;
		ci.FeedID_ = channel.FeedID_;
		ci.ChannelID_ = channel.ChannelID_;
		ci.Link_ = channel.Link_;

		using Util::operator*;

		StorageBackend_->GetChannel (channel.ChannelID_) * [&] (auto&& rc)
		{
			ci.Description_ = rc.Description_;
			ci.Author_ = rc.Author_;
		};

		StorageBackend_->GetFeed (channel.FeedID_) * [&] (auto&& feed) { ci.URL_ = feed.URL_; };

		// TODO introduce a method in SB for this
		ci.NumItems_ = StorageBackend_->GetItems (channel.ChannelID_).size ();

		return ci;
	}

	QPixmap Core::GetChannelPixmap (const QModelIndex& i) const
	{
		// TODO introduce a method in SB for this
		const auto& channelShort = ChannelsModel_->GetChannelForIndex (i);
		if (const auto& maybeChan = StorageBackend_->GetChannel (channelShort.ChannelID_))
			return QPixmap::fromImage (maybeChan->Pixmap_);
		else
			return {};
	}

	void Core::SetTagsForIndex (const QString& tags, const QModelIndex& index)
	{
		auto channel = ChannelsModel_->GetChannelForIndex (index);
		channel.Tags_ = Proxy_->GetTagsManager ()->GetIDs (Proxy_->GetTagsManager ()->Split (tags));
		StorageBackend_->UpdateChannel (channel);
	}

	void Core::UpdateFavicon (const QModelIndex& index)
	{
		const auto& channel = ChannelsModel_->GetChannelForIndex (index);
		// TODO no need to get full channel here
		if (const auto& maybeChan = StorageBackend_->GetChannel (channel.ChannelID_))
			FetchFavicon (*maybeChan);
	}

	QStringList Core::GetCategories (const QModelIndex& index) const
	{
		const auto& cs = ChannelsModel_->GetChannelForIndex (index);
		return GetCategories (StorageBackend_->GetItems (cs.ChannelID_));
	}

	QStringList Core::GetCategories (const items_shorts_t& items) const
	{
		QSet<QString> unique;
		for (const auto& item : items)
			for (const auto& category : item.Categories_)
				unique << category;

		auto result = unique.toList ();
		std::sort (result.begin (), result.end ());
		return result;
	}

	void Core::UpdateFeed (const QModelIndex& si)
	{
		QModelIndex index = si;

		ChannelShort channel;
		try
		{
			channel = ChannelsModel_->GetChannelForIndex (index);
		}
		catch (const std::exception& e)
		{
			qWarning () << Q_FUNC_INFO
				<< e.what ()
				<< si
				<< index;
			ErrorNotification (tr ("Feed update error"),
					tr ("Could not update feed"),
					false);
			return;
		}
		UpdateFeed (channel.FeedID_);
	}

	void Core::AddFromOPML (const QString& filename,
			const QString& tags,
			const std::vector<bool>& mask)
	{
		QFile file (filename);
		if (!file.open (QIODevice::ReadOnly))
		{
			ErrorNotification (tr ("OPML import error"),
					tr ("Could not open file %1 for reading.")
						.arg (filename));
			return;
		}

		QByteArray data = file.readAll ();
		file.close ();

		QString errorMsg;
		int errorLine, errorColumn;
		QDomDocument document;
		if (!document.setContent (data,
					true,
					&errorMsg,
					&errorLine,
					&errorColumn))
		{
			ErrorNotification (tr ("OPML import error"),
					tr ("XML error, file %1, line %2, column %3, error:<br />%4")
						.arg (filename)
						.arg (errorLine)
						.arg (errorColumn)
						.arg (errorMsg));
			return;
		}

		OPMLParser parser (document);
		if (!parser.IsValid ())
		{
			ErrorNotification (tr ("OPML import error"),
					tr ("OPML from file %1 is not valid.")
						.arg (filename));
			return;
		}

		OPMLParser::items_container_t items = parser.Parse ();
		for (std::vector<bool>::const_iterator begin = mask.begin (),
				i = mask.end () - 1; i >= begin; --i)
			if (!*i)
			{
				size_t distance = std::distance (mask.begin (), i);
				OPMLParser::items_container_t::iterator eraser = items.begin ();
				std::advance (eraser, distance);
				items.erase (eraser);
			}

		QStringList tagsList = Proxy_->GetTagsManager ()->Split (tags);
		for (OPMLParser::items_container_t::const_iterator i = items.begin (),
				end = items.end (); i != end; ++i)
		{
			int interval = 0;
			if (i->CustomFetchInterval_)
				interval = i->FetchInterval_;
			AddFeed (i->URL_, tagsList + i->Categories_,
					{ { IDNotFound, interval, i->MaxArticleNumber_, i->MaxArticleAge_, false } });
		}
	}

	void Core::ExportToOPML (const QString& where,
			const QString& title,
			const QString& owner,
			const QString& ownerEmail,
			const std::vector<bool>& mask) const
	{
		auto channels = GetChannels ();

		for (std::vector<bool>::const_iterator begin = mask.begin (),
				i = mask.end () - 1; i >= begin; --i)
			if (!*i)
			{
				size_t distance = std::distance (mask.begin (), i);
				channels_shorts_t::iterator eraser = channels.begin ();
				std::advance (eraser, distance);
				channels.erase (eraser);
			}

		OPMLWriter writer;
		QString data = writer.Write (channels, title, owner, ownerEmail);

		QFile f (where);
		if (!f.open (QIODevice::WriteOnly))
		{
			ErrorNotification (tr ("OPML export error"),
					tr ("Could not open file %1 for write.").arg (where));
			return;
		}

		f.write (data.toUtf8 ());
		f.close ();
	}

	void Core::ExportToBinary (const QString& where,
			const QString& title,
			const QString& owner,
			const QString& ownerEmail,
			const std::vector<bool>& mask) const
	{
		auto channels = GetChannels ();

		for (std::vector<bool>::const_iterator begin = mask.begin (),
				i = mask.end () - 1; i >= begin; --i)
			if (!*i)
			{
				size_t distance = std::distance (mask.begin (), i);
				channels_shorts_t::iterator eraser = channels.begin ();
				std::advance (eraser, distance);
				channels.erase (eraser);
			}

		QFile f (where);
		if (!f.open (QIODevice::WriteOnly))
		{
			ErrorNotification (tr ("Binary export error"),
					tr ("Could not open file %1 for write.").arg (where));
			return;
		}

		QByteArray buffer;
		QDataStream data (&buffer, QIODevice::WriteOnly);

		int version = 1;
		int magic = 0xd34df00d;
		data << magic
				<< version
				<< title
				<< owner
				<< ownerEmail;

		for (channels_shorts_t::const_iterator i = channels.begin (), end = channels.end (); i != end; ++i)
			if (const auto& maybeChannel = StorageBackend_->GetChannel (i->ChannelID_))
			{
				auto channel = *maybeChannel;
				channel.Items_ = StorageBackend_->GetFullItems (channel.ChannelID_);
				data << channel;
			}

		f.write (qCompress (buffer, 9));
	}

	JobHolderRepresentation* Core::GetJobHolderRepresentation () const
	{
		return JobHolderRepresentation_;
	}

	channels_shorts_t Core::GetChannels () const
	{
		channels_shorts_t result;
		for (const auto id : StorageBackend_->GetFeedsIDs ())
		{
			auto feedChannels = StorageBackend_->GetChannels (id);
			std::move (feedChannels.begin (), feedChannels.end (), std::back_inserter (result));
		}
		return result;
	}

	void Core::AddFeeds (const feeds_container_t& feeds, const QString& tagsString)
	{
		auto tags = Proxy_->GetTagsManager ()->Split (tagsString);
		tags.removeDuplicates ();

		for (const auto feed : feeds)
		{
			for (const auto channel : feed->Channels_)
			{
				channel->Tags_ += tags;
				channel->Tags_.removeDuplicates ();
			}

			StorageBackend_->AddFeed (*feed);
		}
	}

	void Core::openLink (const QString& url)
	{
		IWebBrowser *browser = GetWebBrowser ();
		if (!browser ||
				XmlSettingsManager::Instance ()->
					property ("AlwaysUseExternalBrowser").toBool ())
		{
			QDesktopServices::openUrl (QUrl (url));
			return;
		}

		browser->Open (url);
	}

	void Core::handleJobFinished (int id)
	{
		if (!PendingJobs_.contains (id))
		{
			if (PendingOPMLs_.contains (id))
			{
				StartAddingOPML (PendingOPMLs_ [id].Filename_);
				PendingOPMLs_.remove (id);
			}
			return;
		}
		PendingJob pj = PendingJobs_ [id];
		PendingJobs_.remove (id);
		ID2Downloader_.remove (id);

		Util::FileRemoveGuard file (pj.Filename_);
		if (!file.open (QIODevice::ReadOnly))
		{
			qWarning () << Q_FUNC_INFO << "could not open file for pj " << pj.Filename_;
			return;
		}
		if (!file.size ())
		{
			if (pj.Role_ != PendingJob::RFeedExternalData)
				ErrorNotification (tr ("Feed error"),
						tr ("Downloaded file from url %1 has null size.").arg (pj.URL_));
			return;
		}

		channels_container_t channels;
		if (pj.Role_ != PendingJob::RFeedExternalData)
		{
			QByteArray data = file.readAll ();
			QDomDocument doc;
			QString errorMsg;
			int errorLine, errorColumn;
			if (!doc.setContent (data, true, &errorMsg, &errorLine, &errorColumn))
			{
				file.copy (QDir::tempPath () + "/failedFile.xml");
				ErrorNotification (tr ("Feed error"),
						tr ("XML file parse error: %1, line %2, column %3, filename %4, from %5")
						.arg (errorMsg)
						.arg (errorLine)
						.arg (errorColumn)
						.arg (pj.Filename_)
						.arg (pj.URL_));
				return;
			}

			Parser *parser = ParserFactory::Instance ().Return (doc);
			if (!parser)
			{
				file.copy (QDir::tempPath () + "/failedFile.xml");
				ErrorNotification (tr ("Feed error"),
						tr ("Could not find parser to parse file %1 from %2")
						.arg (pj.Filename_)
						.arg (pj.URL_));
				return;
			}

			IDType_t feedId = IDNotFound;
			if (pj.Role_ == PendingJob::RFeedAdded)
			{
				Feed feed;
				feed.URL_ = pj.URL_;
				StorageBackend_->AddFeed (feed);
				feedId = feed.FeedID_;
			}
			else
				feedId = StorageBackend_->FindFeed (pj.URL_);

			if (feedId == IDNotFound)
			{
				ErrorNotification (tr ("Feed error"),
						tr ("Feed with url %1 not found.").arg (pj.URL_));
				return;
			}

			channels = parser->ParseFeed (doc, feedId);
		}

		if (pj.Role_ == PendingJob::RFeedAdded)
			HandleFeedAdded (channels, pj);
		else if (pj.Role_ == PendingJob::RFeedUpdated)
			HandleFeedUpdated (channels, pj);
		else if (pj.Role_ == PendingJob::RFeedExternalData)
			HandleExternalData (pj.URL_, file);
	}

	void Core::handleJobRemoved (int id)
	{
		if (PendingJobs_.contains (id))
		{
			PendingJobs_.remove (id);
			ID2Downloader_.remove (id);
		}
		if (PendingOPMLs_.contains (id))
			PendingOPMLs_.remove (id);
	}

	void Core::handleJobError (int id, IDownload::Error ie)
	{
		if (!PendingJobs_.contains (id))
		{
			if (PendingOPMLs_.contains (id))
				ErrorNotification (tr ("OPML import error"),
						tr ("Unable to download the OPML file."));
			return;
		}

		PendingJob pj = PendingJobs_ [id];
		Util::FileRemoveGuard file (pj.Filename_);

		if ((!XmlSettingsManager::Instance ()->property ("BeSilent").toBool () &&
					pj.Role_ == PendingJob::RFeedUpdated) ||
				pj.Role_ == PendingJob::RFeedAdded)
		{
			QString msg;
			switch (ie)
			{
				case IDownload::ENotFound:
					msg = tr ("Address not found:<br />%1");
					break;
				case IDownload::EAccessDenied:
					msg = tr ("Access denied:<br />%1");
					break;
				case IDownload::ELocalError:
					msg = tr ("Local error for:<br />%1");
					break;
				default:
					msg = tr ("Unknown error for:<br />%1");
					break;
			}
			ErrorNotification (tr ("Download error"),
					msg.arg (pj.URL_));
		}
		PendingJobs_.remove (id);
		ID2Downloader_.remove (id);
	}

	void Core::updateFeeds ()
	{
		for (const auto id : StorageBackend_->GetFeedsIDs ())
		{
			// It's handled by custom timer.
			if (StorageBackend_->GetFeedSettings (id).value_or (Feed::FeedSettings {}).UpdateTimeout_)
				continue;

			UpdateFeed (id);
		}
		XmlSettingsManager::Instance ()->setProperty ("LastUpdateDateTime", QDateTime::currentDateTime ());
		if (int interval = XmlSettingsManager::Instance ()->property ("UpdateInterval").toInt ())
			UpdateTimer_->start (interval * 60 * 1000);
	}

	void Core::fetchExternalFile (const QString& url, const QString& where)
	{
		const auto& e = Util::MakeEntity (QUrl (url),
				where,
				Internal |
					DoNotNotifyUser |
					DoNotSaveInHistory |
					NotPersistent |
					DoNotAnnounceEntity);

		PendingJob pj =
		{
			PendingJob::RFeedExternalData,
			url,
			where,
			{},
			{}
		};

		const auto& delegateResult = Proxy_->GetEntityManager ()->DelegateEntity (e);
		if (!delegateResult)
		{
			ErrorNotification (tr ("Feed error"),
					tr ("Could not find plugin to download external file %1.").arg (url));
			return;
		}

		HandleProvider (delegateResult.Handler_, delegateResult.ID_);
		PendingJobs_ [delegateResult.ID_] = pj;
	}

	void Core::updateIntervalChanged ()
	{
		int min = XmlSettingsManager::Instance ()->
			property ("UpdateInterval").toInt ();
		if (min)
		{
			if (UpdateTimer_->isActive ())
				UpdateTimer_->setInterval (min * 60 * 1000);
			else
				UpdateTimer_->start (min * 60 * 1000);
		}
		else
			UpdateTimer_->stop ();
	}

	void Core::handleSslError (QNetworkReply *reply)
	{
		reply->ignoreSslErrors ();
	}

	void Core::handleCustomUpdates ()
	{
		using Util::operator*;

		QDateTime current = QDateTime::currentDateTime ();
		for (const auto id : StorageBackend_->GetFeedsIDs ())
		{
			const auto ut = (StorageBackend_->GetFeedSettings (id) * &Feed::FeedSettings::UpdateTimeout_).value_or (0);

			// It's handled by normal timer.
			if (!ut)
				continue;

			if (!Updates_.contains (id) ||
					(Updates_ [id].isValid () &&
						Updates_ [id].secsTo (current) / 60 > ut))
				UpdateFeed (id);
		}
	}

	void Core::rotateUpdatesQueue ()
	{
		if (UpdatesQueue_.isEmpty ())
			return;

		const IDType_t id = UpdatesQueue_.takeFirst ();

		if (!UpdatesQueue_.isEmpty ())
			QTimer::singleShot (2000,
					this,
					SLOT (rotateUpdatesQueue ()));

		const auto& maybeFeed = StorageBackend_->GetFeed (id);
		if (!maybeFeed)
		{
			qWarning () << Q_FUNC_INFO
					<< "no feed for id"
					<< id;
			return;
		}

		const auto& url = maybeFeed->URL_;
		for (const auto& pair : Util::Stlize (PendingJobs_))
			if (pair.second.URL_ == url)
			{
				const auto id = pair.first;
				QObject *provider = ID2Downloader_ [id];
				IDownload *downloader = qobject_cast<IDownload*> (provider);
				if (downloader)
				{
					qWarning () << Q_FUNC_INFO
						<< "stalled task detected from"
						<< downloader
						<< "trying to kill...";

					downloader->KillTask (id);
					ID2Downloader_.remove (id);
					qWarning () << Q_FUNC_INFO
						<< "killed!";
				}
				else
					qWarning () << Q_FUNC_INFO
						<< "provider is not a downloader:"
						<< provider
						<< "; cannot kill the task";
			}
		PendingJobs_.clear ();

		QString filename = Util::GetTemporaryName ();

		Entity e = Util::MakeEntity (QUrl (url),
				filename,
				Internal |
					DoNotNotifyUser |
					DoNotSaveInHistory |
					NotPersistent |
					DoNotAnnounceEntity);

		PendingJob pj =
		{
			PendingJob::RFeedUpdated,
			url,
			filename,
			{},
			{}
		};

		const auto& delegateResult = Proxy_->GetEntityManager ()->DelegateEntity (e);
		if (!delegateResult)
		{
			ErrorNotification ("Aggregator",
					tr ("Could not find plugin for feed with URL %1")
						.arg (url));
			return;
		}

		HandleProvider (delegateResult.Handler_, delegateResult.ID_);
		PendingJobs_ [delegateResult.ID_] = pj;
		Updates_ [id] = QDateTime::currentDateTime ();
	}

	void Core::FetchPixmap (const Channel& channel)
	{
		if (QUrl (channel.PixmapURL_).isValid () &&
				!QUrl (channel.PixmapURL_).isRelative ())
		{
			ExternalData data;
			data.Type_ = ExternalData::TImage;
			data.ChannelId_ = channel.ChannelID_;
			QString exFName = LeechCraft::Util::GetTemporaryName ();
			try
			{
				fetchExternalFile (channel.PixmapURL_, exFName);
			}
			catch (const std::runtime_error& e)
			{
				qWarning () << Q_FUNC_INFO << e.what ();
				return;
			}
			PendingJob2ExternalData_ [channel.PixmapURL_] = data;
		}
	}

	void Core::FetchFavicon (const Channel& channel)
	{
		QUrl oldUrl (channel.Link_);
		oldUrl.setPath ("/favicon.ico");
		QString iconUrl = oldUrl.toString ();

		ExternalData data;
		data.Type_ = ExternalData::TIcon;
		data.ChannelId_ = channel.ChannelID_;
		QString exFName = LeechCraft::Util::GetTemporaryName ();
		try
		{
			fetchExternalFile (iconUrl, exFName);
		}
		catch (const std::runtime_error& e)
		{
			qWarning () << Q_FUNC_INFO << e.what ();
			return;
		}
		PendingJob2ExternalData_ [iconUrl] = data;
	}

	void Core::HandleExternalData (const QString& url, const QFile& file)
	{
		const auto& data = PendingJob2ExternalData_.take (url);

		// TODO add separate methods for pixmap/favicon updates in StorageBackend.
		auto maybeChannel = StorageBackend_->GetChannel (data.ChannelId_);
		if (!maybeChannel)
			return;

		auto channel = *maybeChannel;

		const QImage image { file.fileName () };
		switch (data.Type_)
		{
		case ExternalData::TImage:
			channel.Pixmap_ = image;
			break;
		case ExternalData::TIcon:
			channel.Favicon_ = image.scaled (16, 16);
			break;
		}

		StorageBackend_->UpdateChannel (channel);
	}

	void Core::HandleFeedAdded (const channels_container_t& channels,
			const Core::PendingJob& pj)
	{
		for (const auto& channel : channels)
		{
			for (const auto& item : channel->Items_)
				item->FixDate ();

			channel->Tags_ = pj.Tags_;
			StorageBackend_->AddChannel (*channel);

			emit hookGotNewItems (std::make_shared<Util::DefaultHookProxy> (),
					Util::Map (channel->Items_, [] (const Item_ptr& item) { return *item; }));

			FetchPixmap (*channel);
			FetchFavicon (*channel);
		}

		if (pj.FeedSettings_)
		{
			auto fs = *pj.FeedSettings_;
			fs.FeedID_ = StorageBackend_->FindFeed (pj.URL_);
			StorageBackend_->SetFeedSettings (fs);
		}
	}

	void Core::HandleFeedUpdated (const channels_container_t& channels,
			const Core::PendingJob& pj)
	{
		DBUpThread_->ScheduleImpl (&DBUpdateThreadWorker::updateFeed,
				channels,
				pj.URL_);
	}

	void Core::MarkChannel (const QModelIndex& i, bool state)
	{
		try
		{
			ChannelShort cs = ChannelsModel_->GetChannelForIndex (i);
			DBUpThread_->ScheduleImpl (&DBUpdateThreadWorker::toggleChannelUnread,
					cs.ChannelID_,
					state);
		}
		catch (const std::exception& e)
		{
			qWarning () << Q_FUNC_INFO
				<< e.what ();
			ErrorNotification (tr ("Aggregator error"),
					tr ("Could not mark channel"));
		}
	}

	void Core::UpdateFeed (const IDType_t& id)
	{
		if (UpdatesQueue_.isEmpty ())
			QTimer::singleShot (500,
					this,
					SLOT (rotateUpdatesQueue ()));

		UpdatesQueue_ << id;
	}

	void Core::HandleProvider (QObject *provider, int id)
	{
		ID2Downloader_ [id] = provider;

		if (Downloaders_.contains (provider))
			return;

		Downloaders_ << provider;
		connect (provider,
				SIGNAL (jobFinished (int)),
				this,
				SLOT (handleJobFinished (int)));
		connect (provider,
				SIGNAL (jobRemoved (int)),
				this,
				SLOT (handleJobRemoved (int)));
		connect (provider,
				SIGNAL (jobError (int, IDownload::Error)),
				this,
				SLOT (handleJobError (int, IDownload::Error)));
	}

	void Core::ErrorNotification (const QString& h, const QString& body, bool wait) const
	{
		auto e = Util::MakeNotification (h, body, Priority::Critical);
		e.Additional_ ["UntilUserSees"] = wait;
		Proxy_->GetEntityManager ()->HandleEntity (e);
	}
}
}
