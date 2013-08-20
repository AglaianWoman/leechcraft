/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2006-2013  Georg Rudoy
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

#include <memory>
#include <QObject>
#include <interfaces/blasq/iaccount.h>
#include <interfaces/blasq/isupportuploads.h>
#include <interfaces/blasq/isupportdeletes.h>
#include <interfaces/core/icoreproxy.h>

class QNetworkReply;
class QDomElement;
class QStandardItemModel;
class QStandardItem;

namespace LeechCraft
{
namespace Util
{
	class QueueManager;

	namespace SvcAuth
	{
		class VkAuthManager;
	}
}

namespace Blasq
{
namespace Rappor
{
	class VkService;

	class VkAccount : public QObject
					, public IAccount
					, public ISupportUploads
					, public ISupportDeletes
	{
		Q_OBJECT
		Q_INTERFACES (LeechCraft::Blasq::IAccount
				LeechCraft::Blasq::ISupportUploads
				LeechCraft::Blasq::ISupportDeletes)

		QString Name_;
		const QByteArray ID_;
		VkService * const Service_;
		const ICoreProxy_ptr Proxy_;

		QStandardItemModel * const CollectionsModel_;
		QStandardItem *AllPhotosItem_ = 0;
		QHash<int, QStandardItem*> Albums_;

		Util::SvcAuth::VkAuthManager * const AuthMgr_;

		QByteArray LastCookies_;

		Util::QueueManager *RequestQueue_;
		QList<std::function<void (const QString&)>> CallQueue_;

		QHash<QNetworkReply*, QStringList> PhotosUploadServer2Paths_;
	public:
		VkAccount (const QString&, VkService*, ICoreProxy_ptr,
				const QByteArray& id = QByteArray (),
				const QByteArray& cookies = QByteArray ());

		QByteArray Serialize () const;
		static VkAccount* Deserialize (const QByteArray&, VkService*, ICoreProxy_ptr);

		QObject* GetQObject ();
		IService* GetService () const;
		QString GetName () const;
		QByteArray GetID () const;

		QAbstractItemModel* GetCollectionsModel () const;
		void UpdateCollections ();

		bool HasUploadFeature (Feature) const;
		void CreateCollection (const QModelIndex& parent);
		void UploadImages (const QModelIndex& collection, const QStringList& paths);

		void Delete (const QModelIndex&);
	private:
		void HandleAlbumElement (const QDomElement&);
		bool HandlePhotoElement (const QDomElement&, bool atEnd = true);
	private slots:
		void handleGotAlbums ();
		void handleGotPhotos ();

		void handleAlbumCreated ();
		void handlePhotosUploadServer ();
		void handlePhotosUploadProgress (qint64, qint64);
		void handlePhotosUploaded ();
		void handlePhotosSaved ();
		void handlePhotosInfosFetched ();

		void handleAuthKey (const QString&);
		void handleCookies (const QByteArray&);
	signals:
		void accountChanged (VkAccount*);

		void doneUpdating ();
	};
}
}
}
