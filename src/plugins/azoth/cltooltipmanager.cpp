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

#include "cltooltipmanager.h"
#include <QTextDocument>
#include <QStandardItem>
#include <util/util.h>
#include <util/xpc/defaulthookproxy.h>
#include <interfaces/media/audiostructs.h>
#include <interfaces/azoth/iclentry.h>
#include <interfaces/azoth/imucperms.h>
#include <interfaces/azoth/iproxyobject.h>
#include "interfaces/azoth/iadvancedclentry.h"
#include "interfaces/azoth/ihaveentitytime.h"
#include "interfaces/azoth/ihavecontacttune.h"
#include "interfaces/azoth/ihavecontactmood.h"
#include "core.h"
#include "activitydialog.h"
#include "mooddialog.h"
#include "proxyobject.h"
#include "resourcesmanager.h"

namespace LeechCraft
{
namespace Azoth
{
	namespace
	{
		QString Escape (const QString& str)
		{
#if QT_VERSION < 0x050000
			return Qt::escape (str);
#else
			return str.toHtmlEscaped ();
#endif
		}
	}

	CLTooltipManager::CLTooltipManager (Entry2Items_t& items)
	: Entry2Items_ (items)
	, Avatar2TooltipSrcCache_ { 2 * 1024 * 1024 }
	{
	}

	QString CLTooltipManager::Status2Str (const EntryStatus& status, IProxyObject *obj)
	{
		auto result = "<table><tr><td valign='middle'>" + obj->StateToString (status.State_);
		const QString& statusString = Escape (status.StatusString_);
		if (!statusString.isEmpty ())
			result += " (" + statusString + ")";

		const auto& icon = ResourcesManager::Instance ().GetIconForState (status.State_);
		const auto& data = Util::GetAsBase64Src (icon.pixmap (16, 16).toImage ());
		result += "&nbsp;&nbsp;&nbsp;</td><td><img src='" + data + "' /></td></tr></table>";

		return result;
	}

	void CLTooltipManager::AddEntry (ICLEntry *clEntry)
	{
		DirtyTooltips_ << clEntry;

		connect (clEntry->GetQObject (),
				SIGNAL (statusChanged (EntryStatus, QString)),
				this,
				SLOT (remakeTooltipForSender ()));
		connect (clEntry->GetQObject (),
				SIGNAL (availableVariantsChanged (QStringList)),
				this,
				SLOT (remakeTooltipForSender ()));
		connect (clEntry->GetQObject (),
				SIGNAL (entryGenerallyChanged ()),
				this,
				SLOT (remakeTooltipForSender ()));
		connect (clEntry->GetQObject (),
				SIGNAL (nameChanged (const QString&)),
				this,
				SLOT (remakeTooltipForSender ()));
		connect (clEntry->GetQObject (),
				SIGNAL (groupsChanged (const QStringList&)),
				this,
				SLOT (remakeTooltipForSender ()));
		connect (clEntry->GetQObject (),
				SIGNAL (avatarChanged (const QImage&)),
				this,
				SLOT (remakeTooltipForSender ()));
		connect (clEntry->GetQObject (),
				SIGNAL (permsChanged ()),
				this,
				SLOT (remakeTooltipForSender ()));

		if (qobject_cast<IAdvancedCLEntry*> (clEntry->GetQObject ()))
		{
			connect (clEntry->GetQObject (),
					SIGNAL (attentionDrawn (const QString&, const QString&)),
					this,
					SLOT (remakeTooltipForSender ()));
			connect (clEntry->GetQObject (),
					SIGNAL (activityChanged (const QString&)),
					this,
					SLOT (remakeTooltipForSender ()));
			connect (clEntry->GetQObject (),
					SIGNAL (locationChanged (const QString&)),
					this,
					SLOT (remakeTooltipForSender ()));
		}

		if (qobject_cast<IHaveContactTune*> (clEntry->GetQObject ()))
			connect (clEntry->GetQObject (),
					SIGNAL (tuneChanged (QString)),
					this,
					SLOT (remakeTooltipForSender ()));

		if (qobject_cast<IHaveContactMood*> (clEntry->GetQObject ()))
			connect (clEntry->GetQObject (),
					SIGNAL (moodChanged (QString)),
					this,
					SLOT (remakeTooltipForSender ()));
	}

	void CLTooltipManager::RemoveEntry (ICLEntry *entry)
	{
		disconnect (entry->GetQObject (),
				0,
				this,
				0);
		DirtyTooltips_.remove (entry);
	}

