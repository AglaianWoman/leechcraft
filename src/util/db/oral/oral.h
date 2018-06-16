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

#include <stdexcept>
#include <type_traits>
#include <memory>
#include <boost/fusion/include/for_each.hpp>
#include <boost/fusion/include/fold.hpp>
#include <boost/fusion/include/filter_if.hpp>
#include <boost/fusion/container/vector.hpp>
#include <boost/fusion/include/vector.hpp>
#include <boost/fusion/include/transform.hpp>
#include <boost/fusion/include/zip.hpp>
#include <boost/fusion/container/generation/make_vector.hpp>
#include <boost/variant/variant.hpp>
#include <boost/optional.hpp>
#include <QStringList>
#include <QDateTime>
#include <QPair>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QVariant>
#include <QDateTime>
#include <QtDebug>
#include <util/sll/qtutil.h>
#include <util/sll/prelude.h>
#include <util/sll/typelist.h>
#include <util/sll/typelevel.h>
#include <util/sll/typegetter.h>
#include <util/sll/detector.h>
#include <util/sll/unreachable.h>
#include <util/sll/void.h>
#include <util/db/dblock.h>
#include <util/db/util.h>
#include "oraltypes.h"
#include "oraldetailfwd.h"
#include "impldefs.h"
#include "sqliteimpl.h"

namespace LeechCraft
{
namespace Util
{
namespace oral
{
	using QSqlQuery_ptr = std::shared_ptr<QSqlQuery>;

	class QueryException : public std::runtime_error
	{
		const QSqlQuery_ptr Query_;
	public:
		QueryException (const std::string& str, const QSqlQuery_ptr& q)
		: std::runtime_error (str)
		, Query_ (q)
		{
		}

		virtual ~QueryException () noexcept
		{
		}

		const QSqlQuery_ptr& GetQueryPtr () const
		{
			return Query_;
		}

		const QSqlQuery& GetQuery () const
		{
			return *Query_;
		}
	};

	namespace detail
	{
		template<typename U>
		using MorpherDetector = decltype (std::declval<U> ().FieldNameMorpher (QString {}));

		template<typename T>
		QString MorphFieldName (QString str)
		{
			if constexpr (IsDetected_v<MorpherDetector, T>)
				return T::FieldNameMorpher (str);
			else
			{
				if (str.endsWith ('_'))
					str.chop (1);
				return str;
			}
		}

		template<typename Seq, int Idx>
		struct GetFieldName
		{
			static QString value ()
			{
				return MorphFieldName<Seq> (boost::fusion::extension::struct_member_name<Seq, Idx>::call ());
			}
		};

		template<typename S>
		constexpr auto SeqSize = boost::fusion::result_of::size<S>::type::value;

		template<typename S>
		constexpr auto SeqIndices = std::make_index_sequence<SeqSize<S>> {};

		template<typename S>
		struct GetFieldsNames
		{
			QStringList operator() () const
			{
				return Run (SeqIndices<S>);
			}
		private:
			template<size_t... Vals>
			QStringList Run (std::index_sequence<Vals...>) const
			{
				return { GetFieldName<S, Vals>::value ()... };
			}
		};

		template<typename Seq, int Idx>
		struct GetBoundName
		{
			static QString value () { return ':' + Seq::ClassName () + "_" + GetFieldName<Seq, Idx>::value (); }
		};

		template<typename S>
		struct AddressOf
		{
			inline static S Obj_ {};

			template<auto P>
			static constexpr auto Ptr ()
			{
				return &(Obj_.*P);
			}

			template<int Idx>
			static constexpr auto Index ()
			{
				return &boost::fusion::at_c<Idx> (Obj_);
			}
		};

		template<auto Ptr, size_t Idx = 0>
		constexpr size_t FieldIndex ()
		{
			using S = MemberPtrStruct_t<Ptr>;

			if constexpr (Idx == SeqSize<S>)
				throw std::runtime_error { "wut, no such field?" };
			else
			{
				constexpr auto direct = AddressOf<S>::template Ptr<Ptr> ();
				constexpr auto indexed = AddressOf<S>::template Index<Idx> ();
				if constexpr (std::is_same_v<decltype (direct), decltype (indexed)>)
				{
					if (indexed == direct)
						return Idx;
				}

				return FieldIndex<Ptr, Idx + 1> ();
			}
		}
	}

	template<typename ImplFactory, typename T, typename = void>
	struct Type2Name
	{
		QString operator() () const
		{
			if constexpr (HasType<T> (Typelist<int, qulonglong, bool> {}) || std::is_enum_v<T>)
				return "INTEGER";
			else if constexpr (std::is_same_v<T, double>)
				return "REAL";
			else if constexpr (std::is_same_v<T, QString> || std::is_same_v<T, QDateTime>)
				return "TEXT";
			else if constexpr (std::is_same_v<T, QByteArray>)
				return ImplFactory::TypeLits::Binary;
			else
				static_assert (std::is_same_v<T, struct Dummy>, "Unsupported type");
		}
	};

