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

#include "vrooby.h"
#include <QIcon>
#include <QAction>
#include <util/util.h>
#include <util/xpc/util.h>
#include <util/gui/util.h>
#include <util/gui/unhoverdeletemixin.h>

#ifdef ENABLE_UDISKS
#include "backends/udisks/udisksbackend.h"
#endif
#ifdef ENABLE_UDISKS2
#include "backends/udisks2/udisks2backend.h"
#endif

#include "devbackend.h"
#include "trayview.h"

namespace LeechCraft
{
namespace Vrooby
{
	void Plugin::Init (ICoreProxy_ptr proxy)
	{
		Util::InstallTranslator ("vrooby");

		TrayView_ = new TrayView (proxy);
		new Util::UnhoverDeleteMixin (TrayView_, SLOT (hide ()));

		QList<std::shared_ptr<DevBackend>> candidates;

#ifdef ENABLE_UDISKS2
		candidates << std::make_shared<UDisks2::Backend> ();
#endif
#ifdef ENABLE_UDISKS
		candidates << std::make_shared<UDisks::Backend> ();
#endif

		QStringList allBackends;
		for (const auto& cand : candidates)
		{
			allBackends << cand->GetBackendName ();
			if (cand->IsAvailable ())
			{
				qDebug () << Q_FUNC_INFO
						<< "selecting"
						<< cand->GetBackendName ();
				Backend_ = cand;
				break;
			}
		}

		if (!Backend_)
		{
			const auto& e = Util::MakeNotification ("Vrooby",
					tr ("No backends are available, tried the following: %1.")
						.arg (allBackends.join ("; ")),
					PCritical_);
			QMetaObject::invokeMethod (this,
					"gotEntity",
					Qt::QueuedConnection,
					Q_ARG (LeechCraft::Entity, e));
		}
	}

	void Plugin::SecondInit ()
	{
		if (!Backend_)
			return;

		Backend_->Start ();
		connect (Backend_.get (),
				SIGNAL (gotEntity (LeechCraft::Entity)),
				this,
				SIGNAL (gotEntity (LeechCraft::Entity)));

		TrayView_->SetBackend (Backend_.get ());
		connect (TrayView_,
				SIGNAL (hasItemsChanged ()),
				this,
				SLOT (checkAction ()));

		checkAction ();
	}

	QByteArray Plugin::GetUniqueID () const
	{
		return "org.LeechCraft.Vrooby";
	}

	void Plugin::Release ()
	{
	}

	QString Plugin::GetName () const
	{
		return "Vrooby";
	}

	QString Plugin::GetInfo () const
	{
		return tr ("Removable storage devices manager for LeechCraft.");
	}

	QIcon Plugin::GetIcon () const
	{
		static QIcon icon ("lcicons:/vrooby/resources/images/vrooby.svg");
		return icon;
	}

	bool Plugin::SupportsDevType (DeviceType type) const
	{
		return type == DeviceType::MassStorage;
	}

	QAbstractItemModel* Plugin::GetDevicesModel () const
	{
		return Backend_ ? Backend_->GetDevicesModel () : 0;
	}

	void Plugin::MountDevice (const QString& id)
	{
		if (Backend_)
			Backend_->MountDevice (id);
	}

	QList<QAction*> Plugin::GetActions (ActionsEmbedPlace aep) const
	{
		QList<QAction*> result;
		if (aep == ActionsEmbedPlace::LCTray && ActionDevices_)
			result << ActionDevices_.get ();
		return result;
	}

	void Plugin::checkAction ()
	{
		if (!Backend_)
			return;

		if (TrayView_->HasItems () == static_cast<bool> (ActionDevices_))
			return;

		if (!TrayView_->HasItems ())
		{
			ActionDevices_.reset ();
			return;
		}

		ActionDevices_.reset (new QAction (tr ("Removable devices..."), this));
		ActionDevices_->setProperty ("ActionIcon", "drive-removable-media-usb");

		connect (ActionDevices_.get (),
				SIGNAL (triggered ()),
				this,
				SLOT (showTrayView ()));
		emit gotActions ({ ActionDevices_.get () }, ActionsEmbedPlace::LCTray);
	}

	void Plugin::showTrayView ()
	{
		const auto shouldShow = !TrayView_->isVisible ();
		if (shouldShow)
			TrayView_->move (Util::FitRectScreen (QCursor::pos (),
					TrayView_->size (), Util::FitFlag::NoOverlap));
		TrayView_->setVisible (shouldShow);
	}
}
}

LC_EXPORT_PLUGIN (leechcraft_vrooby, LeechCraft::Vrooby::Plugin);
