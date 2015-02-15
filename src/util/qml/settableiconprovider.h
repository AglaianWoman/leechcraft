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

#pragma once

#include "widthiconprovider.h"
#include <QHash>
#include <QIcon>

namespace LeechCraft
{
namespace Util
{
	/** @brief QML image provider with settable icons for each path.
	 *
	 * This class implements a QML image provider that provides preset
	 * icons for given paths. The icons are set via SetIcon() and
	 * ClearIcon().
	 *
	 * @sa WidthIconProvider
	 */
	class UTIL_QML_API SettableIconProvider : public WidthIconProvider
	{
		QHash<QStringList, QIcon> Icons_;
	public:
		/** @brief Sets the \em icon for the given \em path.
		 *
		 * If there is already an icon for this \em path, it is replaced
		 * by the new \em icon.
		 *
		 * The icon set with this function is available for this \em path
		 * via the GetIcon() method.
		 *
		 * @param[in] path The path associated with the \em icon.
		 * @param[in] icon The icon to associate with the \em path.
		 *
		 * @sa ClearIcon()
		 * @sa GetIcon()
		 */
		void SetIcon (const QStringList& path, const QIcon& icon);

		/** @brief Clears the icon associated with the given \em path.
		 *
		 * @param[in] path The path to clear.
		 *
		 * @sa SetIcon()
		 * @sa GetIcon()
		 */
		void ClearIcon (const QStringList& path);

		/** @brief Returns the icon for the \em path previously set with
		 * SetIcon().
		 *
		 * @param[in] path The path for which to return an the icon.
		 * @return The icon for the \em path, or a null icon of no icon
		 * is associated with it.
		 *
		 * @sa SetIcon()
		 * @sa ClearIcon()
		 */
		QIcon GetIcon (const QStringList& path) override;
	};
}
}