	namespace
	{
		void FormatMood (QString& tip, const MoodInfo& moodInfo)
		{
			tip += "<br />" + Core::tr ("Mood:") + ' ';
			tip += MoodDialog::ToHumanReadable (moodInfo.Mood_);
			if (!moodInfo.Text_.isEmpty ())
				tip += " (" + moodInfo.Text_ + ")";
		}

		void FormatActivity (QString& tip, const QMap<QString, QVariant>& actInfo)
		{
			tip += "<br />" + Core::tr ("Activity:") + ' ';
			tip += ActivityDialog::ToHumanReadable (actInfo ["general"].toString ());
			const QString& specific = ActivityDialog::ToHumanReadable (actInfo ["specific"].toString ());
			if (!specific.isEmpty ())
				tip += " (" + specific + ")";
			const QString& text = actInfo ["text"].toString ();
			if (!text.isEmpty ())
				tip += " (" + text + ")";
		}

		void FormatTune (QString& tip, const Media::AudioInfo& tuneInfo)
		{
			const auto& artist = tuneInfo.Artist_;
			const auto& source = tuneInfo.Album_;
			const auto& title = tuneInfo.Title_;

			tip += "<br />" + Core::tr ("Now listening to:") + ' ';
			if (!artist.isEmpty () && !title.isEmpty ())
				tip += "<em>" + artist + "</em>" +
						QString::fromUtf8 (" — ") +
						"<em>" + title + "</em>";
			else if (!artist.isEmpty ())
				tip += "<em>" + artist + "</em>";
			else if (!title.isEmpty ())
				tip += "<em>" + title + "</em>";

			if (!source.isEmpty ())
				tip += ' ' + Core::tr ("from") +
						" <em>" + source + "</em>";

			if (tuneInfo.Length_)
				tip += " (" + Util::MakeTimeFromLong (tuneInfo.Length_) + ")";
		}

		void FormatMucPerms (QString& tip, IMUCPerms *mucPerms, ICLEntry *entry)
		{
			if (!mucPerms)
				return;

			tip += "<hr />";
			const auto& perms = mucPerms->GetPerms (entry->GetQObject ());
			for (const auto& permClass : perms.keys ())
			{
				tip += mucPerms->GetUserString (permClass);
				tip += ": ";

				QStringList users;
				for (const auto& perm : perms [permClass])
					users << mucPerms->GetUserString (perm);
				tip += users.join ("; ");
				tip += "<br />";
			}
		}
	}