	template<typename ImplFactory, typename T>
	struct Type2Name<ImplFactory, Unique<T>>
	{
		QString operator() () const { return Type2Name<ImplFactory, T> () () + " UNIQUE"; }
	};

	template<typename ImplFactory, typename T>
	struct Type2Name<ImplFactory, NotNull<T>>
	{
		QString operator() () const { return Type2Name<ImplFactory, T> () () + " NOT NULL"; }
	};

	template<typename ImplFactory, typename T, typename... Tags>
	struct Type2Name<ImplFactory, PKey<T, Tags...>>
	{
		QString operator() () const { return Type2Name<ImplFactory, T> () () + " PRIMARY KEY"; }
	};

	template<typename ImplFactory, typename... Tags>
	struct Type2Name<ImplFactory, PKey<int, Tags...>>
	{
		QString operator() () const { return ImplFactory::TypeLits::IntAutoincrement; }
	};

	template<typename ImplFactory, auto Ptr>
	struct Type2Name<ImplFactory, References<Ptr>>
	{
		QString operator() () const
		{
			using Seq = MemberPtrStruct_t<Ptr>;
			constexpr auto idx = detail::FieldIndex<Ptr> ();
			return Type2Name<ImplFactory, ReferencesValue_t<Ptr>> () () +
					" REFERENCES " + Seq::ClassName () + " (" + detail::GetFieldName<Seq, idx>::value () + ") ON DELETE CASCADE";
		}
	};

	template<typename T, typename = void>
	struct ToVariant
	{
		QVariant operator() (const T& t) const
		{
			if constexpr (std::is_same_v<T, QDateTime>)
				return t.toString (Qt::ISODate);
			else if constexpr (std::is_enum_v<T>)
				return static_cast<qint64> (t);
			else if constexpr (IsIndirect<T> {})
				return ToVariant<typename T::value_type> {} (t);
			else
				return t;
		}
	};

	template<typename T, typename = void>
	struct FromVariant
	{
		T operator() (const QVariant& var) const
		{
			if constexpr (std::is_same_v<T, QDateTime>)
				return QDateTime::fromString (var.toString (), Qt::ISODate);
			else if constexpr (std::is_enum_v<T>)
				return static_cast<T> (var.value<qint64> ());
			else if constexpr (IsIndirect<T> {})
				return FromVariant<typename T::value_type> {} (var);
			else
				return var.value<T> ();
		}
	};

	namespace detail
	{
		template<typename T>
		struct IsPKey : std::false_type {};

		template<typename U, typename... Tags>
		struct IsPKey<PKey<U, Tags...>> : std::true_type {};

		template<typename T>
		QVariant ToVariantF (const T& t)
		{
			return ToVariant<T> {} (t);
		}

		template<typename T>
		auto MakeInserter (const CachedFieldsData& data, const QSqlQuery_ptr& insertQuery, bool bindPrimaryKey)
		{
			return [data, insertQuery, bindPrimaryKey] (const T& t)
			{
				boost::fusion::fold (t, data.BoundFields_.begin (),
						[&] (auto pos, const auto& elem)
						{
							using Elem = std::decay_t<decltype (elem)>;
							if (bindPrimaryKey || !IsPKey<Elem>::value)
								insertQuery->bindValue (*pos++, ToVariantF (elem));
							return pos;
						});

				if (!insertQuery->exec ())
				{
					DBLock::DumpError (*insertQuery);
					throw QueryException ("insert query execution failed", insertQuery);
				}
			};
		}

		template<typename Seq, int Idx>
		using ValueAtC_t = typename boost::fusion::result_of::value_at_c<Seq, Idx>::type;

		template<typename Seq, typename Idx>
		using ValueAt_t = typename boost::fusion::result_of::value_at<Seq, Idx>::type;

		template<typename Seq, typename MemberIdx = boost::mpl::int_<0>>
		struct FindPKey
		{
			static_assert ((boost::fusion::result_of::size<Seq>::value) != (MemberIdx::value),
					"Primary key not found");

			template<typename T>
			struct Lazy
			{
				using type = T;
			};

			using result_type = typename std::conditional_t<
						IsPKey<ValueAt_t<Seq, MemberIdx>>::value,
						Lazy<MemberIdx>,
						Lazy<FindPKey<Seq, typename boost::mpl::next<MemberIdx>::type>>
					>::type;
		};

		template<typename Seq>
		using FindPKeyDetector = boost::mpl::int_<FindPKey<Seq>::result_type::value>;

		template<typename Seq>
		constexpr auto HasPKey = IsDetected_v<FindPKeyDetector, Seq>;

		template<typename Seq>
		constexpr auto HasAutogenPKey ()
		{
			if constexpr (HasPKey<Seq>)
				return !HasType<NoAutogen> (AsTypelist_t<ValueAtC_t<Seq, FindPKey<Seq>::result_type::value>> {});
			else
				return false;
		}

