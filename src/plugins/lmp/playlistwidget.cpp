/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2006-2012  Georg Rudoy
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **********************************************************************/

#include "playlistwidget.h"
#include <algorithm>
#include <QToolBar>
#include <QInputDialog>
#include <QFileDialog>
#include <QActionGroup>
#include <QToolButton>
#include <QMenu>
#include <QUndoStack>
#include <QMessageBox>
#include <QClipboard>
#include <QApplication>
#include <QKeyEvent>
#include <QSortFilterProxyModel>
#include <util/util.h>
#include <util/gui/clearlineeditaddon.h>
#include "player.h"
#include "playlistdelegate.h"
#include "xmlsettingsmanager.h"
#include "core.h"
#include "playlistmanager.h"
#include "staticplaylistmanager.h"
#include "audiopropswidget.h"
#include "playlistundocommand.h"
#include "util.h"

namespace LeechCraft
{
namespace LMP
{
	namespace
	{
		class PlaylistTreeEventFilter : public QObject
		{
			Player *Player_;
			QTreeView *View_;
			QSortFilterProxyModel *PlaylistFilter_;
			QLineEdit *FilterLine_;
			QAction *FilterToggle_;
		public:
			PlaylistTreeEventFilter (Player* player,
					QTreeView *view,
					QSortFilterProxyModel *filter,
					QLineEdit *filterLine,
					QAction *filterToggle,
					QObject *parent = 0)
			: QObject (parent)
			, Player_ (player)
			, View_ (view)
			, PlaylistFilter_ (filter)
			, FilterLine_ (filterLine)
			, FilterToggle_ (filterToggle)
			{
			}

			bool eventFilter (QObject*, QEvent *e)
			{
				if (e->type () != QEvent::KeyRelease)
					return false;

				const auto key = static_cast<QKeyEvent*> (e)->key ();
				if (key == Qt::Key_Enter || key == Qt::Key_Return || key == Qt::Key_Space)
				{
					Player_->play (PlaylistFilter_->mapToSource (View_->currentIndex ()));
					return true;
				}

				if (key == Qt::Key_F &&
						static_cast<QKeyEvent*> (e)->modifiers () == Qt::CTRL)
				{
					FilterLine_->setVisible (!FilterLine_->isVisible ());
					FilterToggle_->toggle ();
					return true;
				}

				return false;
			}
		};

		class TreeFilterModel : public QSortFilterProxyModel
		{
		public:
			TreeFilterModel (QObject *parent = 0)
			: QSortFilterProxyModel (parent)
			{
				setDynamicSortFilter (true);
			}
		protected:
			bool filterAcceptsRow (int row, const QModelIndex& parent) const
			{
				const auto& str = filterRegExp ().pattern ();
				if (str.isEmpty ())
					return true;

				auto check = [&str] (const QString& string)
				{
					return string.contains (str, Qt::CaseInsensitive);
				};

				const auto& idx = sourceModel ()->index (row, 0, parent);
				const auto& info = idx.data (Player::Role::Info).value<MediaInfo> ();
				if (check (info.Artist_) ||
					check (info.Album_) ||
					check (QString::number (info.Year_)))
					return true;

				if (parent.isValid () && check (info.Title_))
					return true;

				for (int i = 0, rc = sourceModel ()->rowCount (idx); i < rc; ++i)
					if (filterAcceptsRow (i, idx))
						return true;

				return false;
			}
		};
	}

	PlaylistWidget::PlaylistWidget (QWidget *parent)
	: QWidget (parent)
	, PlaylistToolbar_ (new QToolBar ())
	, PlaylistFilter_ (new TreeFilterModel (this))
	, UndoStack_ (new QUndoStack (this))
	, Player_ (0)
	, ActionRemoveSelected_ (0)
	, ActionStopAfterSelected_ (0)
	, ActionShowTrackProps_ (0)
	, ActionShowAlbumArt_ (0)
	{
		Ui_.setupUi (this);

		new Util::ClearLineEditAddon (Core::Instance ().GetProxy (), Ui_.SearchPlaylist_);

		Ui_.BufferProgress_->hide ();
		Ui_.Playlist_->setItemDelegate (new PlaylistDelegate (Ui_.Playlist_, Ui_.Playlist_));

		connect (Ui_.SearchPlaylist_,
				SIGNAL (textChanged (QString)),
				PlaylistFilter_,
				SLOT (setFilterFixedString (QString)));

		connect (PlaylistFilter_,
				SIGNAL (rowsInserted (QModelIndex, int, int)),
				Ui_.Playlist_,
				SLOT (expandAll ()),
				Qt::QueuedConnection);
		connect (PlaylistFilter_,
				SIGNAL (modelReset ()),
				Ui_.Playlist_,
				SLOT (expandAll ()),
				Qt::QueuedConnection);
	}

