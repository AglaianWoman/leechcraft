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

#include <stdexcept>
#include <algorithm>
#include <QtDebug>
#include <QApplication>
#include <QFont>
#include <QPalette>
#include <QIcon>
#include <QFontMetrics>
#include <interfaces/core/icoreproxy.h>
#include <interfaces/core/itagsmanager.h>
#include "channelsmodel.h"
#include "item.h"
#include "xmlsettingsmanager.h"
#include "core.h"
#include "storagebackendmanager.h"

namespace LeechCraft
{
namespace Aggregator
{
	ChannelsModel::ChannelsModel (QObject *parent)
	: QAbstractItemModel (parent)
	{
		Headers_ << tr ("Feed")
			<< tr ("Unread items")
			<< tr ("Last build");

		connect (&StorageBackendManager::Instance (),
				&StorageBackendManager::channelRemoved,
				this,
				&ChannelsModel::RemoveChannel);
		connect (&StorageBackendManager::Instance (),
				&StorageBackendManager::feedRemoved,
				this,
				&ChannelsModel::RemoveFeed);
		connect (&StorageBackendManager::Instance (),
				&StorageBackendManager::channelAdded,
				this,
				[this] (const Channel& channel) { AddChannel (channel.ToShort ()); });
		connect (&StorageBackendManager::Instance (),
				&StorageBackendManager::channelDataUpdated,
				this,
				&ChannelsModel::UpdateChannelData,
				Qt::QueuedConnection);
		connect (&StorageBackendManager::Instance (),
				&StorageBackendManager::storageCreated,
				this,
				&ChannelsModel::PopulateChannels);
	}

	int ChannelsModel::columnCount (const QModelIndex&) const
	{
		return Headers_.size ();
	}

	namespace
	{
		QVariant GetForegroundColor (const ChannelShort& cs)
		{
			bool palette = XmlSettingsManager::Instance ()->property ("UsePaletteColors").toBool ();
			if (cs.Unread_)
			{
				if (XmlSettingsManager::Instance ()->property ("UnreadCustomColor").toBool ())
					return XmlSettingsManager::Instance ()->property ("UnreadItemsColor");
				else
					return palette ?
							QApplication::palette ().link ().color () :
							QVariant ();
			}
			else
				return palette ?
						QApplication::palette ().linkVisited ().color () :
						QVariant ();
		}

		QVariant GetTooltip (const ChannelShort& cs)
		{
			auto result = QString ("<qt><b>%1</b><br />").arg (cs.Title_);
			if (cs.Author_.size ())
			{
				result += ChannelsModel::tr ("<strong>Author</strong>: %1").arg (cs.Author_);
				result += "<br />";
			}
			if (cs.Tags_.size ())
			{
				const auto& hrTags = Core::Instance ().GetProxy ()->GetTagsManager ()->GetTags (cs.Tags_);
				result += ChannelsModel::tr ("<b>Tags</b>: %1").arg (hrTags.join ("; "));
				result += "<br />";
			}
			QString elidedLink = QApplication::fontMetrics ().elidedText (cs.Link_, Qt::ElideMiddle, 400);
			result += QString ("<a href='%1'>%2</a>")
					.arg (cs.Link_)
					.arg (elidedLink);
			result += "</qt>";
			return result;
		}
	}