		template<typename T>
		CachedFieldsData BuildCachedFieldsData (const QString& table)
		{
			const auto& fields = detail::GetFieldsNames<T> {} ();
			const auto& qualified = Util::Map (fields, [&table] (const QString& field) { return table + "." + field; });
			const auto& boundFields = Util::Map (fields, [] (const QString& str) { return ':' + str; });

			return { table, fields, qualified, boundFields };
		}

		template<typename T>
		CachedFieldsData BuildCachedFieldsData ()
		{
			static CachedFieldsData result = BuildCachedFieldsData<T> (T::ClassName ());
			return result;
		}

		template<typename Seq>
		class AdaptInsert
		{
			const QSqlDatabase DB_;
			const CachedFieldsData Data_;

			constexpr static bool HasAutogen_ = HasAutogenPKey<Seq> ();

			IInsertQueryBuilder_ptr QueryBuilder_;
		public:
			template<typename ImplFactory>
			AdaptInsert (const QSqlDatabase& db, CachedFieldsData data, ImplFactory&& factory)
			: Data_ { RemovePKey (data) }
			, QueryBuilder_ { factory.MakeInsertQueryBuilder (db, Data_) }
			{
			}

			auto operator() (Seq& t, InsertAction action = InsertAction::Default) const
			{
				return Run<true> (t, action);
			}

			auto operator() (const Seq& t, InsertAction action = InsertAction::Default) const
			{
				return Run<false> (t, action);
			}
		private:
			template<bool UpdatePKey, typename Val>
			auto Run (Val&& t, InsertAction action) const
			{
				const auto query = QueryBuilder_->GetQuery (action);

				MakeInserter<Seq> (Data_, query, !HasAutogen_) (t);

				if constexpr (HasAutogen_)
				{
					constexpr auto index = FindPKey<Seq>::result_type::value;

					const auto& lastId = FromVariant<ValueAtC_t<Seq, index>> {} (query->lastInsertId ());
					if constexpr (UpdatePKey)
						boost::fusion::at_c<index> (t) = lastId;
					else
						return lastId;
				}
			}

			static CachedFieldsData RemovePKey (CachedFieldsData data)
			{
				if constexpr (HasAutogen_)
				{
					constexpr auto index = FindPKey<Seq>::result_type::value;
					data.Fields_.removeAt (index);
					data.BoundFields_.removeAt (index);
				}
				return data;
			}
		};

		template<typename Seq, bool HasPKey = HasPKey<Seq>>
		struct AdaptDelete
		{
			std::function<void (Seq)> Deleter_;
		public:
			template<bool B = HasPKey>
			AdaptDelete (const QSqlDatabase& db, const CachedFieldsData& data, std::enable_if_t<B>* = nullptr)
			{
				const auto index = FindPKey<Seq>::result_type::value;

				const auto& boundName = data.BoundFields_.at (index);
				const auto& del = "DELETE FROM " + data.Table_ +
						" WHERE " + data.Fields_.at (index) + " = " + boundName + ";";

				const auto deleteQuery = std::make_shared<QSqlQuery> (db);
				deleteQuery->prepare (del);

				Deleter_ = [deleteQuery, boundName] (const Seq& t)
				{
					constexpr auto index = FindPKey<Seq>::result_type::value;
					deleteQuery->bindValue (boundName, ToVariantF (boost::fusion::at_c<index> (t)));
					if (!deleteQuery->exec ())
						throw QueryException ("delete query execution failed", deleteQuery);
				};
			}

			template<bool B = HasPKey>
			AdaptDelete (const QSqlDatabase&, const CachedFieldsData&, std::enable_if_t<!B>* = nullptr)
			{
			}

			template<bool B = HasPKey>
			std::enable_if_t<B> operator() (const Seq& seq)
			{
				Deleter_ (seq);
			}
		};

		template<typename T, typename... Args>
		using AggregateDetector_t = decltype (new T { std::declval<Args> ()... });

		template<typename T, size_t... Indices>
		T InitializeFromQuery (const QSqlQuery& q, std::index_sequence<Indices...>)
		{
			if constexpr (IsDetected_v<AggregateDetector_t, T, ValueAtC_t<T, Indices>...>)
				return T { FromVariant<ValueAtC_t<T, Indices>> {} (q.value (Indices))... };
			else
			{
				T t;
				const auto dummy = std::initializer_list<int>
				{
					(static_cast<void> (boost::fusion::at_c<Indices> (t) = FromVariant<ValueAtC_t<T, Indices>> {} (q.value (Indices))), 0)...
				};
				Q_UNUSED (dummy);
				return t;
			}
		}

		template<int HeadT, int... TailT>
		struct FieldsUnpacker
		{
			static const int Head = HeadT;
			using Tail_t = FieldsUnpacker<TailT...>;
		};

