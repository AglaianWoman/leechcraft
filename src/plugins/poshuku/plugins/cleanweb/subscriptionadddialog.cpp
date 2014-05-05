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

#include "subscriptionadddialog.h"
#include <QStandardItemModel>
#include <QDomDocument>
#include <QDir>
#include <QUrl>
#include <util/util.h>

namespace LeechCraft
{
namespace Poshuku
{
namespace CleanWeb
{
	SubscriptionAddDialog::SubscriptionAddDialog (QWidget *parent)
	: QDialog (parent)
	{
		Ui_.setupUi (this);

		QDir subscrListFileDir;
		try
		{
			subscrListFileDir = Util::CreateIfNotExists ("data/poshuku/cleanweb/");
		}
		catch (const std::exception& e)
		{
			// that's ok, the directory just doesn't
			// exist, skip it at all
			qDebug () << Q_FUNC_INFO
					<< e.what ();
			return;
		}

		if (!subscrListFileDir.exists ("subscriptionslist.xml"))
			return;

		QFile file (subscrListFileDir.filePath ("subscriptionslist.xml"));
		if (!file.open (QIODevice::ReadOnly))
		{
			qWarning () << Q_FUNC_INFO
					<< "could not open file"
					<< file.fileName ()
					<< "for reading:"
					<< file.errorString ();
			return;
		}
		QByteArray fileData = file.readAll ();
		QString errMsg;
		int errLine = 0;
		int errColumn = 0;
		QDomDocument doc;
		if (!doc.setContent (fileData, &errMsg, &errLine, &errColumn))
		{
			qWarning () << Q_FUNC_INFO
					<< "could not parse document at err:line "
					<< errLine
					<< errColumn
					<< "with"
					<< errMsg
					<< "; original contents follow:"
					<< fileData;
			return;
		}

		Ui_.PredefinedSubscriptions_->setEnabled (true);

		QStandardItemModel *model = new QStandardItemModel (this);
		QStringList labels;
		labels << tr ("Name")
				<< tr ("Purpose")
				<< tr ("URL");
		model->setHorizontalHeaderLabels (labels);

		QDomElement docElem = doc.documentElement ();
		Iterate (doc.documentElement (), model->invisibleRootItem ());

		Ui_.PredefinedSubscriptions_->setModel (model);

		Ui_.PredefinedSubscriptions_->expandAll ();
	}

	QString SubscriptionAddDialog::GetURL() const
	{
		return Ui_.URLEdit_->text ();
	}

	QString SubscriptionAddDialog::GetName () const
	{
		return Ui_.TitleEdit_->text ();
	}

	QList<QUrl> SubscriptionAddDialog::GetAdditionalSubscriptions() const
	{
		QList<QUrl> result;
		Q_FOREACH (QStandardItem *item, Items_)
			if (item->checkState () == Qt::Checked)
			{
				QString data = item->data ().toString ();
				result << QUrl::fromEncoded (data.toUtf8 ());
			}

		return result;
	}

	void SubscriptionAddDialog::Iterate (const QDomElement& subParent,
				QStandardItem *parent)
	{
		QDomElement subscr = subParent.firstChildElement ("subscription");
		while (!subscr.isNull ())
		{
			QString url = subscr.attribute ("url");
			QString name = subscr.attribute ("name");
			QString purpose = subscr.attribute ("purpose");

			QStandardItem *nameItem = new QStandardItem (name);
			nameItem->setCheckable (true);
			nameItem->setCheckState (Qt::Unchecked);
			nameItem->setData (url);
			Items_ << nameItem;

			QList<QStandardItem*> items;
			items << nameItem;
			items << new QStandardItem (purpose);
			items << new QStandardItem (url);
			parent->appendRow (items);

			Iterate (subscr, nameItem);

			subscr = subscr.nextSiblingElement ("subscription");
		}
	}
}
}
}