	QVariant ChannelsModel::data (const QModelIndex& index, int role) const
	{
		if (!index.isValid ())
			return QVariant ();

		const auto row = index.row ();
		const auto column = index.column ();
		switch (role)
		{
		case Qt::DisplayRole:
			switch (column)
			{
			case ColumnTitle:
				return Channels_.at (row).DisplayTitle_.isEmpty () ?
						Channels_.at (row).Title_ :
						Channels_.at (row).DisplayTitle_;
			case ColumnUnread:
				return Channels_.at (row).Unread_;
			case ColumnLastBuild:
				return Channels_.at (row).LastBuild_;
			default:
				return {};
			}
		case Qt::DecorationRole:
			if (column == ColumnTitle)
			{
				QIcon result = QPixmap::fromImage (Channels_.at (row).Favicon_);
				if (result.isNull ())
					result = QIcon (":/resources/images/rss.png");
				return result;
			}
			else
				return {};
		case Qt::ForegroundRole:
			return GetForegroundColor (Channels_.at (row));
		case Qt::FontRole:
			if (Channels_.at (row).Unread_)
				return XmlSettingsManager::Instance ()->property ("UnreadItemsFont");
			else
				return {};
		case Qt::ToolTipRole:
			return GetTooltip (Channels_.at (row));
		case RoleTags:
			return Channels_.at (row).Tags_;
		case ChannelRoles::UnreadCount:
			return Channels_.at (row).Unread_;
		case ChannelRoles::ChannelID:
			return Channels_.at (row).ChannelID_;
		case ChannelRoles::FeedID:
			return Channels_.at (row).FeedID_;
		case ChannelRoles::HumanReadableTags:
			return Core::Instance ().GetProxy ()->GetTagsManager ()->GetTags (Channels_.at (row).Tags_);
		default:
			return {};
		}
	}

	Qt::ItemFlags ChannelsModel::flags (const QModelIndex& index) const
	{
		if (!index.isValid ())
			return 0;
		else
			return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
	}

	QVariant ChannelsModel::headerData (int column, Qt::Orientation orient, int role) const
	{
		if (orient == Qt::Horizontal && role == Qt::DisplayRole)
			return Headers_.at (column);
		else
			return QVariant ();
	}

	QModelIndex ChannelsModel::index (int row, int column, const QModelIndex& parent) const
	{
		if (!hasIndex (row, column, parent))
			return QModelIndex ();

		return createIndex (row, column);
	}

	QModelIndex ChannelsModel::parent (const QModelIndex&) const
	{
		return QModelIndex ();
	}

	int ChannelsModel::rowCount (const QModelIndex& parent) const
	{
		return parent.isValid () ? 0 : Channels_.size ();
	}

	void ChannelsModel::AddChannel (const ChannelShort& channel)
	{
		beginInsertRows (QModelIndex (), rowCount (), rowCount ());
		Channels_ << channel;
		endInsertRows ();
	}

	const ChannelShort& ChannelsModel::GetChannelForIndex (const QModelIndex& index) const
	{
		if (!index.isValid ())
			throw std::runtime_error ("Invalid index");
		else
			return Channels_ [index.row ()];
	}

	void ChannelsModel::Clear ()
	{
		beginResetModel ();
		Channels_.clear ();
		endResetModel ();
	}

	void ChannelsModel::RemoveChannel (IDType_t id)
	{
		const auto pos = std::find_if (Channels_.begin (), Channels_.end (),
				[id] (const ChannelShort& cs) { return cs.ChannelID_ == id; });
		if (pos == Channels_.end ())
			return;

		const auto idx = pos - Channels_.begin ();
		beginRemoveRows ({}, idx, idx);
		Channels_.erase (pos);
		endRemoveRows ();
	}

	void ChannelsModel::RemoveFeed (IDType_t id)
	{
		for (auto it = Channels_.begin (); it != Channels_.end ();)
		{
			if (it->FeedID_ != id)
			{
				++it;
				continue;
			}

			const auto idx = it - Channels_.begin ();
			beginRemoveRows ({}, idx, idx);
			it = Channels_.erase (it);
			endRemoveRows ();
		}
	}

	void ChannelsModel::UpdateChannelData (const Channel& channel)
	{
		const auto cid = channel.ChannelID_;

		const auto pos = std::find_if (Channels_.begin (), Channels_.end (),
				[cid] (const ChannelShort& cs) { return cs.ChannelID_ == cid; });
		if (pos == Channels_.end ())
			return;

		*pos = channel.ToShort ();
		pos->Unread_ = StorageBackendManager::Instance ().MakeStorageBackendForThread ()->GetUnreadItems (cid);

		const auto idx = pos - Channels_.begin ();
		emit dataChanged (index (idx, 0), index (idx, 2));
	}

	void ChannelsModel::PopulateChannels ()
	{
		Clear ();

		auto storage = StorageBackendManager::Instance ().MakeStorageBackendForThread ();
		for (const auto feedId : storage->GetFeedsIDs ())
			for (const auto& chan : storage->GetChannels (feedId))
				AddChannel (chan);
	}
}
}