		template<int HeadT>
		struct FieldsUnpacker<HeadT>
		{
			static const int Head = HeadT;
			using Tail_t = std::false_type;
		};

		template<typename FieldsUnpacker, typename HeadArg, typename... TailArgs>
		struct ValueBinder
		{
			QSqlQuery_ptr Query_;
			QList<QString> BoundFields_;

			void operator() (const HeadArg& arg, const TailArgs&... tail) const
			{
				Query_->bindValue (BoundFields_.at (FieldsUnpacker::Head), arg);

				ValueBinder<typename FieldsUnpacker::Tail_t, TailArgs...> { Query_, BoundFields_ } (tail...);
			}
		};

		template<typename FieldsUnpacker, typename HeadArg>
		struct ValueBinder<FieldsUnpacker, HeadArg>
		{
			QSqlQuery_ptr Query_;
			QList<QString> BoundFields_;

			void operator() (const HeadArg& arg) const
			{
				Query_->bindValue (BoundFields_.at (FieldsUnpacker::Head), arg);
			}
		};

		enum class ExprType
		{
			ConstTrue,

			LeafStaticPlaceholder,
			LeafData,

			Greater,
			Less,
			Equal,
			Geq,
			Leq,
			Neq,

			And,
			Or
		};

		inline QString TypeToSql (ExprType type)
		{
			switch (type)
			{
			case ExprType::Greater:
				return ">";
			case ExprType::Less:
				return "<";
			case ExprType::Equal:
				return "=";
			case ExprType::Geq:
				return ">=";
			case ExprType::Leq:
				return "<=";
			case ExprType::Neq:
				return "!=";
			case ExprType::And:
				return "AND";
			case ExprType::Or:
				return "OR";

			case ExprType::LeafStaticPlaceholder:
			case ExprType::LeafData:
			case ExprType::ConstTrue:
				return "invalid type";
			}

			Util::Unreachable ();
		}

		constexpr bool IsRelational (ExprType type)
		{
			return type == ExprType::Greater ||
					type == ExprType::Less ||
					type == ExprType::Equal ||
					type == ExprType::Geq ||
					type == ExprType::Leq ||
					type == ExprType::Neq;
		}

		template<typename T>
		struct ToSqlState
		{
			int LastID_;
			QVariantMap BoundMembers_;
		};

		template<typename T>
		struct WrapDirect
		{
			using value_type = T;
		};

		template<typename T>
		using UnwrapIndirect_t = typename std::conditional_t<IsIndirect<T> {},
				T,
				WrapDirect<T>>::value_type;

		template<typename Seq, typename L, typename R>
		using ComparableDetector = decltype (std::declval<UnwrapIndirect_t<typename L::template ValueType_t<Seq>>> () ==
				std::declval<UnwrapIndirect_t<typename R::template ValueType_t<Seq>>> ());

		template<typename Seq, typename L, typename R>
		constexpr auto AreComparableTypes = IsDetected_v<ComparableDetector, Seq, L, R> || IsDetected_v<ComparableDetector, Seq, R, L>;

		template<typename Seq, typename L, typename R, typename = void>
		struct RelationalTypesCheckerBase : std::false_type {};

		template<typename Seq, typename L, typename R>
		struct RelationalTypesCheckerBase<Seq, L, R, std::enable_if_t<AreComparableTypes<Seq, L, R>>> : std::true_type {};

		template<ExprType Type, typename Seq, typename L, typename R, typename = void>
		struct RelationalTypesChecker : std::true_type {};

		template<ExprType Type, typename Seq, typename L, typename R>
		struct RelationalTypesChecker<Type, Seq, L, R, std::enable_if_t<IsRelational (Type)>> : RelationalTypesCheckerBase<Seq, L, R> {};

		template<ExprType Type, typename L = void, typename R = void>
		class ExprTree;

		template<typename T>
		struct IsExprTree : std::false_type {};

		template<ExprType Type, typename L, typename R>
		struct IsExprTree<ExprTree<Type, L, R>> : std::true_type {};

		template<typename L, typename R>
		class AssignList
		{
			L Left_;
			R Right_;
		public:
			AssignList (const L& l, const R& r)
			: Left_ { l }
			, Right_ { r }
			{
			}

			template<typename T>
			QString ToSql (ToSqlState<T>& state) const
			{
				if constexpr (IsExprTree<L> {})
					return Left_.GetFieldName () + " = " + Right_.ToSql (state);
				else
					return Left_.ToSql (state) + ", " + Right_.ToSql (state);
			}

			template<typename OL, typename OR>
			auto operator, (const AssignList<OL, OR>& tail)
			{
				return AssignList<AssignList<L, R>, AssignList<OL, OR>> { *this, tail };
			}
		};

		template<ExprType Type, typename L, typename R>
		class ExprTree
		{
			L Left_;
			R Right_;
		public:
			ExprTree (const L& l, const R& r)
			: Left_ (l)
			, Right_ (r)
			{
			}