	void PlaylistWidget::SetPlayer (Player *player)
	{
		Player_ = player;

		connect (Player_,
				SIGNAL (bufferStatusChanged (int)),
				this,
				SLOT (handleBufferStatus (int)));

		PlaylistFilter_->setSourceModel (Player_->GetPlaylistModel ());
		Ui_.Playlist_->setModel (PlaylistFilter_);
		Ui_.Playlist_->expandAll ();

		connect (Ui_.Playlist_,
				SIGNAL (doubleClicked (QModelIndex)),
				this,
				SLOT (play (QModelIndex)));
		connect (Player_,
				SIGNAL (insertedAlbum (QModelIndex)),
				this,
				SLOT (expand (QModelIndex)));

		Ui_.PlaylistLayout_->addWidget (PlaylistToolbar_);

		InitToolbarActions ();
		InitViewActions ();

		auto model = Player_->GetPlaylistModel ();
		connect (model,
				SIGNAL (rowsInserted (QModelIndex, int, int)),
				this,
				SLOT (updateStatsLabel ()),
				Qt::QueuedConnection);
		connect (model,
				SIGNAL (rowsRemoved (QModelIndex, int, int)),
				this,
				SLOT (updateStatsLabel ()),
				Qt::QueuedConnection);
		connect (model,
				SIGNAL (modelReset ()),
				this,
				SLOT (updateStatsLabel ()),
				Qt::QueuedConnection);
		updateStatsLabel ();

		Ui_.Playlist_->installEventFilter (new PlaylistTreeEventFilter (Player_,
					Ui_.Playlist_,
					PlaylistFilter_,
					Ui_.SearchPlaylist_,
					ActionToggleSearch_));
	}

	void PlaylistWidget::InitToolbarActions ()
	{
		QAction *clearPlaylist = new QAction (tr ("Clear..."), this);
		clearPlaylist->setProperty ("ActionIcon", "edit-clear-list");
		connect (clearPlaylist,
				SIGNAL (triggered ()),
				Player_,
				SLOT (clear ()));
		PlaylistToolbar_->addAction (clearPlaylist);

		QAction *savePlaylist = new QAction (tr ("Save playlist..."), this);
		savePlaylist->setProperty ("ActionIcon", "document-save");
		connect (savePlaylist,
				SIGNAL (triggered ()),
				this,
				SLOT (handleSavePlaylist ()));
		PlaylistToolbar_->addAction (savePlaylist);

		QAction *loadFiles = new QAction (tr ("Load from disk..."), this);
		loadFiles->setProperty ("ActionIcon", "document-open");
		connect (loadFiles,
				SIGNAL (triggered ()),
				this,
				SLOT (loadFromDisk ()));
		PlaylistToolbar_->addAction (loadFiles);

		QAction *addURL = new QAction (tr ("Add URL..."), this);
		addURL->setProperty ("ActionIcon", "folder-remote");
		connect (addURL,
				SIGNAL (triggered ()),
				this,
				SLOT (addURL ()));
		PlaylistToolbar_->addAction (addURL);

		PlaylistToolbar_->addSeparator ();

		SetPlayModeButton ();
		SetSortOrderButton ();

		PlaylistToolbar_->addAction (Util::CreateSeparator (this));
		auto undo = UndoStack_->createUndoAction (this);
		undo->setProperty ("ActionIcon", "edit-undo");
		undo->setShortcut (QKeySequence ("Ctrl+Z"));
		PlaylistToolbar_->addAction (undo);
		auto redo = UndoStack_->createRedoAction (this);
		redo->setProperty ("ActionIcon", "edit-redo");
		PlaylistToolbar_->addAction (redo);
	}

