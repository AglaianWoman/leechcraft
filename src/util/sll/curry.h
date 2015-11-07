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

#include <tuple>
#include <type_traits>
#include <utility>
#include "oldcppkludges.h"

namespace LeechCraft
{
namespace Util
{
	template<typename F, typename... PrevArgs>
	class CurryImpl
	{
		const F m_f;

		const std::tuple<PrevArgs...> m_prevArgs;
	public:
		CurryImpl (F f, const std::tuple<PrevArgs...>& prev)
		: m_f { f }
		, m_prevArgs { prev }
		{
		}

		template<typename T>
		auto operator() (const T& arg) const
		{
			return invoke (arg, 0);
		}
	private:
		template<typename T>
		std::result_of_t<F (PrevArgs..., T)> invoke (const T& arg, int) const
		{
			return invokeIndexed (arg, std::index_sequence_for<PrevArgs...> {});
		}

		template<typename IF>
		struct Invoke
		{
			template<typename... IArgs>
			auto operator() (IF fr, IArgs... args)
			{
				return fr (args...);
			}
		};

		template<typename R, typename C, typename... Args>
		struct Invoke<R (C::*) (Args...)>
		{
			auto operator() (R (C::*ptr) (Args...), C c, Args... rest)
			{
				return (c.*ptr) (rest...);
			}

			auto operator() (R (C::*ptr) (Args...), C *c, Args... rest)
			{
				return (c->*ptr) (rest...);
			}
		};

		template<typename T, std::size_t... Is>
		auto invokeIndexed (const T& arg, std::index_sequence<Is...>) const
		{
			return Invoke<F> {} (m_f, std::get<Is> (m_prevArgs)..., arg);
		}

		template<typename T>
		auto invoke (const T& arg, ...) const
		{
			return CurryImpl<F, PrevArgs..., T> { m_f, std::tuple_cat (m_prevArgs, std::tuple<T> { arg }) };
		}
	};

	template<typename F>
	auto Curry (F f)
	{
		return CurryImpl<F> { f, {} };
	}
}
}