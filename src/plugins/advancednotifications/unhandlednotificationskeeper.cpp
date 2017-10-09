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

#include "unhandlednotificationskeeper.h"
#include <QStandardItemModel>
#include <interfaces/structures.h>
#include <interfaces/core/icoreproxy.h>
#include <interfaces/core/ipluginsmanager.h>
#include <interfaces/an/entityfields.h>
#include <interfaces/an/ianemitter.h>
#include <util/xpc/anutil.h>
#include <util/xpc/stdanfields.h>
#include <util/sll/prelude.h>
#include <util/sll/qtutil.h>
#include "notificationrule.h"

namespace LeechCraft
{
namespace AdvancedNotifications
{
	UnhandledNotificationsKeeper::UnhandledNotificationsKeeper (const ICoreProxy_ptr& proxy, QObject *parent)
	: QObject { parent }
	, Proxy_ { proxy }
	, Model_ { new QStandardItemModel { this } }
	{
		Model_->setHorizontalHeaderLabels ({ tr ("Title"), tr ("Text"), tr ("Category"), tr ("Type") });
	}

	void UnhandledNotificationsKeeper::AddUnhandled (const Entity& e)
	{
		while (Model_->rowCount () >= 1000)
			Model_->removeRow (0);

		const auto& category = e.Additional_ [AN::EF::EventCategory].toString ();
		const auto& type = e.Additional_ [AN::EF::EventType].toString ();

		QList<QStandardItem*> row
		{
			new QStandardItem { e.Entity_.toString () },
			new QStandardItem { e.Additional_ [AN::EF::FullText].toString () },
			new QStandardItem { Util::AN::GetCategoryName (category) },
			new QStandardItem { Util::AN::GetTypeName (type) }
		};
		for (const auto item : row)
			item->setEditable (false);

		row [0]->setData (QVariant::fromValue (e));

		auto possibleFields = Util::GetStdANFields (category) + Util::GetStdANFields (type);
		const auto& sender = e.Additional_ ["org.LC.AdvNotifications.SenderID"].toByteArray ();
		if (const auto iane = qobject_cast<IANEmitter*> (Proxy_->GetPluginsManager ()->GetPluginByID (sender)))
			possibleFields += iane->GetANFields ();

		for (const auto& fieldData : possibleFields)
			if (e.Additional_.contains (fieldData.ID_))
			{
				QList<QStandardItem*> subrow
				{
					new QStandardItem { fieldData.Name_ },
					new QStandardItem { e.Additional_ [fieldData.ID_].toString () },
					new QStandardItem { fieldData.Description_ }
				};

				for (const auto item : subrow)
					item->setEditable (false);

				subrow [0]->setData (fieldData.ID_);

				row [0]->appendRow (subrow);
			}

		Model_->insertRow (0, row);
	}

	QAbstractItemModel* UnhandledNotificationsKeeper::GetUnhandledModel () const
	{
		return Model_;
	}

	namespace
	{
		auto BuildHierarchy (const QList<QStandardItem*>& allItems)
		{
			QHash<QStandardItem*, QList<QStandardItem*>> result;
			for (const auto& item : allItems)
				if (const auto& parent = item->parent ())
				{
					if (allItems.contains (parent))
						result [parent] << item;
				}
				else
					result.insert (item, {});
			return result;
		}
	}

	QList<Entity> UnhandledNotificationsKeeper::GetRulesEntities (const QList<QModelIndex>& idxs) const
	{
		QList<Entity> result;

		const auto& allItems = Util::Map (idxs,
				[this] (const QModelIndex& idx) { return Model_->itemFromIndex (idx); });
		const auto& hierarchy = BuildHierarchy (allItems);

		for (const auto& pair : Util::Stlize (hierarchy))
		{
			const auto& entityNode = pair.first;
			const auto& fieldNodes = pair.second;
			auto entity = entityNode->data ().value<Entity> ();
			for (int i = 0; i < entityNode->rowCount (); ++i)
			{
				const auto& fieldNode = entityNode->child (i);
				if (!fieldNodes.contains (fieldNode))
					entity.Additional_.remove (fieldNode->data ().toString ());
			}

			result << entity;
		}

		return result;
	}
}
}