	void PlaylistWidget::SetPlayModeButton ()
	{
		auto playButton = new QToolButton;
		playButton->setIcon (Core::Instance ().GetProxy ()->GetIcon ("view-media-playlist"));
		playButton->setPopupMode (QToolButton::InstantPopup);
		QMenu *playMode = new QMenu (tr ("Play mode"));
		playButton->setMenu (playMode);

		const std::vector<Player::PlayMode> modes = { Player::PlayMode::Sequential,
				Player::PlayMode::Shuffle, Player::PlayMode::RepeatTrack,
				Player::PlayMode::RepeatAlbum, Player::PlayMode::RepeatWhole };
		const std::vector<QString> names = { tr ("Sequential"),
				tr ("Shuffle"), tr ("Repeat track"),
				tr ("Repeat album"), tr ("Repeat whole") };
		PlayModesGroup_ = new QActionGroup (this);
		for (size_t i = 0; i < modes.size (); ++i)
		{
			QAction *action = new QAction (names [i], this);
			action->setProperty ("PlayMode", static_cast<int> (modes.at (i)));
			action->setCheckable (true);
			action->setChecked (!i);
			action->setActionGroup (PlayModesGroup_);
			playMode->addAction (action);

			connect (action,
					SIGNAL (triggered ()),
					this,
					SLOT (handleChangePlayMode ()));
		}
		connect (Player_,
				SIGNAL (playModeChanged (Player::PlayMode)),
				this,
				SLOT (handlePlayModeChanged (Player::PlayMode)));
		const int resumeMode = XmlSettingsManager::Instance ()
				.Property ("PlayMode", static_cast<int> (Player::PlayMode::Sequential)).toInt ();
		Player_->SetPlayMode (static_cast<Player::PlayMode> (resumeMode));

		PlaylistToolbar_->addWidget (playButton);
	}

	void PlaylistWidget::SetSortOrderButton ()
	{
		auto sortButton = new QToolButton;
		sortButton->setIcon (Core::Instance ().GetProxy ()->GetIcon ("view-sort-ascending"));
		sortButton->setPopupMode (QToolButton::InstantPopup);

		auto menu = new QMenu (tr ("Sorting"));
		sortButton->setMenu (menu);

		auto getInts = [] (const QList<Player::SortingCriteria>& crit)
		{
			QVariantList result;
			std::transform (crit.begin (), crit.end (), std::back_inserter (result),
					[] (decltype (crit.front ()) item) { return static_cast<int> (item); });
			return result;
		};

		typedef QPair<QString, QList<Player::SortingCriteria>> SortPair_t;
		QList<SortPair_t> stdSorts;
#if QT_VERSION >= 0x040800
		stdSorts << SortPair_t (tr ("Artist / Year / Album / Track number"),
					{
						Player::SortingCriteria::Artist,
						Player::SortingCriteria::Year,
						Player::SortingCriteria::Album,
						Player::SortingCriteria::TrackNumber
					});
		stdSorts << SortPair_t (tr ("Artist / Track title"),
					{
						Player::SortingCriteria::Artist,
						Player::SortingCriteria::TrackTitle
					});
		stdSorts << SortPair_t (tr ("File path"),
					{
						Player::SortingCriteria::FilePath
					});
		stdSorts << SortPair_t (tr ("No sort"), {});
#endif

		auto sortGroup = new QActionGroup (this);
		bool isFirst = true;
		Q_FOREACH (const auto& pair, stdSorts)
		{
			auto act = menu->addAction (pair.first);
			act->setProperty ("SortInts", getInts (pair.second));
			act->setCheckable (true);
			act->setChecked (isFirst);
			sortGroup->addAction (act);

			isFirst = false;

			connect (act,
					SIGNAL (triggered ()),
					this,
					SLOT (handleStdSort ()));
		}

		PlaylistToolbar_->addWidget (sortButton);
	}

