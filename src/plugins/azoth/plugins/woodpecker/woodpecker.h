/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2013  Slava Barinov <rayslava@gmail.com>
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

#pragma once

#include <QObject>
#include <interfaces/iinfo.h>
#include <interfaces/ihavetabs.h>
#include <interfaces/iplugin2.h>
#include <interfaces/ientityhandler.h>
#include <interfaces/ihavesettings.h>
#include <interfaces/structures.h>
#include <interfaces/ihaverecoverabletabs.h>
#include "twitterinterface.h"
#include "tweet.h"
#include "xmlsettingsmanager.h"
#include "usermanager.h"

class QTranslator;

namespace LeechCraft
{
namespace Azoth
{
namespace Woodpecker
{
	class UserManager;

	class Plugin	: public QObject
			, public IInfo
			, public IHaveTabs
			, public IHaveSettings
			, public IHaveRecoverableTabs
			, public IPlugin2
	{
		Q_OBJECT
		Q_INTERFACES (IInfo IHaveTabs IHaveSettings IHaveRecoverableTabs IPlugin2)

		LC_PLUGIN_METADATA ("org.LeechCraft.Azoth.WoodPecker")

		QList<QPair<TabClassInfo, std::function<void (TabClassInfo)>>> TabClasses_;
		Util::XmlSettingsDialog_ptr XmlSettingsDialog_;
		UserManager * UserManager_;

		void MakeTab (QWidget*, const TabClassInfo&);

	public:
		TabClassInfo HomeTC_;
		TabClassInfo UserTC_;
		TabClassInfo SearchTC_;
		TabClassInfo FavoriteTC_;

		void Init (ICoreProxy_ptr) override;
		void SecondInit () override;
		void Release () override;
		QByteArray GetUniqueID () const override;
		QString GetName () const override;
		QString GetInfo () const override;
		QIcon GetIcon () const override;

		QSet<QByteArray> GetPluginClasses () const override;

		TabClasses_t GetTabClasses () const override;
		void TabOpenRequested (const QByteArray&) override;
		Util::XmlSettingsDialog_ptr GetSettingsDialog () const override;
		UserManager* GetUserManager () const;

		/**
		 * Tab recovery functions
		 * Feature #1845
		 */
		void RecoverTabs (const QList<TabRecoverInfo>& infos) override;
		bool HasSimilarTab (const QByteArray& data, const QList<QByteArray>&) const override;

		/** @brief Create new tab with certain parameters
		 *
		 * @param[in] tc New tab class
		 * @param[in] name Tab creation menu caption
		 * @param[in] mode Twitter API connection mode
		 * @param[in] params Twitter API parameters which should be added to every request
		 */
		void AddTab (const TabClassInfo& tc, const QString& name = QString (),
					 const FeedMode mode = FeedMode::HomeTimeline,
					 const KQOAuthParameters& params = KQOAuthParameters ());
	signals:
		void addNewTab (const QString&, QWidget*) override;
		void removeTab (QWidget*) override;
		void changeTabName (QWidget*, const QString&) override;
		void changeTabIcon (QWidget*, const QIcon&) override;
		void statusBarChanged (QWidget*, const QString&) override;
		void raiseTab (QWidget*) override;
	};
};
};
};
