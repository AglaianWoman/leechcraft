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

#ifndef PLUGINS_AZOTH_PLUGINS_XOOX_ROOMPARTICIPANTENTRY_H
#define PLUGINS_AZOTH_PLUGINS_XOOX_ROOMPARTICIPANTENTRY_H
#include <memory>
#include <QObject>
#include <QStringList>
#include <QXmppMucIq.h>
#include "entrybase.h"

namespace LeechCraft
{
namespace Azoth
{
namespace Xoox
{
	class GlooxAccount;
	class RoomPublicMessage;
	class GlooxMessage;
	class RoomHandler;

	class RoomParticipantEntry : public EntryBase
							   , public std::enable_shared_from_this<RoomParticipantEntry>
	{
		Q_OBJECT

		QString Nick_;
		RoomHandler *RoomHandler_;

		QXmppMucItem::Affiliation Affiliation_;
		QXmppMucItem::Role Role_;
	public:
		RoomParticipantEntry (const QString&, RoomHandler*, GlooxAccount*);

		IAccount* GetParentAccount () const ;
		ICLEntry* GetParentCLEntry () const;
		Features GetEntryFeatures () const;
		EntryType GetEntryType () const;
		QString GetEntryName () const;
		void SetEntryName (const QString&);
		QString GetEntryID () const;
		QStringList Groups () const;
		void SetGroups (const QStringList&);
		QStringList Variants () const;
		IMessage* CreateMessage (IMessage::Type,
				const QString&, const QString&);

		QString GetJID () const;
		QString GetRealJID () const;
		QString GetNick () const;

		void StealMessagesFrom (RoomParticipantEntry*);

		QXmppMucItem::Affiliation GetAffiliation () const;
		void SetAffiliation (QXmppMucItem::Affiliation);
		QXmppMucItem::Role GetRole () const;
		void SetRole (QXmppMucItem::Role);
	};

	typedef std::shared_ptr<RoomParticipantEntry> RoomParticipantEntry_ptr;
}
}
}

#endif