	void PlaylistWidget::InitViewActions ()
	{
		ActionRemoveSelected_ = new QAction (tr ("Delete from playlist"), Ui_.Playlist_);
		ActionRemoveSelected_->setProperty ("ActionIcon", "list-remove");
		ActionRemoveSelected_->setShortcut (Qt::Key_Delete);
		connect (ActionRemoveSelected_,
				SIGNAL (triggered ()),
				this,
				SLOT (removeSelectedSongs ()));
		Ui_.Playlist_->addAction (ActionRemoveSelected_);

		ActionStopAfterSelected_ = new QAction (tr ("Stop after this track"), Ui_.Playlist_);
		ActionStopAfterSelected_->setProperty ("ActionIcon", "media-playback-stop");
		connect (ActionStopAfterSelected_,
				SIGNAL (triggered ()),
				this,
				SLOT (setStopAfterSelected ()));

		ActionShowTrackProps_ = new QAction (tr ("Show track properties"), Ui_.Playlist_);
		ActionShowTrackProps_->setProperty ("ActionIcon", "document-properties");
		connect (ActionShowTrackProps_,
				SIGNAL (triggered ()),
				this,
				SLOT (showTrackProps ()));

		ActionShowAlbumArt_ = new QAction (tr ("Show album art"), Ui_.Playlist_);
		ActionShowAlbumArt_->setProperty ("ActionIcon", "media-optical");
		connect (ActionShowAlbumArt_,
				SIGNAL (triggered ()),
				this,
				SLOT (showAlbumArt ()));

		ActionMoveUp_ = new QAction (tr ("Move tracks up"), Ui_.Playlist_);
		ActionMoveUp_->setProperty ("ActionIcon", "go-up");
		connect (ActionMoveUp_,
				SIGNAL (triggered ()),
				this,
				SLOT (handleMoveUp ()));

		ActionMoveDown_ = new QAction (tr ("Move tracks down"), Ui_.Playlist_);
		ActionMoveDown_->setProperty ("ActionIcon", "go-down");
		connect (ActionMoveDown_,
				SIGNAL (triggered ()),
				this,
				SLOT (handleMoveDown ()));

		ActionToggleSearch_ = new QAction (tr ("Toggle search field"), Ui_.Playlist_);
		ActionToggleSearch_->setShortcut (QKeySequence::Find);
		ActionToggleSearch_->setCheckable (true);
		connect (ActionToggleSearch_,
				SIGNAL (toggled (bool)),
				Ui_.SearchPlaylist_,
				SLOT (setVisible (bool)));
		Ui_.SearchPlaylist_->setVisible (false);
	}

	void PlaylistWidget::SelectSources (const QList<Phonon::MediaSource>& sources)
	{
		auto tryIdx = [&sources, this] (const QModelIndex& idx)
		{
			if (sources.contains (Player_->GetIndexSources (idx).value (0)))
				Ui_.Playlist_->selectionModel ()->select (PlaylistFilter_->mapFromSource (idx),
						QItemSelectionModel::Select | QItemSelectionModel::Rows);
		};

		auto plModel = Player_->GetPlaylistModel ();
		for (int i = 0; i < plModel->rowCount (); ++i)
		{
			const auto& albumIdx = plModel->index (i, 0);

			const int tracks = plModel->rowCount (albumIdx);
			if (!tracks)
				tryIdx (albumIdx);
			else
				for (int j = 0; j < tracks; ++j)
					tryIdx (plModel->index (j, 0, albumIdx));
		}
	}

	void PlaylistWidget::focusIndex (const QModelIndex& index)
	{
		Ui_.Playlist_->scrollTo (PlaylistFilter_->mapFromSource (index),
				QAbstractItemView::PositionAtCenter);
	}

