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

#include "regexp.h"
#include <QtDebug>

#ifdef USE_PCRE
#include <pcre.h>
#else
#include <QRegExp>
#endif

namespace LeechCraft
{
namespace Util
{
#ifdef USE_PCRE

#ifndef PCRE_STUDY_JIT_COMPILE
#define PCRE_STUDY_JIT_COMPILE 0
#endif

	class PCREWrapper
	{
		pcre *RE_;
		pcre_extra *Extra_;

		QString Pattern_;
		Qt::CaseSensitivity CS_;
	public:
		PCREWrapper ()
		: RE_ (0)
		, Extra_ (0)
		{
		}

		PCREWrapper (const QString& str, Qt::CaseSensitivity cs)
		: RE_ (Compile (str, cs))
		, Extra_ (0)
		, Pattern_ (str)
		, CS_ (cs)
		{
			if (RE_)
			{
				pcre_refcount (RE_, 1);
				const char *error = 0;
				const int opts = PCRE_STUDY_JIT_COMPILE;
				Extra_ = pcre_study (RE_, opts, &error);
			}
		}

		PCREWrapper (const PCREWrapper& other)
		: RE_ (0)
		, Extra_ (0)
		{
			*this = other;
		}

		PCREWrapper& operator= (const PCREWrapper& other)
		{
			if (RE_ && !pcre_refcount (RE_, -1))
			{
				FreeStudy ();
				pcre_free (RE_);
			}

			RE_ = other.RE_;
			Extra_ = other.Extra_;
			if (RE_)
				pcre_refcount (RE_, 1);

			return *this;
		}

		~PCREWrapper ()
		{
			if (!RE_)
				return;

			if (!pcre_refcount (RE_, -1))
			{
				FreeStudy ();
				pcre_free (RE_);
			}
		}

		const QString& GetPattern () const
		{
			return Pattern_;
		}

		Qt::CaseSensitivity GetCS () const
		{
			return CS_;
		}

		int Exec (const QByteArray& utf8) const
		{
			return RE_ ? pcre_exec (RE_, Extra_, utf8.constData (), utf8.size (), 0, 0, NULL, 0) : -1;
		}
	private:
		pcre* Compile (const QString& str, Qt::CaseSensitivity cs)
		{
			const char *error = 0;
			int errOffset = 0;
			int options = PCRE_UTF8;
			if (cs == Qt::CaseInsensitive)
				options |= PCRE_CASELESS;
			auto re = pcre_compile (str.toUtf8 ().constData (), options, &error, &errOffset, NULL);
			if (!re)
				qWarning () << Q_FUNC_INFO
						<< "failed compiling"
						<< str
						<< error;
			return re;
		}

		void FreeStudy ()
		{
			if (Extra_)
#ifdef PCRE_CONFIG_JIT
				pcre_free_study (Extra_);
#else
				pcre_free (Extra_);
#endif
		}
	};
#endif

	struct RegExpImpl
	{
#if USE_PCRE
		PCREWrapper PRx_;
#else
		QRegExp Rx_;
#endif
	};

	bool RegExp::IsFast ()
	{
#ifdef USE_PCRE
		return true;
#else
		return false;
#endif
	}

	RegExp::RegExp (const QString& str, Qt::CaseSensitivity cs)
#ifdef USE_PCRE
	: Impl_ { new RegExpImpl { { str, cs } } }
#else
	: Impl_ { new RegExpImpl { QRegExp { str, cs, QRegExp::RegExp } } }
#endif
	{
	}

	bool RegExp::Matches (const QString& str) const
	{
		if (!Impl_)
			return {};

#ifdef USE_PCRE
		return Impl_->PRx_.Exec (str.toUtf8 ()) >= 0;
#else
		return Impl_->Rx_.exactMatch (str);
#endif
	}

	QString RegExp::GetPattern () const
	{
		if (!Impl_)
			return {};

#ifdef USE_PCRE
		return Impl_->PRx_.GetPattern ();
#else
		return Impl_->Rx_.pattern ();
#endif
	}

	Qt::CaseSensitivity RegExp::GetCaseSensitivity () const
	{
		if (!Impl_)
			return {};

#ifdef USE_PCRE
		return Impl_->PRx_.GetCS ();
#else
		return Impl_->Rx_.caseSensitivity ();
#endif
	}
}
}
