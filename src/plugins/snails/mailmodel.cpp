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

#include "mailmodel.h"
#include <QIcon>
#include <util/util.h>
#include <util/sll/prelude.h>
#include <util/models/modelitembase.h>
#include <interfaces/core/iiconthememanager.h>
#include "core.h"
#include "messagelistactionsmanager.h"

namespace LeechCraft
{
namespace Snails
{
	struct MailModel::TreeNode : Util::ModelItemBase<MailModel::TreeNode>
	{
		Message_ptr Msg_;

		QSet<QByteArray> UnreadChildren_;

		bool IsAvailable_ = true;

		bool IsChecked_ = false;

		TreeNode () = default;

		TreeNode (const Message_ptr& msg, const TreeNode_ptr& parent)
		: Util::ModelItemBase<TreeNode> { parent }
		, Msg_ { msg }
		{
		}

		void SetParent (const TreeNode_wptr& parent)
		{
			Parent_ = parent;
		}

		template<typename G, typename F>
		std::result_of_t<G (const TreeNode*)> Fold (const G& g, const F& f) const
		{
			const auto& values = Util::Map (Children_, [&] (const TreeNode_ptr& c) { return c->Fold (g, f); });
			return std::accumulate (values.begin (), values.end (), g (this), f);
		}
	};

	MailModel::MailModel (const MessageListActionsManager *actsMgr, QObject *parent)
	: QAbstractItemModel { parent }
	, ActionsMgr_ { actsMgr }
	, Headers_ { tr ("From"), {}, {}, tr ("Subject"), tr ("Date"), tr ("Size") }
	, Folder_ { "INBOX" }
	, Root_ { std::make_shared<TreeNode> () }
	{
	}

	QVariant MailModel::headerData (int section, Qt::Orientation orient, int role) const
	{
		if (orient != Qt::Horizontal || role != Qt::DisplayRole)
			return {};

		return Headers_.value (section);
	}

	int MailModel::columnCount (const QModelIndex&) const
	{
		return Headers_.size ();
	}

	namespace
	{
		QString GetNeatDate (QDateTime dt)
		{
			dt = dt.toLocalTime ();

			const auto& now = QDateTime::currentDateTime ();
			if (dt.secsTo (now) < 60 * 60 * 24)
				return dt.toLocalTime ().time ().toString ("HH:mm");

			const auto days = dt.daysTo (now);
			if (days < 7)
				return dt.toString ("dddd, HH:mm");

			if (days < 365)
				return dt.date ().toString ("dd MMMM");

			return dt.date ().toString (Qt::DefaultLocaleLongDate);
		}
	}

	QVariant MailModel::data (const QModelIndex& index, int role) const
	{
		const auto structItem = static_cast<TreeNode*> (index.internalPointer ());
		if (structItem == Root_.get ())
			return {};

		const auto& msg = structItem->Msg_;

		const auto column = static_cast<Column> (index.column ());

		switch (role)
		{
		case MessageActions:
			return QVariant::fromValue (MsgId2Actions_.value (msg->GetFolderID ()));
		case Qt::DisplayRole:
		case Sort:
			break;
		case Qt::CheckStateRole:
			return structItem->IsChecked_ ?
					Qt::Checked :
					Qt::Unchecked;
		case Qt::TextAlignmentRole:
		{
			switch (column)
			{
			case Column::StatusIcon:
			case Column::UnreadChildren:
				return Qt::AlignHCenter;
			default:
				return {};
			}
		}
		case Qt::DecorationRole:
		{
			QString iconName;

			switch (column)
			{
			case Column::StatusIcon:
				if (!msg->IsRead ())
					iconName = "mail-unread-new";
				else if (structItem->UnreadChildren_.size ())
					iconName = "mail-unread";
				else
					iconName = "mail-read";
				break;
			default:
				break;
			}

			if (iconName.isEmpty ())
				return {};

			return Core::Instance ().GetProxy ()->GetIconThemeManager ()->GetIcon (iconName);
		}
		case ID:
			return msg->GetFolderID ();
		case IsRead:
			return msg->IsRead ();
		case UnreadChildrenCount:
			return structItem->UnreadChildren_.size ();
		case TotalChildrenCount:
			return structItem->Fold ([] (const TreeNode *n) { return n->GetRowCount (); },
					[] (int a, int b) { return a + b; });
		default:
			return {};
		}

		switch (column)
		{
		case Column::From:
		{
			const auto& addr = msg->GetAddress (Message::Address::From);
			return addr.first.isEmpty () ? addr.second : addr.first;
		}
		case Column::Subject:
		{
			const auto& subject = msg->GetSubject ();
			return subject.isEmpty () ? "<" + tr ("No subject") + ">" : subject;
		}
		case Column::Date:
		{
			const auto& date = role != Sort || index.parent ().isValid () ?
						msg->GetDate () :
						structItem->Fold ([] (const TreeNode *n) { return n->Msg_->GetDate (); },
								[] (auto d1, auto d2) { return std::max (d1, d2); });
			if (role == Sort)
				return date;
			else
				return GetNeatDate (date);
		}
		case Column::Size:
			if (role == Sort)
				return msg->GetSize ();
			else
				return Util::MakePrettySize (msg->GetSize ());
		case Column::UnreadChildren:
			if (const auto unread = structItem->UnreadChildren_.size ())
				return unread;

			return role == Sort ? 0 : QString::fromUtf8 ("·");
		case Column::StatusIcon:
			break;
		}

		return {};
	}