	void PlaylistWidget::on_Playlist__customContextMenuRequested (const QPoint& pos)
	{
		const auto& idx = Ui_.Playlist_->indexAt (pos);
		if (!idx.isValid ())
			return;

		auto menu = new QMenu (Ui_.Playlist_);
		menu->addAction (ActionRemoveSelected_);
		if (idx.data (Player::Role::IsAlbum).toBool ())
			menu->addAction (ActionShowAlbumArt_);
		else
		{
			menu->addAction (ActionStopAfterSelected_);
			menu->addAction (ActionShowTrackProps_);
		}

		menu->addSeparator ();

		menu->addAction (ActionMoveUp_);
		menu->addAction (ActionMoveDown_);

		menu->addSeparator ();

		menu->addAction (ActionToggleSearch_);

		menu->setAttribute (Qt::WA_DeleteOnClose);

		menu->exec (Ui_.Playlist_->viewport ()->mapToGlobal (pos));
	}

	void PlaylistWidget::handleChangePlayMode ()
	{
		auto mode = sender ()->property ("PlayMode").toInt ();
		Player_->SetPlayMode (static_cast<Player::PlayMode> (mode));
		XmlSettingsManager::Instance ().setProperty ("PlayMode", mode);
	}

	void PlaylistWidget::handlePlayModeChanged (Player::PlayMode mode)
	{
		Q_FOREACH (QAction *action, PlayModesGroup_->actions ())
			if (action->property ("PlayMode").toInt () == static_cast<int> (mode))
			{
				action->setChecked (true);
				return;
			}
	}

	void PlaylistWidget::play (const QModelIndex& index)
	{
		Player_->play (PlaylistFilter_->mapToSource (index));
	}

	void PlaylistWidget::expand (const QModelIndex& index)
	{
		Ui_.Playlist_->expand (PlaylistFilter_->mapFromSource (index));
	}

	void PlaylistWidget::handleBufferStatus (int status)
	{
		Ui_.BufferProgress_->setValue (status);
		Ui_.BufferProgress_->setVisible (status > 0 && status < 100);
	}

	void PlaylistWidget::handleStdSort ()
	{
		const auto& intVars = sender ()->property ("SortInts").toList ();
		QList<Player::SortingCriteria> criteria;
		std::transform (intVars.begin (), intVars.end (), std::back_inserter (criteria),
				[] (decltype (intVars.front ()) var)
					{ return static_cast<Player::SortingCriteria> (var.toInt ()); });
		Player_->SetSortingCriteria (criteria);
	}

	void PlaylistWidget::removeSelectedSongs ()
	{
		auto selModel = Ui_.Playlist_->selectionModel ();
		if (!selModel)
			return;

		auto indexes = selModel->selectedRows ();
		if (indexes.isEmpty ())
			indexes << Ui_.Playlist_->currentIndex ();
		indexes.removeAll (QModelIndex ());

		QList<Phonon::MediaSource> removedSources;
		const QString& title = indexes.size () == 1 ?
				tr ("Remove %1").arg (indexes.at (0).data ().toString ()) :
				tr ("Remove %n song(s)", 0, indexes.size ());

		Q_FOREACH (const auto& idx, indexes)
			removedSources << Player_->GetIndexSources (PlaylistFilter_->mapToSource (idx));

		auto cmd = new PlaylistUndoCommand (title, removedSources, Player_);
		UndoStack_->push (cmd);
	}

	void PlaylistWidget::setStopAfterSelected ()
	{
		auto index = PlaylistFilter_->mapToSource (Ui_.Playlist_->currentIndex ());
		if (!index.isValid ())
			return;

		Player_->SetStopAfter (index);
	}

	void PlaylistWidget::showTrackProps ()
	{
		const auto& index = Ui_.Playlist_->currentIndex ();
		const auto& info = index.data (Player::Role::Info).value<MediaInfo> ();
		if (info.LocalPath_.isEmpty ())
			return;

		AudioPropsWidget::MakeDialog ()->SetProps (info);
	}

	void PlaylistWidget::showAlbumArt ()
	{
		const auto& index = Ui_.Playlist_->currentIndex ();
		const auto& info = index.data (Player::Role::Info).value<MediaInfo> ();

		ShowAlbumArt (info.LocalPath_, QCursor::pos ());
	}

