/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2014  Georg Rudoy
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

#include "pendinguploadbase.h"
#include <memory>
#include <QNetworkReply>
#include <QHttpMultiPart>
#include <QFileInfo>
#include <QFile>
#include <QStandardItemModel>
#include <QtDebug>
#include <util/sys/mimedetector.h>
#include <util/xpc/util.h>
#include <util/util.h>
#include <interfaces/ijobholder.h>

namespace LeechCraft
{
namespace Zalil
{
	PendingUploadBase::PendingUploadBase (const QString& filename,
			const ICoreProxy_ptr& proxy, QObject *parent)
	: QObject { parent }
	, ProgressRow_
	{
		new QStandardItem { tr ("Uploading %1")
			.arg (QFileInfo { filename }.fileName ()) },
		new QStandardItem { tr ("Uploading...") },
		new QStandardItem
	}
	, ProgressRowGuard_
	{
		[this]
		{
			if (const auto model = ProgressRow_.value (0)->model ())
				model->removeRow (ProgressRow_.value (0)->row ());
		}
	}
	, Filename_ { filename }
	, Proxy_ { proxy }
	{
		for (const auto item : ProgressRow_)
		{
			item->setEditable (false);
			item->setData (QVariant::fromValue<JobHolderRow> (JobHolderRow::ProcessProgress),
					CustomDataRoles::RoleJobHolderRow);
		}
	}

	const QList<QStandardItem*>& PendingUploadBase::GetReprRow () const
	{
		return ProgressRow_;
	}

	QHttpMultiPart* PendingUploadBase::MakeStandardMultipart ()
	{
		auto fileDev = std::make_unique<QFile> (Filename_, this);
		if (!fileDev->open (QIODevice::ReadOnly))
		{
			qWarning () << Q_FUNC_INFO
					<< "unable to open"
					<< Filename_
					<< fileDev->errorString ();
			return nullptr;
		}

		const auto multipart = new QHttpMultiPart { QHttpMultiPart::FormDataType, this };

		QHttpPart textPart;
		const QFileInfo fi { Filename_ };
		textPart.setHeader (QNetworkRequest::ContentDispositionHeader,
				"form-data; name=\"file\"; filename=\"" + fi.fileName () + "\"");
		textPart.setHeader (QNetworkRequest::ContentTypeHeader, Util::MimeDetector {} (Filename_));

		textPart.setBodyDevice (fileDev.release ());

		multipart->append (textPart);

		return multipart;
	}

	void PendingUploadBase::handleUploadProgress (qint64 done, qint64 total)
	{
		Util::SetJobHolderProgress (ProgressRow_, done, total,
				tr ("%1 of %2")
					.arg (Util::MakePrettySize (done))
					.arg (Util::MakePrettySize (total)));
	}

	void PendingUploadBase::handleError ()
	{
		deleteLater ();

		const auto reply = qobject_cast<QNetworkReply*> (sender ());
		reply->deleteLater ();

		qWarning () << Q_FUNC_INFO
				<< reply->error ()
				<< reply->errorString ();
	}
}
}
