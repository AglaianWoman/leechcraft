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

#include "progresslistener.h"

namespace LeechCraft
{
namespace Snails
{
	ProgressListener::ProgressListener (QObject *parent)
	: QObject { parent }
	{
	}

	void ProgressListener::Increment ()
	{
		progress (LastProgress_ + 1, LastTotal_);
	}

	bool ProgressListener::cancel () const
	{
		return false;
	}

	void ProgressListener::start (const size_t total)
	{
		emit started (total);
		progress (0, total);
	}

	void ProgressListener::progress (const size_t done, const size_t total)
	{
		LastProgress_ = done;
		LastTotal_ = total;

		emit gotProgress (done, total);
	}

	void ProgressListener::stop (const size_t total)
	{
		progress (total, total);
	}

	bool operator< (const ProgressListener_wptr& w1, const ProgressListener_wptr& w2)
	{
		return w1.owner_before (w2);
	}
}
}