	Qt::ItemFlags MailModel::flags (const QModelIndex& index) const
	{
		auto flags = QAbstractItemModel::flags (index);
		if (index.column () == static_cast<int> (Column::Subject))
			flags |= Qt::ItemIsEditable;

		const auto structItem = static_cast<TreeNode*> (index.internalPointer ());
		if (!structItem->IsAvailable_)
			flags &= ~Qt::ItemIsEnabled;

		return flags;
	}

	QModelIndex MailModel::index (int row, int column, const QModelIndex& parent) const
	{
		const auto structItem = parent.isValid () ?
				static_cast<TreeNode*> (parent.internalPointer ()) :
				Root_.get ();
		const auto childItem = structItem->GetChild (row).get ();
		if (!childItem)
			return {};

		return createIndex (row, column, childItem);
	}

	QModelIndex MailModel::parent (const QModelIndex& index) const
	{
		if (!index.isValid ())
			return {};

		const auto structItem = static_cast<TreeNode*> (index.internalPointer ());
		const auto& parentItem = structItem->GetParent ();
		if (parentItem == Root_)
			return {};

		return createIndex (parentItem->GetRow (), 0, parentItem.get ());
	}

	int MailModel::rowCount (const QModelIndex& parent) const
	{
		const auto structItem = parent.isValid () ?
				static_cast<TreeNode*> (parent.internalPointer ()) :
				Root_.get ();
		return structItem->GetRowCount ();
	}

	bool MailModel::setData (const QModelIndex& index, const QVariant& value, int role)
	{
		if (role != Qt::CheckStateRole)
			return false;

		if (!index.isValid ())
			return false;

		const auto structItem = static_cast<TreeNode*> (index.internalPointer ());
		structItem->IsChecked_ = value.toInt () == Qt::Checked;

		emit dataChanged (index, index);

		emit messagesSelectionChanged ();

		return true;
	}

	void MailModel::SetFolder (const QStringList& folder)
	{
		Folder_ = folder;
	}

	QStringList MailModel::GetCurrentFolder () const
	{
		return Folder_;
	}

	Message_ptr MailModel::GetMessage (const QByteArray& id) const
	{
		return GetMessageByFolderId (id);
	}

	void MailModel::Clear ()
	{
		if (Messages_.isEmpty ())
			return;

		beginRemoveRows ({}, 0, Messages_.size () - 1);
		Messages_.clear ();
		Root_->EraseChildren (Root_->begin (), Root_->end ());
		FolderId2Nodes_.clear ();
		MsgId2FolderId_.clear ();
		endRemoveRows ();

		MsgId2Actions_.clear ();
	}

