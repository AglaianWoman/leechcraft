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

#include <functional>
#include <QObject>

class QWheelEvent;
class QAction;

namespace LeechCraft
{
namespace Poshuku
{
	class Zoomer : public QObject
	{
		Q_OBJECT

		const QList<qreal> Zooms_;
	public:
		using ZoomGetter_f = std::function<qreal (void)>;
		using ZoomSetter_f = std::function<void (qreal)>;
	private:
		const ZoomGetter_f Getter_;
		const ZoomSetter_f Setter_;
	public:
		Zoomer (const ZoomGetter_f&,
				const ZoomSetter_f&,
				QObject*,
				const QList<qreal>& = { 0.3, 0.5, 0.67, 0.8, 0.9, 1, 1.1, 1.2, 1.33, 1.5, 1.7, 2, 2.4, 3 });

		void SetActionsTriple (QAction *in, QAction *out, QAction *reset);
		void InstallScrollFilter (QObject*, const std::function<bool (QWheelEvent*)>&);
	private:
		int LevelForZoom (qreal) const;
		void SetZoom (qreal);
	public slots:
		void zoomIn ();
		void zoomOut ();
		void zoomReset ();
	signals:
		void zoomChanged ();
	};
}
}
