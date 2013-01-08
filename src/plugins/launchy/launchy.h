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

#pragma once

#include <QObject>
#include <interfaces/iinfo.h>
#include <interfaces/iactionsexporter.h>
#include <interfaces/iquarkcomponentprovider.h>

namespace LeechCraft
{
namespace Launchy
{
	class ItemsFinder;
	class FavoritesManager;

	class Plugin : public QObject
				 , public IInfo
				 , public IActionsExporter
				 , public IQuarkComponentProvider
	{
		Q_OBJECT
		Q_INTERFACES (IInfo IActionsExporter IQuarkComponentProvider)

		ICoreProxy_ptr Proxy_;
		ItemsFinder *Finder_;
		FavoritesManager *FavManager_;
		QAction *FSLauncher_;

		QuarkComponent LaunchQuark_;
	public:
		void Init (ICoreProxy_ptr);
		void SecondInit ();
		QByteArray GetUniqueID () const;
		void Release ();
		QString GetName () const;
		QString GetInfo () const;
		QIcon GetIcon () const;

		QList<QAction*> GetActions (ActionsEmbedPlace) const;

		QuarkComponents_t GetComponents () const;
	private slots:
		void handleFSRequested ();
	signals:
		void gotEntity (const LeechCraft::Entity&);
		void gotActions (QList<QAction*>, LeechCraft::ActionsEmbedPlace);
	};
}
}