	void MailModel::Append (QList<Message_ptr> messages)
	{
		if (messages.isEmpty ())
			return;

		for (auto i = messages.begin (); i != messages.end (); )
		{
			const auto& msg = *i;

			if (!msg->GetFolders ().contains (Folder_))
			{
				i = messages.erase (i);
				continue;
			}

			if (Update (msg))
			{
				i = messages.erase (i);
				continue;
			}

			++i;
		}

		if (messages.isEmpty ())
			return;

		std::sort (messages.begin (), messages.end (),
				[] (const Message_ptr& left, const Message_ptr& right)
					{ return left->GetDate () < right->GetDate (); });

		Messages_ += messages;

		for (const auto& msg : messages)
		{
			const auto& msgId = msg->GetMessageID ();
			if (!msgId.isEmpty ())
				MsgId2FolderId_ [msgId] = msg->GetFolderID ();

			const auto& acts = ActionsMgr_->GetMessageActions (msg);
			if (!acts.isEmpty ())
				MsgId2Actions_ [msg->GetFolderID ()] = acts;
		}

		for (const auto& msg : messages)
			if (!AppendStructured (msg))
			{
				const auto node = std::make_shared<TreeNode> (msg, Root_);

				const auto childrenCount = Root_->GetRowCount ();
				beginInsertRows ({}, childrenCount, childrenCount);
				Root_->AppendExisting (node);
				FolderId2Nodes_ [msg->GetFolderID ()] << node;
				endInsertRows ();
			}

		emit messageListUpdated ();
	}

	bool MailModel::Update (const Message_ptr& msg)
	{
		const auto pos = std::find_if (Messages_.begin (), Messages_.end (),
				[&msg] (const Message_ptr& other)
					{ return other->GetFolderID () == msg->GetFolderID (); });
		if (pos == Messages_.end ())
			return false;

		if (*pos != msg)
		{
			const auto readChanged = (*pos)->IsRead () != msg->IsRead ();

			*pos = msg;
			for (const auto& indexPair : GetIndexes (msg->GetFolderID (), { 0, columnCount () - 1 }))
			{
				for (const auto& index : indexPair)
					static_cast<TreeNode*> (index.internalPointer ())->Msg_ = msg;
				emit dataChanged (indexPair.value (0), indexPair.value (1));
			}

			if (readChanged)
				UpdateParentReadCount (msg->GetFolderID (), !msg->IsRead ());
		}

		return true;
	}

	bool MailModel::Remove (const QByteArray& id)
	{
		const auto msgPos = std::find_if (Messages_.begin (), Messages_.end (),
				[&id] (const Message_ptr& other) { return other->GetFolderID () == id; });
		if (msgPos == Messages_.end ())
			return false;

		UpdateParentReadCount (id, false);

		for (const auto& node : FolderId2Nodes_.value (id))
			RemoveNode (node);

		FolderId2Nodes_.remove (id);
		MsgId2FolderId_.remove ((*msgPos)->GetMessageID ());
		Messages_.erase (msgPos);

		return true;
	}

	void MailModel::MarkUnavailable (const QList<QByteArray>& ids)
	{
		for (const auto& id : ids)
			for (const auto& node : FolderId2Nodes_.value (id))
			{
				if (!node->IsAvailable_)
					continue;

				node->IsAvailable_ = false;
				EmitRowChanged (node);
			}
	}

	QList<QByteArray> MailModel::GetCheckedIds () const
	{
		QList<QByteArray> result;

		for (const auto& list : FolderId2Nodes_)
			for (const auto& node : list)
				if (node->IsChecked_)
					result << node->Msg_->GetFolderID ();

		std::sort (result.begin (), result.end ());
		result.erase (std::unique (result.begin (), result.end ()), result.end ());

		return result;
	}

	bool MailModel::HasCheckedIds () const
	{
		return std::any_of (FolderId2Nodes_.begin (), FolderId2Nodes_.end (),
				[] (const auto& list)
				{
					return std::any_of (list.begin (), list.end (),
							[] (const auto& node) { return node->IsChecked_; });
				});
	}

