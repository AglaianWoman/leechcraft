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

#pragma once

#include <QObject>
#include <QImage>
#include <QMap>
#include <QVariant>
#include <QXmppMessage.h>
#include <QXmppVCardIq.h>
#include <QXmppVersionIq.h>
#include <QXmppDiscoveryIq.h>
#include <interfaces/media/audiostructs.h>
#include <interfaces/azoth/iclentry.h>
#include <interfaces/azoth/iadvancedclentry.h>
#include <interfaces/azoth/imetainfoentry.h>
#include <interfaces/azoth/ihavedirectedstatus.h>
#include <interfaces/azoth/isupportgeolocation.h>
#include <interfaces/azoth/isupportmicroblogs.h>
#include <interfaces/azoth/ihaveentitytime.h>
#include <interfaces/azoth/ihavepings.h>
#include <interfaces/azoth/ihavequeriableversion.h>
#include <interfaces/azoth/ihavecontacttune.h>
#include <interfaces/azoth/ihavecontactmood.h>
#include <interfaces/azoth/ihavecontactactivity.h>

class QXmppPresence;
class QXmppVersionIq;
class QXmppEntityTimeIq;

namespace LeechCraft
{
namespace Azoth
{
namespace Xoox
{
	class ClientConnection;
	class PEPEventBase;
	class GlooxMessage;
	class VCardDialog;
	class GlooxAccount;
	class UserTune;
	class UserMood;
	class UserActivity;