	QString CLTooltipManager::MakeTooltipString (ICLEntry *entry)
	{
		QString tip = "<table border='0'><tr><td>";

		const auto& icons = ResourcesManager::Instance ().GetClientIconForEntry (entry);

		if (entry->GetEntryType () != ICLEntry::EntryType::MUC)
		{
			const int avatarSize = 75;

			auto avatar = entry->GetAvatar ();
			if (avatar.isNull ())
				avatar = ResourcesManager::Instance ().GetDefaultAvatar (avatarSize);

			QString data;
			if (auto dataPtr = Avatar2TooltipSrcCache_ [avatar])
				data = *dataPtr;
			else
			{
				const int minAvatarSize = 32;

				if (std::max (avatar.width (), avatar.height ()) > avatarSize)
					avatar = avatar.scaled (avatarSize, avatarSize, Qt::KeepAspectRatio, Qt::FastTransformation);
				else if (std::max (avatar.width (), avatar.height ()) < minAvatarSize)
					avatar = avatar.scaled (minAvatarSize, minAvatarSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

				data = Util::GetAsBase64Src (avatar);
				Avatar2TooltipSrcCache_.insert (avatar, new QString (data), data.size ());
			}

			tip += "<img src='" + data + "' />";
			tip += "</td><td>";
		}

		tip += "<strong>" + Escape (entry->GetEntryName ()) + "</strong>";
		tip += "&nbsp;(<em>" + Escape (entry->GetHumanReadableID ()) + "</em>)";
		tip += Status2Str (entry->GetStatus (), Core::Instance ().GetPluginProxy ());
		if (entry->GetEntryType () != ICLEntry::EntryType::PrivateChat)
		{
			tip += "<br />";
			tip += tr ("In groups:") + ' ' + Escape (entry->Groups ().join ("; "));
		}

		const QStringList& variants = entry->Variants ();

		if (auto mucEntry = qobject_cast<IMUCEntry*> (entry->GetParentCLEntryObject ()))
		{
			const QString& jid = mucEntry->GetRealID (entry->GetQObject ());
			tip += "<br />" + tr ("Real ID:") + ' ';
			tip += jid.isEmpty () ? tr ("unknown") : Escape (jid);
		}

		FormatMucPerms (tip,
				qobject_cast<IMUCPerms*> (entry->GetParentCLEntryObject ()),
				entry);

		Util::DefaultHookProxy_ptr proxy (new Util::DefaultHookProxy);
		proxy->SetValue ("tooltip", tip);
		emit hookTooltipBeforeVariants (proxy, entry->GetQObject ());
		proxy->FillValue ("tooltip", tip);

		auto cleanupBR = [&tip] ()
		{
			tip = tip.trimmed ();
			while (tip.endsWith ("<br />"))
			{
				tip.chop (6);
				tip = tip.trimmed ();
			}
		};

		cleanupBR ();

		const auto iHaveTune = qobject_cast<IHaveContactTune*> (entry->GetQObject ());
		const auto iHaveMood = qobject_cast<IHaveContactMood*> (entry->GetQObject ());
		for (const auto& variant : variants)
		{
			const auto& info = entry->GetClientInfo (variant);

			tip += "<hr />";
			if (!variant.isEmpty ())
			{
				tip += "<strong>" + variant;
				if (info.contains ("priority"))
					tip += " (" + QString::number (info.value ("priority").toInt ()) + ")";
				tip += "</strong>";
			}
			if (!variant.isEmpty () || variants.size () > 1)
				tip += Status2Str (entry->GetStatus (variant), Core::Instance ().GetPluginProxy ());

			QString clientIconString;
			if (!icons.value (variant).isNull ())
			{
				const auto& data = Util::GetAsBase64Src (icons.value (variant).pixmap (16, 16).toImage ());
				clientIconString = "&nbsp;&nbsp;&nbsp;<img src='" + data + "'/>";
			}

			bool clientIconInserted = false;

			if (info.contains ("client_name"))
			{
				tip += "<br />" + tr ("Using:") + ' ' + Escape (info.value ("client_name").toString ());

				if (!info.contains ("client_version"))
				{
					tip += clientIconString;
					clientIconInserted = true;
				}
			}
			if (info.contains ("client_version"))
			{
				tip += " " + Escape (info.value ("client_version").toString ());

				tip += clientIconString;
				clientIconInserted = true;
			}
			if (info.contains ("client_remote_name"))
			{
				tip += "<br />" + tr ("Claiming:") + ' ' + Escape (info.value ("client_remote_name").toString ());

				if (!clientIconInserted)
				{
					tip += clientIconString;
					clientIconInserted = true;
				}
			}
			if (info.contains ("client_os"))
				tip += "<br />" + tr ("OS:") + ' ' + Escape (info.value ("client_os").toString ());

			if (info.contains ("client_time"))
			{
				const auto& datetime = info.value ("client_time").toDateTime ();
				const auto& dateStr = ProxyObject {}.PrettyPrintDateTime (datetime);
				tip += "<br />" + tr ("Client time:") + ' ' + dateStr;
			}

			if (info.contains ("user_activity"))
				FormatActivity (tip, info ["user_activity"].toMap ());
			if (iHaveMood)
				FormatMood (tip, iHaveMood->GetUserMood (variant));
			if (iHaveTune)
				FormatTune (tip, iHaveTune->GetUserTune (variant));

			if (info.contains ("custom_user_visible_map"))
			{
				const auto& map = info ["custom_user_visible_map"].toMap ();
				for (const auto& key : map.keys ())
					tip += "<br />" + key + ": " + Escape (map [key].toString ()) + "<br />";
			}
		}

		cleanupBR ();

		tip += "</td></tr></table>";

		return tip;
	}

	void CLTooltipManager::RebuildTooltip (ICLEntry *entry)
	{
		if (const auto ihet = qobject_cast<IHaveEntityTime*> (entry->GetQObject ()))
		{
			ihet->UpdateEntityTime ();

			for (const auto& var : entry->Variants ())
				if (entry->GetClientInfo (var).contains ("client_time"))
				{
					DirtyTooltips_ << entry;
					break;
				}
		}

		if (!DirtyTooltips_.contains (entry))
			return;

		const auto& tip = MakeTooltipString (entry);
		for (auto item : Entry2Items_.value (entry))
			item->setToolTip (tip);

		DirtyTooltips_.remove (entry);
	}

	void CLTooltipManager::remakeTooltipForSender ()
	{
		ICLEntry *entry = qobject_cast<ICLEntry*> (sender ());
		if (!entry)
		{
			qWarning () << Q_FUNC_INFO
					<< sender ()
					<< "could not be casted to ICLEntry";
			return;
		}

		for (auto item : Entry2Items_.value (entry))
			item->setToolTip ({});

		DirtyTooltips_ << entry;
	}
}
}
