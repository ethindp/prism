//-------------------------------------------------------------------------------------------------------
// moderncom - Part of HHD Software Belt library
// Copyright (C) 2017 HHD Software Ltd.
// Written by Alexander Bessonov
//
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#pragma once

#include <tuple>

namespace belt::mpl
{
	// vector
	template<class... Types>
	struct vector
	{
		typedef vector type;
	};

	// size
	template<class T>
	struct size;

	template<class... Types>
	struct size<vector<Types...>>
	{
		static const size_t value = sizeof...(Types);
	};

	// push_back
	template<class Vector, class T>
	struct push_back;

	template<class... Types, class T>
	struct push_back<vector<Types...>, T>
	{
		typedef vector<Types..., T> type;
	};

	template<class Vector, class T>
	using push_back_t = typename push_back<Vector, T>::type;

	// push_front
	template<class Vector, class T>
	struct push_front;

	template<class... Types, class T>
	struct push_front<vector<Types...>, T>
	{
		typedef vector<T, Types...> type;
	};

	template<class Vector, class T>
	using push_front_t = typename push_front<Vector, T>::type;

	// append
	template<class A, class B>
	struct append;

	template<class... Types, class B>
	struct append<vector<Types...>, B>
	{
		typedef vector<Types..., B> type;
	};

	template<class A, class... Types>
	struct append<A, vector<Types...>>
	{
		typedef vector<A, Types...> type;
	};

	template<class... TypesA, class... TypesB>
	struct append<vector<TypesA...>, vector<TypesB...>>
	{
		typedef vector<TypesA..., TypesB...> type;
	};

	template<class A, class B>
	using append_t = typename append<A, B>::type;

	// front
	template<class T>
	struct front;

	template<class First, class... Rest>
	struct front<vector<First, Rest...>>
	{
		typedef First type;
	};

	template<class T>
	using front_t = typename front<T>::type;

	// back
	template<class T>
	struct back;

	template<class T>
	struct back<vector<T>>
	{
		typedef T type;
	};

	template<class Head, class... Tail>
	struct back<vector<Head, Tail...>>
	{
		typedef typename back<vector<Tail...>>::type type;
	};

	template<class T>
	using back_t = typename back<T>::type;

	// remove_front
	template<class T>
	struct remove_front;

	template<class First, class... Rest>
	struct remove_front<vector<First, Rest...>>
	{
		typedef vector<Rest...> type;
	};

	template<class T>
	using remove_front_t = typename remove_front<T>::type;

	template<class T>
	struct remove_back;

	template<class T>
	struct remove_back<vector<T>>
	{
		typedef vector<> type;
	};

	template<class Head, class... Tail>
	struct remove_back<vector<Head, Tail...>>
	{
		typedef typename push_front<typename remove_back<vector<Tail...>>::type, Head>::type type;
	};

	template<class T>
	using remove_back_t = typename remove_back<T>::type;

	// as_tuple
	template<class T>
	struct as_tuple;

	template<class... Types>
	struct as_tuple<vector<Types...>>
	{
		typedef std::tuple<Types...> type;
	};

	template<class T>
	using as_tuple_t = typename as_tuple<T>::type;
}
