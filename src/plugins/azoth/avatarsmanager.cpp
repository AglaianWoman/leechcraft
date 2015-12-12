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

#include "avatarsmanager.h"
#include <util/threads/futures.h>
#include <util/sll/util.h>
#include <util/sll/qtutil.h>
#include "interfaces/azoth/iaccount.h"
#include "avatarsstorage.h"
#include "resourcesmanager.h"
#include "xmlsettingsmanager.h"

namespace LeechCraft
{
namespace Azoth
{
	AvatarsManager::AvatarsManager (QObject *parent)
	: QObject { parent }
	, Storage_ { new AvatarsStorage { this } }
	{
		handleCacheSizeChanged ();
		XmlSettingsManager::Instance ().RegisterObject ("AvatarsCacheSize",
				this, "handleCacheSizeChanged");
	}

	namespace
	{
		int Size2Dim (IHaveAvatars::Size size)
		{
			switch (size)
			{
			case IHaveAvatars::Size::Full:
				return 256;
			case IHaveAvatars::Size::Thumbnail:
				return 64;
			}

			qWarning () << Q_FUNC_INFO
					<< "unknown size"
					<< static_cast<int> (size);
			return 256;
		}
	}

	QFuture<QImage> AvatarsManager::GetAvatar (QObject *entryObj, IHaveAvatars::Size size)
	{
		const auto defaultAvatarGetter = [size]
		{
			return ResourcesManager::Instance ().GetDefaultAvatar (Size2Dim (size));
		};

		const auto entry = qobject_cast<ICLEntry*> (entryObj);
		const auto iha = qobject_cast<IHaveAvatars*> (entryObj);
		if (!iha)
			return Util::MakeReadyFuture (defaultAvatarGetter ());

		const auto& sizes = PendingRequests_.value (entryObj);
		if (sizes.contains (size))
			return sizes.value (size);

		const auto entryId = entry->GetEntryID ();
		auto future = Util::Sequence (entryObj, Storage_->GetAvatar (entry, size))
				.DestructionValue (defaultAvatarGetter) >>
				[=] (const MaybeImage& image)
				{
					if (image)
						return Util::MakeReadyFuture (*image);

					auto refreshFuture = iha->RefreshAvatar (size);

					Util::Sequence (this, refreshFuture) >>
							[=] (const QImage& img) { Storage_->SetAvatar (entryId, size, img); };

					return refreshFuture;
				} >>
				[=] (QImage image)
				{
					auto& sizes = PendingRequests_ [entryObj];

					sizes.remove (size);
					if (sizes.isEmpty ())
						PendingRequests_.remove (entryObj);

					if (image.isNull ())
						image = defaultAvatarGetter ();

					return Util::MakeReadyFuture (image);
				};
		PendingRequests_ [entryObj] [size] = future;
		return future;
	}

	bool AvatarsManager::HasAvatar (QObject *entryObj) const
	{
		const auto iha = qobject_cast<IHaveAvatars*> (entryObj);
		return iha ?
				iha->HasAvatar () :
				false;
	}

	Util::DefaultScopeGuard AvatarsManager::Subscribe (QObject *obj,
			IHaveAvatars::Size size, const AvatarHandler_f& handler)
	{
		const auto id = ++SubscriptionID_;
		Subscriptions_ [obj] [size] [id] = handler;

		return Util::MakeScopeGuard ([=] { Subscriptions_ [obj] [size].remove (id); });
	}

	void AvatarsManager::HandleSubscriptions (QObject *entry)
	{
		for (const auto& pair : Util::Stlize (Subscriptions_.value (entry)))
		{
			if (pair.second.isEmpty ())
				continue;

			const auto size = pair.first;
			Util::Sequence (this, GetAvatar (entry, pair.first)) >>
					[this, size, entry] (const boost::optional<QImage>& image)
					{
						const auto& realImg = image.get_value_or ({});
						for (const auto& handler : Subscriptions_.value (entry).value (size))
							handler (realImg);
					};
		}
	}

	void AvatarsManager::handleAccount (QObject *accObj)
	{
		connect (accObj,
				SIGNAL (gotCLItems (QList<QObject*>)),
				this,
				SLOT (handleEntries (QList<QObject*>)));

		handleEntries (qobject_cast<IAccount*> (accObj)->GetCLEntries ());
	}

	void AvatarsManager::handleEntries (const QList<QObject*>& entries)
	{
		for (const auto entryObj : entries)
		{
			const auto iha = qobject_cast<IHaveAvatars*> (entryObj);
			if (!iha)
				continue;

			connect (entryObj,
					SIGNAL (avatarChanged (QObject*)),
					this,
					SLOT (invalidateAvatar (QObject*)));
		}
	}

	void AvatarsManager::invalidateAvatar (QObject *that)
	{
		const auto entry = qobject_cast<ICLEntry*> (that);
		if (!entry)
		{
			qWarning () << Q_FUNC_INFO
					<< "object is not an entry:"
					<< sender ()
					<< that;
			return;
		}

		Storage_->DeleteAvatars (entry->GetEntryID ());

		emit avatarInvalidated (that);

		HandleSubscriptions (that);
	}

	void AvatarsManager::handleCacheSizeChanged ()
	{
		Storage_->SetCacheSize (XmlSettingsManager::Instance ()
				.property ("AvatarsCacheSize").toInt ());
	}
}
}
