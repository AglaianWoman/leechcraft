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

#include "packagesmodel.h"
#include <QIcon>
#include <QApplication>
#include <util/util.h>
#include "core.h"
#include "storage.h"
#include "pendingmanager.h"

namespace LeechCraft
{
namespace LackMan
{
	PackagesModel::PackagesModel (QObject *parent)
	: QAbstractItemModel (parent)
	{
	}

	int PackagesModel::columnCount (const QModelIndex&) const
	{
		return Columns::MaxColumn;
	}

	QVariant PackagesModel::headerData (int section, Qt::Orientation orientation, int role) const
	{
		if (orientation == Qt::Vertical)
			return QVariant ();

		if (role != Qt::DisplayRole)
			return QVariant ();

		switch (section)
		{
		case Columns::Inst:
			return tr ("I");
		case Columns::Upd:
			return tr ("U");
		case Columns::Name:
			return tr ("Name");
		case Columns::Description:
			return tr ("Description");
		case Columns::Version:
			return tr ("Version");
		case Columns::Size:
			return tr ("Size");
		default:
			return "unknown";
		}
	}

	QVariant PackagesModel::data (const QModelIndex& index, int role) const
	{
		const auto& lpi = Packages_.at (index.row ());

		const int col = index.column ();

		switch (role)
		{
		case Qt::DisplayRole:
			switch (col)
			{
			case Columns::Name:
				return lpi.Name_;
			case Columns::Description:
				return lpi.ShortDescription_;
			case Columns::Version:
				return lpi.Version_;
			case Columns::Size:
			{
				const auto size = Core::Instance ().GetStorage ()->GetPackageSize (lpi.PackageID_);
				return size > 0 ? Util::MakePrettySize (size) : tr ("unknown");
			}
			default:
				return QVariant ();
			}
		case Qt::DecorationRole:
			return col != Columns::Name ?
					QVariant () :
					Core::Instance ().GetIconForLPI (lpi);
		case Qt::CheckStateRole:
		{
			auto pm = Core::Instance ().GetPendingManager ();
			switch (col)
			{
			case Columns::Inst:
				if (lpi.IsInstalled_)
					return pm->GetPendingRemove ().contains (lpi.PackageID_) ?
							Qt::Unchecked :
							Qt::Checked;
				else
					return pm->GetPendingInstall ().contains (lpi.PackageID_) ?
							Qt::Checked :
							Qt::Unchecked;
				break;
			case Columns::Upd:
				if (!lpi.HasNewVersion_)
					return QVariant ();
				return pm->GetPendingUpdate ().contains (lpi.PackageID_) ?
						Qt::Checked :
						Qt::Unchecked;
			default:
				return QVariant ();
			}
		}
		case PMRPackageID:
			return lpi.PackageID_;
		case PMRShortDescription:
			return lpi.ShortDescription_;
		case PMRLongDescription:
			return lpi.LongDescription_;
		case PMRTags:
			return lpi.Tags_;
		case PMRInstalled:
			return lpi.IsInstalled_;
		case PMRUpgradable:
			return lpi.HasNewVersion_;
		case PMRVersion:
			return lpi.Version_;
		case PMRThumbnails:
		case PMRScreenshots:
		{
			const auto& images = Core::Instance ().GetStorage ()->GetImages (lpi.Name_);
			QStringList result;
			Q_FOREACH (const Image& img, images)
				if (img.Type_ == (role == PMRThumbnails ? Image::TThumbnail : Image::TScreenshot))
					result << img.URL_;
			return result;
		}
		case PMRSize:
			return Core::Instance ().GetStorage ()->GetPackageSize (lpi.PackageID_);
		default:
			return QVariant ();
		}
	}

	bool PackagesModel::setData (const QModelIndex& index, const QVariant& value, int role)
	{
		if (role != Qt::CheckStateRole)
			return false;

		const auto& lpi = Packages_.at (index.row ());

		const Qt::CheckState state = static_cast<Qt::CheckState> (value.toInt ());
		switch (index.column ())
		{
		case Columns::Inst:
		{
			const bool isNewState = (state == Qt::Checked && !lpi.IsInstalled_) ||
					(state == Qt::Unchecked && lpi.IsInstalled_);
			Core::Instance ().GetPendingManager ()->
					ToggleInstallRemove (lpi.PackageID_, isNewState, lpi.IsInstalled_);
			emit dataChanged (index, index);
			return true;
		}
		case Columns::Upd:
			Core::Instance ().GetPendingManager ()->ToggleUpdate (lpi.PackageID_, state == Qt::Checked);
			emit dataChanged (index, index);
			return true;
		default:
			return false;
		}
	}

	Qt::ItemFlags PackagesModel::flags (const QModelIndex& index) const
	{
		if (!index.isValid ())
			return 0;

		auto flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
		const auto& lpi = Packages_.at (index.row ());
		switch (index.column ())
		{
		case Columns::Inst:
			flags |= Qt::ItemIsUserCheckable;
			break;
		case Columns::Upd:
			if (lpi.HasNewVersion_)
				flags |= Qt::ItemIsUserCheckable;
			break;
		}
		return flags;
	}

	QModelIndex PackagesModel::index (int row, int column, const QModelIndex& parent) const
	{
		if (!hasIndex (row, column, parent))
			return QModelIndex ();

		return createIndex (row, column);
	}

	QModelIndex PackagesModel::parent (const QModelIndex&) const
	{
		return QModelIndex ();
	}

	int PackagesModel::rowCount (const QModelIndex& parent) const
	{
		return parent.isValid () ? 0 : Packages_.size ();
	}

	void PackagesModel::AddRow (const ListPackageInfo& lpi)
	{
		int size = Packages_.size ();
		beginInsertRows (QModelIndex (), size, size);
		Packages_ << lpi;
		endInsertRows ();
	}

	void PackagesModel::UpdateRow (const ListPackageInfo& lpi)
	{
		for (int i = 0, size = Packages_.size ();
				i < size; ++i)
			if (Packages_.at (i).Name_ == lpi.Name_)
			{
				Packages_ [i] = lpi;
				emit dataChanged (index (i, 0),
						index (i, columnCount () - 1));
				break;
			}
	}

	void PackagesModel::RemovePackage (int packageId)
	{
		for (int i = 0, size = Packages_.size ();
				i < size; ++i)
			if (Packages_.at (i).PackageID_ == packageId)
			{
				beginRemoveRows (QModelIndex (), i, i);
				Packages_.removeAt (i);
				endRemoveRows ();
				break;
			}
	}

	ListPackageInfo PackagesModel::FindPackage (const QString& name) const
	{
		Q_FOREACH (const ListPackageInfo& lpi, Packages_)
			if (lpi.Name_ == name)
				return lpi;

		return ListPackageInfo ();
	}

	int PackagesModel::GetRow (int packageId) const
	{
		for (int i = 0, size = Packages_.size (); i < size; ++i)
			if (Packages_.at (i).PackageID_ == packageId)
				return i;

		return -1;
	}

	void PackagesModel::Clear ()
	{
		Packages_.clear ();
		reset ();
	}

	void PackagesModel::handlePackageUpdateToggled (int id)
	{
		const auto row = GetRow (id);
		if (row == -1)
		{
			qWarning () << Q_FUNC_INFO
					<< "unknown package ID"
					<< id;
			return;
		}

		const auto& idx = index (row, Columns::Upd);
		emit dataChanged (idx, idx);
	}
}
}
