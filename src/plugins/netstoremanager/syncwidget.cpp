/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2010-2012  Oleg Linkin
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

#include "syncwidget.h"
#include <QStandardItemModel>
#include <QMessageBox>
#include <QtDebug>
#include <QDir>
#include "accountsmanager.h"
#include "syncitemdelegate.h"
#include "xmlsettingsmanager.h"
#include "interfaces/netstoremanager/istorageaccount.h"
#include "interfaces/netstoremanager/istorageplugin.h"

namespace LeechCraft
{
namespace NetStoreManager
{
	SyncWidget::SyncWidget (AccountsManager *am, QWidget *parent)
	: QWidget (parent)
	, AM_ (am)
	, Model_ (new QStandardItemModel (this))
	{
		Ui_.setupUi (this);

		Model_->setHorizontalHeaderLabels ({ tr ("Account"),
				tr ("Local directory"), tr ("Remote directory") });
		Ui_.SyncView_->horizontalHeader ()->setStretchLastSection (true);
		Ui_.SyncView_->setItemDelegate (new SyncItemDelegate (AM_, Model_, this));
		Ui_.SyncView_->setModel (Model_);
	}

	void SyncWidget::RestoreData ()
	{
		QVariantMap map = XmlSettingsManager::Instance ().property ("Synchronization").toMap ();
		for (auto key : map.keys ())
		{
			auto isa = AM_->GetAccountFromUniqueID (key);
			if (!isa)
			{
				qWarning () << Q_FUNC_INFO
						<< "there is no account with ID"
						<< key;
				continue;
			}
			auto isp = qobject_cast<IStoragePlugin*> (isa->GetParentPlugin ());
			if (!isp)
				continue;

			QStandardItem *accItem = new QStandardItem;
			accItem->setData (isp->GetStorageName () + ": " + isa->GetAccountName (),
					Qt::EditRole);
			accItem->setData (key, SyncItemDelegate::AccountId);
			QStandardItem *localDirItem = new QStandardItem;
			SyncDirs_t dirs = map [key].value<SyncDirs_t> ();
			const QString& localPath = dirs.first.isEmpty () ?
				QDir::homePath () :
				dirs.first;
			localDirItem->setData (localPath, Qt::EditRole);
			QStandardItem *remoteDirItem = new QStandardItem;
			const QString& remotePath = dirs.second.isEmpty () ?
				("LeechCraft_" + isa->GetAccountName ()) :
				dirs.second;
			remoteDirItem->setData (remotePath, Qt::EditRole);
			Model_->appendRow ({ accItem, localDirItem, remoteDirItem });

			Ui_.SyncView_->openPersistentEditor (Model_->indexFromItem (accItem));
			Ui_.SyncView_->resizeColumnToContents (SyncItemDelegate::Account);
		}

		emit directoriesToSyncUpdated (map);
	}

	void SyncWidget::accept ()
	{
		QVariantMap map;
		for (int i = 0; i < Model_->rowCount (); ++i)
		{
			QStandardItem *accItem = Model_->item (i, SyncItemDelegate::Account);
			QStandardItem *localDirItem = Model_->item (i, SyncItemDelegate::LocalDirectory);
			QStandardItem *remoteDirItem = Model_->item (i, SyncItemDelegate::RemoteDirecory);
			if (!accItem ||
					!localDirItem ||
					!remoteDirItem ||
					accItem->data (SyncItemDelegate::AccountId).isNull () ||
					localDirItem->text ().isEmpty () ||
					remoteDirItem->text ().isEmpty ())
			{
				continue;
			}

			map [accItem->data (SyncItemDelegate::AccountId).toString ()] =
					QVariant::fromValue<SyncDirs_t> (qMakePair (localDirItem->text (),
							remoteDirItem->text ()));
		}

		emit directoriesToSyncUpdated (map);
		XmlSettingsManager::Instance ().setProperty ("Synchronization", map);
	}

	void SyncWidget::on_Add__released ()
	{
		Model_->appendRow ({ new QStandardItem, new QStandardItem });
		Ui_.SyncView_->openPersistentEditor (Model_->index (Model_->rowCount () - 1,
				SyncItemDelegate::Account));
		Ui_.SyncView_->resizeColumnToContents (SyncItemDelegate::Account);
	}

	void SyncWidget::on_Remove__released ()
	{
		const auto& idxList = Ui_.SyncView_->selectionModel ()->selectedIndexes ();
		for (auto idx : idxList)
			Model_->removeRow (idx.row ());
	}
}
}