			template<typename T>
			QString ToSql (ToSqlState<T>& state) const
			{
				static_assert (RelationalTypesChecker<Type, T, L, R>::value,
						"Incompatible types passed to a relational operator.");

				return Left_.ToSql (state) + " " + TypeToSql (Type) + " " + Right_.ToSql (state);
			}

			template<typename T>
			QSet<QString> AdditionalTables () const
			{
				return Left_.template AdditionalTables<T> () + Right_.template AdditionalTables<T> ();
			}
		};

		template<int Idx>
		class ExprTree<ExprType::LeafStaticPlaceholder, boost::mpl::int_<Idx>, void>
		{
		public:
			template<typename T>
			using ValueType_t = ValueAtC_t<T, Idx>;

			template<typename T>
			QString ToSql (ToSqlState<T>&) const
			{
				static_assert (Idx < boost::fusion::result_of::size<T>::type::value, "Index out of bounds.");
				return detail::GetFieldName<T, Idx>::value ();
			}

			template<typename>
			QSet<QString> AdditionalTables () const
			{
				return {};
			}
		};

		template<auto... Ptr>
		struct MemberPtrs {};

		template<auto Ptr>
		class ExprTree<ExprType::LeafStaticPlaceholder, MemberPtrs<Ptr>, void>
		{
		public:
			template<typename>
			using ValueType_t = MemberPtrType_t<Ptr>;

			template<typename T>
			QString ToSql (ToSqlState<T>&) const
			{
				return MemberPtrStruct_t<Ptr>::ClassName () + "." + GetFieldName ();
			}

			QString GetFieldName () const
			{
				using Seq = MemberPtrStruct_t<Ptr>;
				constexpr auto idx = FieldIndex<Ptr> ();
				return detail::GetFieldName<Seq, idx>::value ();
			}

			template<typename T>
			QSet<QString> AdditionalTables () const
			{
				using Seq = MemberPtrStruct_t<Ptr>;
				if constexpr (std::is_same_v<Seq, T>)
					return {};
				else
					return { Seq::ClassName () };
			}

			template<typename R>
			auto operator= (const R&) const;
		};

		template<typename T>
		class ExprTree<ExprType::LeafData, T, void>
		{
			T Data_;
		public:
			template<typename>
			using ValueType_t = T;

			ExprTree (const T& t)
			: Data_ (t)
			{
			}

			template<typename ObjT>
			QString ToSql (ToSqlState<ObjT>& state) const
			{
				const auto& name = ":bound_" + QString::number (++state.LastID_);
				state.BoundMembers_ [name] = ToVariantF (Data_);
				return name;
			}

			template<typename>
			QSet<QString> AdditionalTables () const
			{
				return {};
			}
		};

		template<>
		class ExprTree<ExprType::ConstTrue, void, void> {};

		constexpr auto ConstTrueTree_v = ExprTree<ExprType::ConstTrue> {};

		template<typename T>
		constexpr auto AsLeafData (const T& node)
		{
			if constexpr (IsExprTree<T> {})
				return node;
			else
				return ExprTree<ExprType::LeafData, T> { node };
		}

		template<auto Ptr>
		template<typename R>
		auto ExprTree<ExprType::LeafStaticPlaceholder, MemberPtrs<Ptr>, void>::operator= (const R& r) const
		{
			return AssignList { *this, AsLeafData (r) };
		}

		template<ExprType Type, typename L, typename R>
		ExprTree<Type, L, R> MakeExprTree (const L& left, const R& right)
		{
			return { left, right };
		}

		template<typename L, typename R>
		using EnableRelOp_t = std::enable_if_t<AnyOf<IsExprTree, L, R>>;

		template<typename L, typename R>
		constexpr auto AllTrees_v = AllOf<IsExprTree, L, R>;

		template<typename L, typename R, typename = EnableRelOp_t<L, R>>
		auto operator< (const L& left, const R& right)
		{
			if constexpr (AllTrees_v<L, R>)
				return MakeExprTree<ExprType::Less> (left, right);
			else
				return AsLeafData (left) < AsLeafData (right);
		}

		template<typename L, typename R, typename = EnableRelOp_t<L, R>>
		auto operator> (const L& left, const R& right)
		{
			if constexpr (AllTrees_v<L, R>)
				return MakeExprTree<ExprType::Greater> (left, right);
			else
				return AsLeafData (left) > AsLeafData (right);
		}

		template<typename L, typename R, typename = EnableRelOp_t<L, R>>
		auto operator== (const L& left, const R& right)
		{
			if constexpr (AllTrees_v<L, R>)
				return MakeExprTree<ExprType::Equal> (left, right);
			else
				return AsLeafData (left) == AsLeafData (right);
		}

		template<typename L, typename R, typename = EnableRelOp_t<L, R>>
		auto operator&& (const L& left, const R& right)
		{
			if constexpr (AllTrees_v<L, R>)
				return MakeExprTree<ExprType::And> (left, right);
			else
				return AsLeafData (left) && AsLeafData (right);
		}