	void PlaylistWidget::handleMoveUp ()
	{
		const auto& selected = Ui_.Playlist_->selectionModel ()->selectedRows ();
		QList<Phonon::MediaSource> sources;
		Q_FOREACH (const auto& index, selected)
			sources += Player_->GetIndexSources (PlaylistFilter_->mapToSource (index));

		if (sources.isEmpty ())
			return;

		auto allSrcs = Player_->GetQueue ();
		for (int i = 1, size = allSrcs.size (); i < size; ++i)
			if (sources.contains (allSrcs.at (i)))
				std::swap (allSrcs [i], allSrcs [i - 1]);

		Player_->ReplaceQueue (allSrcs, false);

		SelectSources (sources);
	}

	void PlaylistWidget::handleMoveDown ()
	{
		const auto& selected = Ui_.Playlist_->selectionModel ()->selectedRows ();
		QList<Phonon::MediaSource> sources;
		Q_FOREACH (const auto& index, selected)
			sources += Player_->GetIndexSources (PlaylistFilter_->mapToSource (index));

		if (sources.isEmpty ())
			return;

		auto allSrcs = Player_->GetQueue ();
		for (int i = allSrcs.size () - 2; i >= 0; --i)
			if (sources.contains (allSrcs.at (i)))
				std::swap (allSrcs [i], allSrcs [i + 1]);

		Player_->ReplaceQueue (allSrcs, false);

		SelectSources (sources);
	}

	void PlaylistWidget::handleSavePlaylist ()
	{
		const auto& name = QInputDialog::getText (this,
				tr ("Save playlist"),
				tr ("Enter name for the playlist:"));
		if (name.isEmpty ())
			return;

		auto mgr = Core::Instance ().GetPlaylistManager ()->GetStaticManager ();

		if (mgr->EnumerateCustomPlaylists ().contains (name) &&
				QMessageBox::question (this,
						"LeechCraft",
						tr ("Playlist %1 already exists. Do you want to overwrite it?")
							.arg ("<em>" + name + "</em>"),
						QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
			return;

		mgr->SaveCustomPlaylist (name, Player_->GetQueue ());
	}

	void PlaylistWidget::loadFromDisk ()
	{
		auto prevPath = XmlSettingsManager::Instance ()
				.Property ("PrevAddToPlaylistPath", QDir::homePath ()).toString ();
		const auto& files = QFileDialog::getOpenFileNames (this,
				tr ("Load files"),
				prevPath,
				tr ("Music files (*.ogg *.flac *.mp3 *.wav);;Playlists (*.pls *.m3u *.m3u8 *.xspf);;All files (*.*)"));
		if (files.isEmpty ())
			return;

		prevPath = QFileInfo (files.at (0)).absoluteDir ().absolutePath ();
		XmlSettingsManager::Instance ().setProperty ("PrevAddToPlaylistPath", prevPath);

		Player_->Enqueue (files);
	}

	void PlaylistWidget::addURL ()
	{
		auto cb = qApp->clipboard ();
		QString textCb = cb->text (QClipboard::Selection);
		if (textCb.isEmpty () || !QUrl (textCb).isValid ())
			textCb = cb->text (QClipboard::Selection);
		if (!QUrl (textCb).isValid ())
			textCb.clear ();

		const auto& url = QInputDialog::getText (this,
				"LeechCraft",
				tr ("Enter URL to add to the play queue:"),
				QLineEdit::Normal,
				textCb);
		if (url.isEmpty ())
			return;

		QUrl urlObj (url);
		if (!urlObj.isValid ())
		{
			QMessageBox::warning (this,
					"LeechCraft",
					tr ("Invalid URL."));
			return;
		}

		Player_->Enqueue (QList<Phonon::MediaSource> () << urlObj);
	}

	void PlaylistWidget::updateStatsLabel ()
	{
		const int tracksCount = Player_->GetQueue ().size ();

		auto model = Player_->GetPlaylistModel ();
		int length = 0;
		for (int i = 0, rc = model->rowCount (); i < rc; ++i)
		{
			const auto& idx = model->index (i, 0);
			length += model->rowCount (idx) ?
					idx.data (Player::Role::AlbumLength).toInt () :
					idx.data (Player::Role::Info).value<MediaInfo> ().Length_;
		}

		Ui_.StatsLabel_->setText (tr ("%n track(s), total duration: %1", 0, tracksCount)
					.arg (Util::MakeTimeFromLong (length)));
	}
}
}
