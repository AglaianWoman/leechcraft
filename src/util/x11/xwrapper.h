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

#include <QX11Info>
#include <QList>
#include <QString>
#include <QHash>
#include <QObject>
#include <QAbstractNativeEventFilter>
#include <X11/Xdefs.h>
#include "x11config.h"
#include "winflags.h"

class QIcon;
class QWidget;
class QRect;

typedef unsigned long Window;
#define _XTYPEDEF_XID

typedef union _XEvent XEvent;

namespace LeechCraft
{
namespace Util
{
	class UTIL_X11_API XWrapper : public QObject
								, public QAbstractNativeEventFilter
	{
		Q_OBJECT

		Display *Display_;
		Window AppWin_;

		QHash<QString, Atom> Atoms_;

		XWrapper ();
	public:
		enum class Layer
		{
			Top,
			Bottom,
			Normal
		};

		static XWrapper& Instance ();

		Display* GetDisplay () const;
		Window GetRootWindow () const;

		bool nativeEventFilter (const QByteArray& eventType, void *message, long *result) override;

		void Sync ();

		QList<Window> GetWindows ();
		QString GetWindowTitle (Window);
		QIcon GetWindowIcon (Window);
		WinStateFlags GetWindowState (Window);
		AllowedActionFlags GetWindowActions (Window);

		Window GetActiveApp ();

		bool IsLCWindow (Window);
		bool ShouldShow (Window);

		void Subscribe (Window);

		void SetStrut (QWidget*, Qt::ToolBarArea);
		void ClearStrut (QWidget*);
		void SetStrut (Window wid,
				int left, int right, int top, int bottom,
				int leftStartY, int leftEndY,
				int rightStartY, int rightEndY,
				int topStartX, int topEndX,
				int bottomStartX, int bottomEndX);

		void RaiseWindow (Window);
		void MinimizeWindow (Window);
		void MaximizeWindow (Window);
		void UnmaximizeWindow (Window);
		void ShadeWindow (Window);
		void UnshadeWindow (Window);
		void MoveWindowTo (Window, Layer);
		void CloseWindow (Window);

		void ResizeWindow (Window, int, int);

		int GetDesktopCount ();
		int GetCurrentDesktop ();
		void SetCurrentDesktop (int);
		QStringList GetDesktopNames ();
		QString GetDesktopName (int, const QString& = QString ());
		int GetWindowDesktop (Window);
		void MoveWindowToDesktop (Window, int);

		QRect GetAvailableGeometry (int screen = -1);
		QRect GetAvailableGeometry (QWidget*);

		Atom GetAtom (const QString&);
	private:
		template<typename T>
		void HandlePropNotify (T);

		Window GetActiveWindow ();

		bool GetWinProp (Window, Atom, ulong*, uchar**, Atom = static_cast<Atom> (0)) const;
		bool GetRootWinProp (Atom, ulong*, uchar**, Atom = static_cast<Atom> (0)) const;
		QList<Atom> GetWindowType (Window);

		bool SendMessage (Window, Atom, ulong, ulong = 0, ulong = 0, ulong = 0, ulong = 0);
	private slots:
		void initialize ();
	signals:
		void windowListChanged ();
		void activeWindowChanged ();
		void desktopChanged ();

		void windowNameChanged (ulong);
		void windowIconChanged (ulong);
		void windowDesktopChanged (ulong);
		void windowStateChanged (ulong);
		void windowActionsChanged (ulong);
	};
}
}