		template<typename>
		auto HandleExprTree (const ExprTree<ExprType::ConstTrue>&, int lastId = 0)
		{
			return std::tuple { QString {}, Void {}, lastId };
		}

		template<typename Seq, typename Tree,
				typename = decltype (std::declval<Tree> ().ToSql (std::declval<ToSqlState<Seq>&> ()))>
		auto HandleExprTree (const Tree& tree, int lastId = 0)
		{
			ToSqlState<Seq> state { lastId, {} };

			const auto& sql = tree.ToSql (state);

			return std::tuple
			{
				sql,
				[state] (QSqlQuery& query)
				{
					for (const auto& pair : Stlize (state.BoundMembers_))
						query.bindValue (pair.first, pair.second);
				},
				state.LastID_
			};
		}

		enum class AggregateFunction
		{
			Count
		};

		template<AggregateFunction>
		struct AggregateType {};
	}

	namespace sph
	{
		template<int Idx>
		using pos = detail::ExprTree<detail::ExprType::LeafStaticPlaceholder, boost::mpl::int_<Idx>>;

		static constexpr pos<0> _0 = {};
		static constexpr pos<1> _1 = {};
		static constexpr pos<2> _2 = {};
		static constexpr pos<3> _3 = {};
		static constexpr pos<4> _4 = {};

		template<auto Ptr>
		constexpr detail::ExprTree<detail::ExprType::LeafStaticPlaceholder, detail::MemberPtrs<Ptr>> f {};

		template<auto... Ptrs>
		constexpr detail::MemberPtrs<Ptrs...> fields {};

		constexpr detail::AggregateType<detail::AggregateFunction::Count> count {};
	};

	namespace detail
	{
		template<auto... Ptrs, size_t... Idxs>
		auto MakeIndexedQueryHandler (detail::MemberPtrs<Ptrs...>, std::index_sequence<Idxs...>)
		{
			return [] (const QSqlQuery& q)
			{
				if constexpr (sizeof... (Ptrs) == 1)
					return FromVariant<UnwrapIndirect_t<Head_t<Typelist<MemberPtrType_t<Ptrs>...>>>> {} (q.value (0));
				else
					return std::tuple { FromVariant<UnwrapIndirect_t<MemberPtrType_t<Ptrs>>> {} (q.value (Idxs))... };
			};
		}

		template<auto... Ptrs>
		QStringList BuildFieldNames ()
		{
			return { BuildCachedFieldsData<MemberPtrStruct_t<Ptrs>> ().QualifiedFields_.value (FieldIndex<Ptrs> ())... };
		}

		enum class SelectBehaviour { Some, One };

		struct SelectWhole {};

		template<typename T, SelectBehaviour SelectBehaviour>
		class SelectWrapper
		{
			const QSqlDatabase DB_;
			const CachedFieldsData Cached_;

			template<typename Selector, typename Tree>
			struct Builder
			{
				const SelectWrapper& W_;

				Selector Selector_;
				Tree Tree_;

				template<typename NewSel>
				auto Select (NewSel&& selector) &&
				{
					return Builder<NewSel, Tree> { W_, std::forward<NewSel> (selector), Tree_ };
				}

				template<typename NewTree>
				auto Where (NewTree&& tree) &&
				{
					return Builder<Selector, NewTree> { W_, Selector_, std::forward<NewTree> (tree) };
				}

				auto operator() ()
				{
					return W_ (Selector_, Tree_);
				}
			};
		public:
			SelectWrapper (const QSqlDatabase& db, const CachedFieldsData& data)
			: DB_ { db }
			, Cached_ (data)
			{
			}

			auto Build () const
			{
				return Builder<SelectWhole, decltype (ConstTrueTree_v)> { *this, SelectWhole {}, ConstTrueTree_v };
			}

			auto operator() () const
			{
				return Build () ();
			}

			template<typename Single>
			auto operator() (Single&& single) const
			{
				if constexpr (IsExprTree<std::decay_t<Single>> {})
					return Build ().Where (std::forward<Single> (single)) ();
				else
					return Build ().Select (std::forward<Single> (single)) ();
			}