	/** Common base class for GlooxCLEntry, which reprensents usual
	 * entries in the contact list, and RoomCLEntry, which represents
	 * participants in MUCs.
	 *
	 * This class tries to unify and provide a common implementation of
	 * what those classes, well, have in common.
	 */
	class EntryBase : public QObject
					, public ICLEntry
					, public IAdvancedCLEntry
					, public IMetaInfoEntry
					, public IHaveDirectedStatus
					, public ISupportMicroblogs
					, public IHaveContactTune
					, public IHaveContactMood
					, public IHaveContactActivity
					, public IHaveEntityTime
					, public IHavePings
					, public IHaveQueriableVersion
	{
		Q_OBJECT
		Q_INTERFACES (LeechCraft::Azoth::ICLEntry
				LeechCraft::Azoth::IAdvancedCLEntry
				LeechCraft::Azoth::IMetaInfoEntry
				LeechCraft::Azoth::IHaveDirectedStatus
				LeechCraft::Azoth::ISupportMicroblogs
				LeechCraft::Azoth::IHaveContactTune
				LeechCraft::Azoth::IHaveContactMood
				LeechCraft::Azoth::IHaveContactActivity
				LeechCraft::Azoth::IHaveEntityTime
				LeechCraft::Azoth::IHavePings
				LeechCraft::Azoth::IHaveQueriableVersion)
	protected:
		GlooxAccount *Account_;

		const QString HumanReadableId_;

		QList<GlooxMessage*> AllMessages_;
		QList<GlooxMessage*> UnreadMessages_;
		QMap<QString, EntryStatus> CurrentStatus_;
		QList<QAction*> Actions_;
		QAction *Commands_;
		QAction *DetectNick_;
		QAction *StdSep_;

		QMap<QString, GeolocationInfo_t> Location_;

		QImage Avatar_;
		QPointer<VCardDialog> VCardDialog_;

		QByteArray VCardPhotoHash_;

		QMap<QString, QMap<QString, QVariant>> Variant2ClientInfo_;
		QMap<QString, QByteArray> Variant2VerString_;
		QMap<QString, QXmppVersionIq> Variant2Version_;
		QMap<QString, QList<QXmppDiscoveryIq::Identity>> Variant2Identities_;

		QMap<QString, Media::AudioInfo> Variant2Audio_;
		QMap<QString, MoodInfo> Variant2Mood_;
		QMap<QString, ActivityInfo> Variant2Activity_;

		struct EntityTimeInfo
		{
			int Diff_;
			int Tzo_;
		};
		QMap<QString, EntityTimeInfo> Variant2SecsDiff_;
		QDateTime LastEntityTimeRequest_;

		bool HasUnreadMsgs_ = false;
		bool HasBlindlyRequestedVCard_ = false;
	public:
		EntryBase (const QString& humanReadableId, GlooxAccount* = nullptr);
		virtual ~EntryBase ();

		// ICLEntry
		QObject* GetQObject ();
		QList<IMessage*> GetAllMessages () const;
		void PurgeMessages (const QDateTime&);
		void SetChatPartState (ChatPartState, const QString&);
		EntryStatus GetStatus (const QString&) const;
		virtual QList<QAction*> GetActions () const;
		QImage GetAvatar () const;
		QString GetHumanReadableID () const final;
		void ShowInfo ();
		QMap<QString, QVariant> GetClientInfo (const QString&) const;
		void MarkMsgsRead ();
		void ChatTabClosed ();

		// IAdvancedCLEntry
		AdvancedFeatures GetAdvancedFeatures () const;
		void DrawAttention (const QString&, const QString&);

		// IMetaInfoEntry
		QVariant GetMetaInfo (DataField) const;
		QList<QPair<QString, QVariant>> GetVCardRepresentation () const;

		// IHaveDirectedStatus
		bool CanSendDirectedStatusNow (const QString&);
		void SendDirectedStatus (const EntryStatus&, const QString&);

		// ISupportMicroblogs
		void RequestLastPosts (int);

		// IHaveContactTune
		Media::AudioInfo GetUserTune (const QString&) const;

		// IHaveContactMood
		MoodInfo GetUserMood (const QString&) const;

		// IHaveContactActivity
		ActivityInfo GetUserActivity (const QString&) const;

		// IHaveEntityTime
		void UpdateEntityTime ();

		// IHavePings
		QObject* Ping (const QString& variant);

		// IHaveQueriableVersion
		QObject* QueryVersion (const QString& variant);

		virtual QString GetJID () const = 0;

		void HandlePresence (const QXmppPresence&, const QString&);
		void HandleMessage (GlooxMessage*);
		void HandlePEPEvent (QString, PEPEventBase*);
		void HandleAttentionMessage (const QXmppMessage&);
		void UpdateChatState (QXmppMessage::State, const QString&);
		void SetStatus (const EntryStatus&, const QString&, const QXmppPresence&);
		void SetAvatar (const QByteArray&);
		void SetAvatar (const QImage&);
		QXmppVCardIq GetVCard () const;
		void SetVCard (const QXmppVCardIq&);

		bool HasUnreadMsgs () const;
		QList<GlooxMessage*> GetUnreadMessages () const;

		void SetClientInfo (const QString&, const QString&, const QByteArray&);
		void SetClientInfo (const QString&, const QXmppPresence&);
		void SetClientVersion (const QString&, const QXmppVersionIq&);
		void SetDiscoIdentities (const QString&, const QList<QXmppDiscoveryIq::Identity>&);

		GeolocationInfo_t GetGeolocationInfo (const QString&) const;

		QByteArray GetVariantVerString (const QString&) const;
		QXmppVersionIq GetClientVersion (const QString&) const;
	private:
		void HandleUserActivity (const UserActivity*, const QString&);
		void HandleUserMood (const UserMood*, const QString&);
		void HandleUserTune (const UserTune*, const QString&);

		void CheckVCardUpdate (const QXmppPresence&);
		void SetNickFromVCard (const QXmppVCardIq&);
	private slots:
		void handleTimeReceived (const QXmppEntityTimeIq&);

		void handleCommands ();
		void handleDetectNick ();
	signals:
		void gotMessage (QObject*);
		void statusChanged (const EntryStatus&, const QString&);
		void avatarChanged (const QImage&);
		void availableVariantsChanged (const QStringList&);
		void nameChanged (const QString&);
		void groupsChanged (const QStringList&);
		void chatPartStateChanged (const ChatPartState&, const QString&);
		void permsChanged ();
		void entryGenerallyChanged ();

		void chatTabClosed ();

		void attentionDrawn (const QString&, const QString&);
		void moodChanged (const QString&);
		void activityChanged (const QString&);
		void tuneChanged (const QString&);
		void locationChanged (const QString&);

		void gotRecentPosts (const QList<LeechCraft::Azoth::Post>&);
		void gotNewPost (const LeechCraft::Azoth::Post&);

		void locationChanged (const QString&, QObject*);

		void vcardUpdated ();

		void entityTimeUpdated ();
	};
}
}
}
