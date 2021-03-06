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

#ifndef PLUGINS_AZOTH_PLUGINS_XOOX_GLOOXCLENTRY_H
#define PLUGINS_AZOTH_PLUGINS_XOOX_GLOOXCLENTRY_H
#include <memory>
#include <QObject>
#include <QStringList>
#include <QXmppRosterIq.h>
#include <QXmppVCardIq.h>
#include <QXmppPresence.h>
#include <interfaces/azoth/iauthable.h>
#include "entrybase.h"
#include "offlinedatasource.h"

namespace LeechCraft
{
namespace Azoth
{
class IAccount;
class IProxyObject;

namespace Xoox
{
	class GlooxAccount;
	class PrivacyList;
	class VCardStorage;

	class GlooxCLEntry : public EntryBase
					   , public IAuthable
	{
		Q_OBJECT
		Q_INTERFACES (LeechCraft::Azoth::IAuthable)
	private:
		OfflineDataSource_ptr ODS_;

		struct MessageQueueItem
		{
			IMessage::Type Type_;
			QString Variant_;
			QString Text_;
			QDateTime DateTime_;
		};
		QList<MessageQueueItem> MessageQueue_;

		bool AuthRequested_ = false;

		mutable QList<QAction*> GWActions_;
	public:
		GlooxCLEntry (const QString& bareJID, GlooxAccount*);
		GlooxCLEntry (OfflineDataSource_ptr, GlooxAccount*);

		OfflineDataSource_ptr ToOfflineDataSource () const;
		void Convert2ODS ();

		static QString JIDFromID (GlooxAccount*, const QString&);

		void UpdateRI (const QXmppRosterIq::Item&);
		QXmppRosterIq::Item GetRI () const;

		// ICLEntry
		Features GetEntryFeatures () const;
		EntryType GetEntryType () const;
		QString GetEntryName () const;
		void SetEntryName (const QString&);
		/** Entry ID for GlooxCLEntry is its jid.
		 */
		QString GetEntryID () const;
		QStringList Groups () const;
		void SetGroups (const QStringList&);
		QStringList Variants () const;
		EntryStatus GetStatus (const QString&) const;
		IMessage* CreateMessage (IMessage::Type,
				const QString&, const QString&);
		QList<QAction*> GetActions () const;

		// IAuthable
		AuthStatus GetAuthStatus () const;
		void ResendAuth (const QString&);
		void RevokeAuth (const QString&);
		void Unsubscribe (const QString&);
		void RerequestAuth (const QString&);

		QString GetJID () const;

		bool IsGateway (QString* = 0) const;

		void SetAuthRequested (bool);
	private:
		void SendGWPresence (QXmppPresence::Type);
	private slots:
		void handleGWLogin ();
		void handleGWLogout ();
		void handleGWEdit ();
	};
}
}
}

#endif