			template<typename Selector, ExprType Type, typename L, typename R>
			auto operator() (Selector&& selector, const ExprTree<Type, L, R>& tree) const
			{
				const auto& [where, binder, _] = HandleExprTree<T> (tree);
				Q_UNUSED (_);
				const auto& [fields, initializer, postproc] = HandleSelector (std::forward<Selector> (selector));
				return postproc (Select (fields, BuildFromClause (tree), where, binder, initializer));
			}
		private:
			template<typename Binder, typename Initializer>
			auto Select (const QString& fields, const QString& from, QString where,
					Binder&& binder, Initializer&& initializer) const
			{
				if (!where.isEmpty ())
					where.prepend (" WHERE ");

				const auto& queryStr = "SELECT " + fields +
						" FROM " + from +
						where;

				QSqlQuery query { DB_ };
				query.prepare (queryStr);
				if constexpr (!std::is_same_v<Void, std::decay_t<Binder>>)
					binder (query);

				if (!query.exec ())
				{
					DBLock::DumpError (query);
					throw QueryException ("fetch query execution failed", std::make_shared<QSqlQuery> (query));
				}

				if constexpr (SelectBehaviour == SelectBehaviour::Some)
				{
					QList<std::result_of_t<Initializer (QSqlQuery)>> result;
					while (query.next ())
						result << initializer (query);
					return result;
				}
				else
				{
					using RetType_t = boost::optional<std::result_of_t<Initializer (QSqlQuery)>>;
					return query.next () ?
							RetType_t { initializer (query) } :
							RetType_t {};
				}
			}

			template<ExprType Type, typename L, typename R>
			QString BuildFromClause (const ExprTree<Type, L, R>& tree) const
			{
				if constexpr (Type != ExprType::ConstTrue)
				{
					const auto& additionalTables = Util::MapAs<QList> (tree.template AdditionalTables<T> (),
							[] (const QString& table) { return ", " + table; });
					return Cached_.Table_ + additionalTables.join (QString {});
				}
				else
					return Cached_.Table_;
			}

			auto HandleSelector (SelectWhole) const
			{
				return std::tuple
				{
					Cached_.QualifiedFields_.join (", "),
					[] (const QSqlQuery& q) { return InitializeFromQuery<T> (q, SeqIndices<T>); },
					Id
				};
			}

			template<int Idx>
			auto HandleSelector (sph::pos<Idx>) const
			{
				return std::tuple
				{
					Cached_.QualifiedFields_.value (Idx),
					[] (const QSqlQuery& q) { return FromVariant<UnwrapIndirect_t<ValueAtC_t<T, Idx>>> {} (q.value (0)); },
					Id
				};
			}

			template<auto... Ptrs>
			auto HandleSelector (MemberPtrs<Ptrs...> ptrs) const
			{
				return std::tuple
				{
					BuildFieldNames<Ptrs...> ().join (", "),
					MakeIndexedQueryHandler (ptrs, std::make_index_sequence<sizeof... (Ptrs)> {}),
					Id
				};
			}

			template<AggregateFunction Fun>
			auto HandleSelector (AggregateType<Fun>) const
			{
				if constexpr (Fun == AggregateFunction::Count)
					return std::tuple
					{
						QString { "count(1)" },
						[] (const QSqlQuery& q) { return q.value (0).toLongLong (); },
						[] (const QList<long long>& list) { return list.value (0); }
					};
			}
		};

		template<typename T>
		class DeleteByFieldsWrapper
		{
			const QSqlDatabase DB_;
			const CachedFieldsData Cached_;
		public:
			DeleteByFieldsWrapper (const QSqlDatabase& db, const CachedFieldsData& data)
			: DB_ { db }
			, Cached_ (data)
			{
			}

			template<ExprType Type, typename L, typename R>
			void operator() (const ExprTree<Type, L, R>& tree) const
			{
				const auto& [where, binder, _] = HandleExprTree<T> (tree);
				Q_UNUSED (_);

				const auto& selectAll = "DELETE FROM " + Cached_.Table_ +
						" WHERE " + where + ";";

				QSqlQuery query { DB_ };
				query.prepare (selectAll);
				binder (query);
				query.exec ();
			}
		};

		template<typename T, bool HasPKey = HasPKey<T>>
		class AdaptUpdate
		{
			const QSqlDatabase DB_;
			const CachedFieldsData Cached_;

			std::function<void (T)> Updater_;
		public:
			AdaptUpdate (const QSqlDatabase& db, const CachedFieldsData& data)
			: DB_ { db }
			, Cached_ { data }
			{
				if constexpr (HasPKey)
				{
					const auto index = FindPKey<T>::result_type::value;

					QList<QString> removedFields { data.Fields_ };
					QList<QString> removedBoundFields { data.BoundFields_ };

					const auto& fieldName = removedFields.takeAt (index);
					const auto& boundName = removedBoundFields.takeAt (index);

					const auto& statements = Util::ZipWith (removedFields, removedBoundFields,
							[] (const QString& s1, const QString& s2) { return s1 + " = " + s2; });

					const auto& update = "UPDATE " + data.Table_ +
										 " SET " + statements.join (", ") +
										 " WHERE " + fieldName + " = " + boundName + ";";

					const auto updateQuery = std::make_shared<QSqlQuery> (db);
					updateQuery->prepare (update);
					Updater_ = MakeInserter<T> (data, updateQuery, true);
				}
			}

			template<bool B = HasPKey>
			std::enable_if_t<B> operator() (const T& seq)
			{
				Updater_ (seq);
			}

