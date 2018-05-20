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

#include "export.h"
#include <QFileInfo>
#include <QFileDialog>
#include <QDir>
#include <QTimer>
#include "feed.h"
#include "storagebackendmanager.h"

namespace LeechCraft
{
namespace Aggregator
{
	Export::Export (const QString& title,
			const QString& exportTitle,
			const QString& choices,
			QWidget *parent)
	: QDialog (parent)
	{
		Ui_.setupUi (this);
		setWindowTitle (title);
		Title_ = exportTitle;
		Choices_ = choices;
		Ui_.ButtonBox_->button (QDialogButtonBox::Save)->setEnabled (false);
		on_Browse__released ();
	}
	
	QString Export::GetDestination () const
	{
		return Ui_.File_->text ();
	}
	
	QString Export::GetTitle () const
	{
		return Ui_.Title_->text ();
	}
	
	QString Export::GetOwner () const
	{
		return Ui_.Owner_->text ();
	}
	
	QString Export::GetOwnerEmail () const
	{
		return Ui_.OwnerEmail_->text ();
	}
	
	std::vector<bool> Export::GetSelectedFeeds () const
	{
		std::vector<bool> result (Ui_.Channels_->topLevelItemCount ());
	
		for (int i = 0, items = Ui_.Channels_->topLevelItemCount ();
				i < items; ++i)
			result [i] = (Ui_.Channels_->topLevelItem (i)->
					data (0, Qt::CheckStateRole) == Qt::Checked);
	
		return result;
	}
	
	void Export::SetFeeds (const channels_shorts_t& channels)
	{
		const auto& sb = StorageBackendManager::Instance ().MakeStorageBackendForThread ();
		for (channels_shorts_t::const_iterator i = channels.begin (),
				end = channels.end (); i != end; ++i)
		{
			const auto& feed = sb->GetFeed (i->FeedID_);
			const auto item = new QTreeWidgetItem (Ui_.Channels_, { i->Title_, feed.URL_ });
			item->setData (0, Qt::CheckStateRole, Qt::Checked);
		}
	}
	
	void Export::on_File__textEdited (const QString& text)
	{
		Ui_.ButtonBox_->button (QDialogButtonBox::Save)->setEnabled (!text.isEmpty ());
	}
	
	void Export::on_Browse__released ()
	{
		QString startingPath = QFileInfo (Ui_.File_->text ()).path ();
		if (Ui_.File_->text ().isEmpty () ||
				startingPath.isEmpty ())
			startingPath = QDir::homePath () + "/feeds.opml";
	
		QString filename = QFileDialog::getSaveFileName (this,
				Title_,
				startingPath,
				Choices_);
		if (filename.isEmpty ())
		{
			QTimer::singleShot (0,
					this,
					SLOT (reject ()));
			return;
		}
	
		Ui_.File_->setText (filename);
		Ui_.ButtonBox_->button (QDialogButtonBox::Save)->setEnabled (true);
	}
}
}