	void MailModel::UpdateParentReadCount (const QByteArray& folderId, bool addUnread)
	{
		QList<TreeNode_ptr> nodes;
		for (const auto& node : FolderId2Nodes_.value (folderId))
		{
			const auto& parent = node->GetParent ();
			if (parent != Root_)
				nodes << parent;
		}

		for (int i = 0; i < nodes.size (); ++i)
		{
			const auto& item = nodes.at (i);

			bool emitUpdate = false;
			if (addUnread && !item->UnreadChildren_.contains (folderId))
			{
				item->UnreadChildren_ << folderId;
				emitUpdate = true;
			}
			else if (!addUnread && item->UnreadChildren_.remove (folderId))
				emitUpdate = true;

			if (emitUpdate)
			{
				const auto& leftIdx = createIndex (item->GetRow (), 0, item.get ());
				const auto& rightIdx = createIndex (item->GetRow (), columnCount () - 1, item.get ());
				emit dataChanged (leftIdx, rightIdx);
			}

			const auto& parent = item->GetParent ();
			if (parent != Root_)
				nodes << parent;
		}
	}

	void MailModel::RemoveNode (const TreeNode_ptr& node)
	{
		const auto& parent = node->GetParent ();

		const auto& parentIndex = parent == Root_ ?
				QModelIndex {} :
				createIndex (parent->GetRow (), 0, parent.get ());

		const auto row = node->GetRow ();

		if (const auto childCount = node->GetRowCount ())
		{
			const auto& nodeIndex = createIndex (row, 0, node.get ());

			beginRemoveRows (nodeIndex, 0, childCount - 1);
			auto childNodes = std::move (node->GetChildren ());
			endRemoveRows ();

			for (const auto& childNode : childNodes)
				childNode->SetParent (parent);

			beginInsertRows (parentIndex,
					parent->GetRowCount (),
					parent->GetRowCount () + childCount - 1);
			parent->AppendExisting (childNodes);
			endInsertRows ();
		}

		beginRemoveRows (parentIndex, row, row);
		parent->EraseChild (parent->begin () + row);
		endRemoveRows ();
	}

	bool MailModel::AppendStructured (const Message_ptr& msg)
	{
		auto refs = msg->GetReferences ();
		for (const auto& replyTo : msg->GetInReplyTo ())
			if (!refs.contains (replyTo))
				refs << replyTo;

		if (refs.isEmpty ())
			return false;

		QByteArray folderId;
		for (int i = refs.size () - 1; i >= 0; --i)
		{
			folderId = MsgId2FolderId_.value (refs.at (i));
			if (!folderId.isEmpty ())
				break;
		}
		if (folderId.isEmpty ())
			return false;

		const auto& indexes = GetIndexes (folderId, 0);
		for (const auto& parentIndex : indexes)
		{
			const auto parentNode = static_cast<TreeNode*> (parentIndex.internalPointer ());
			const auto row = parentNode->GetRowCount ();

			const auto node = std::make_shared<TreeNode> (msg, parentNode->shared_from_this ());
			beginInsertRows (parentIndex, row, row);
			parentNode->AppendExisting (node);
			FolderId2Nodes_ [msg->GetFolderID ()] << node;
			endInsertRows ();
		}

		if (!msg->IsRead ())
			UpdateParentReadCount (msg->GetFolderID (), true);

		return !indexes.isEmpty ();
	}

	void MailModel::EmitRowChanged (const TreeNode_ptr& node)
	{
		emit dataChanged (GetIndex (node, 0),
				GetIndex (node, static_cast<int> (Column::MaxNext)));
	}

	QModelIndex MailModel::GetIndex (const TreeNode_ptr& node, int column) const
	{
		return createIndex (node->GetRow (), column, node.get ());
	}

	QList<QModelIndex> MailModel::GetIndexes (const QByteArray& folderId, int column) const
	{
		return Util::Map (FolderId2Nodes_.value (folderId),
				[this, column] (const auto& node) { return GetIndex (node, column); });
	}

	QList<QList<QModelIndex>> MailModel::GetIndexes (const QByteArray& folderId, const QList<int>& columns) const
	{
		return Util::Map (FolderId2Nodes_.value (folderId),
				[this, &columns] (const auto& node)
				{
					return Util::Map (columns,
							[this, &node] (auto column) { return GetIndex (node, column); });
				});
	}

	Message_ptr MailModel::GetMessageByFolderId (const QByteArray& id) const
	{
		const auto pos = std::find_if (Messages_.begin (), Messages_.end (),
				[&id] (const Message_ptr& msg) { return msg->GetFolderID () == id; });
		if (pos == Messages_.end ())
			return {};

		return *pos;
	}
}
}