			template<typename SL, typename SR, ExprType WType, typename WL, typename WR>
			void operator() (const AssignList<SL, SR>& set, const ExprTree<WType, WL, WR>& where)
			{
				const auto& [setClause, setBinder, setLast] = HandleExprTree<T> (set);
				const auto& [whereClause, whereBinder, _] = HandleExprTree<T> (where, setLast);

				const auto& update = "UPDATE " + Cached_.Table_ +
						" SET " + setClause +
						" WHERE " + whereClause;

				QSqlQuery query { DB_ };
				query.prepare (update);
				setBinder (query);
				whereBinder (query);
				query.exec ();
			}
		};

		template<typename T>
		using ConstraintsDetector = typename T::Constraints;

		template<typename T>
		using ConstraintsType = Util::IsDetected_t<Constraints<>, ConstraintsDetector, T>;

		template<typename T>
		struct ExtractConstraintFields;

		template<int... Fields>
		struct ExtractConstraintFields<UniqueSubset<Fields...>>
		{
			QString operator() (const CachedFieldsData& data) const
			{
				return "UNIQUE (" + QStringList { data.Fields_.value (Fields)... }.join (", ") + ")";
			}
		};

		template<int... Fields>
		struct ExtractConstraintFields<PrimaryKey<Fields...>>
		{
			QString operator() (const CachedFieldsData& data) const
			{
				return "PRIMARY KEY (" + QStringList { data.Fields_.value (Fields)... }.join (", ") + ")";
			}
		};

		template<typename... Args>
		QStringList GetConstraintsStringList (Constraints<Args...>, const CachedFieldsData& data)
		{
			return { ExtractConstraintFields<Args> {} (data)... };
		}

		template<typename ImplFactory, typename T, size_t... Indices>
		QList<QString> GetTypes (std::index_sequence<Indices...>)
		{
			return { Type2Name<ImplFactory, ValueAtC_t<T, Indices>> {} ()... };
		}

		template<typename ImplFactory, typename T>
		QString AdaptCreateTable (const CachedFieldsData& data)
		{
			const auto& types = GetTypes<ImplFactory, T> (SeqIndices<T>);

			const auto& constraints = GetConstraintsStringList (ConstraintsType<T> {}, data);
			const auto& constraintsStr = constraints.isEmpty () ?
					QString {} :
					(", " + constraints.join (", "));

			const auto& statements = Util::ZipWith (types, static_cast<const QList<QString>&> (data.Fields_),
					[] (const QString& type, const QString& field) { return field + " " + type; });
			return "CREATE TABLE " +
					data.Table_ +
					" (" +
					statements.join (", ") +
					constraintsStr +
					");";
		}
	}

	template<auto... Ptrs>
	InsertAction::Replace::FieldsType<Ptrs...>::operator InsertAction::Replace () const
	{
		return { { detail::BuildCachedFieldsData<MemberPtrStruct_t<Ptrs>> ().Fields_.value (detail::FieldIndex<Ptrs> ())... } };
	}

	template<typename Seq>
	InsertAction::Replace::PKeyType<Seq>::operator InsertAction::Replace () const
	{
		static_assert (detail::HasPKey<Seq>, "Sequence does not have any primary keys");
		return { { detail::GetFieldName<Seq, detail::FindPKey<Seq>::result_type::value>::value () } };
	}

	template<typename T>
	struct ObjectInfo
	{
		detail::AdaptInsert<T> Insert;
		detail::AdaptUpdate<T> Update;
		detail::AdaptDelete<T> Delete;

		detail::SelectWrapper<T, detail::SelectBehaviour::Some> Select;
		detail::SelectWrapper<T, detail::SelectBehaviour::One> SelectOne;
		detail::DeleteByFieldsWrapper<T> DeleteBy;

		ObjectInfo (const ObjectInfo<T>&) = delete;
		ObjectInfo (ObjectInfo<T>&&) = default;
	};

	template<typename T, typename ImplFactory = detail::SQLite::ImplFactory>
	ObjectInfo<T> Adapt (const QSqlDatabase& db)
	{
		const auto& cachedData = detail::BuildCachedFieldsData<T> ();

		if (db.record (cachedData.Table_).isEmpty ())
			RunTextQuery (db, detail::AdaptCreateTable<ImplFactory, T> (cachedData));

		ImplFactory factory;

		return
		{
			{ db, cachedData, factory },
			{ db, cachedData },
			{ db, cachedData },
			{ db, cachedData },
			{ db, cachedData },
			{ db, cachedData }
		};
	}

	template<typename T>
	using ObjectInfo_ptr = std::shared_ptr<ObjectInfo<T>>;

	template<typename T, typename ImplFactory = SQLiteImplFactory>
	ObjectInfo_ptr<T> AdaptPtr (const QSqlDatabase& db)
	{
		return std::make_shared<ObjectInfo<T>> (Adapt<T, ImplFactory> (db));
	}
}
}
}
