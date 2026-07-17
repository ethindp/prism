//-------------------------------------------------------------------------------------------------------
// moderncom - Part of HHD Software Belt library
// Copyright (C) 2017 HHD Software Ltd.
// Written by Alexander Bessonov
//
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


#pragma once
#include <stdexcept>
#include <string>
#include <cassert>
#include <cstdint>
#include <utility>
#include <functional>

#if !defined(GUID_DEFINED)
#define GUID_DEFINED
struct GUID {
	uint32_t Data1;
	uint16_t Data2;
	uint16_t Data3;
	uint8_t Data4[8];
};
#endif

namespace belt::com
{
	namespace details
	{
		constexpr const size_t short_guid_form_length = 36;	// {XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}
		constexpr const size_t long_guid_form_length = 38;	// XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX

		//
		constexpr int parse_hex_digit(const char c)
		{
			using namespace std::string_literals;
			if ('0' <= c && c <= '9')
				return c - '0';
			else if ('a' <= c && c <= 'f')
				return 10 + c - 'a';
			else if ('A' <= c && c <= 'F')
				return 10 + c - 'A';
			else
				throw std::domain_error{ "invalid character in GUID"s };
		}

		template<class T>
		constexpr T parse_hex(const char *ptr)
		{
			constexpr size_t digits = sizeof(T) * 2;
			T result{};
			for (size_t i = 0; i < digits; ++i)
				result |= parse_hex_digit(ptr[i]) << (4 * (digits - i - 1));
			return result;
		}

		constexpr GUID make_guid_helper(const char *begin)
		{
			GUID result{};
			result.Data1 = parse_hex<uint32_t>(begin);
			begin += 8 + 1;
			result.Data2 = parse_hex<uint16_t>(begin);
			begin += 4 + 1;
			result.Data3 = parse_hex<uint16_t>(begin);
			begin += 4 + 1;
			result.Data4[0] = parse_hex<uint8_t>(begin);
			begin += 2;
			result.Data4[1] = parse_hex<uint8_t>(begin);
			begin += 2 + 1;
			for (size_t i = 0; i < 6; ++i)
				result.Data4[i + 2] = parse_hex<uint8_t>(begin + i * 2);
			return result;
		}

		template<size_t N>
		constexpr GUID make_guid(const char(&str)[N])
		{
			using namespace std::string_literals;
			static_assert(N == (long_guid_form_length + 1) || N == (short_guid_form_length + 1), "String GUID of the form {XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX} or XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX is expected");

			if constexpr(N == (long_guid_form_length + 1))
			{
				if (str[0] != '{' || str[long_guid_form_length - 1] != '}')
					throw std::domain_error{ "Missing opening or closing brace"s };
			}

			return make_guid_helper(str + (N == (long_guid_form_length + 1) ? 1 : 0));
		}

		//
		template<class T, class>
		struct has_get_guid_impl : std::false_type {};

		template<class T>
		struct has_get_guid_impl<T, decltype(void(T::get_guid()))> : std::true_type {};

		template<class T>
		using has_get_guid = has_get_guid_impl<T, void>;

		template<class T, class>
		struct has_free_get_guid_impl : std::false_type {};

		template<class T>
		struct has_free_get_guid_impl<T, decltype(void(get_guid(std::declval<T *>())))> : std::true_type {};

		template<class T>
		using has_free_get_guid = has_free_get_guid_impl<T, void>;

		//
		template<class T>
		struct interface_wrapper
		{
			using type = T;
		};

		template<class T>
		constexpr const auto msvc_get_guid_workaround = T::get_guid();

		//
		template<class T>
		constexpr GUID get_interface_guid_impl(std::true_type, std::false_type)
		{
			return msvc_get_guid_workaround<T>;
		}

		template<class T, class Any>
		constexpr GUID get_interface_guid_impl(Any, std::true_type) noexcept
		{
			return get_guid(static_cast<T *>(nullptr));
		}

		template<class T>
		constexpr GUID get_interface_guid_impl(std::false_type, std::false_type) noexcept
		{
			return __uuidof(T);
		}

		//
		template<class T>
		constexpr GUID get_interface_guid(T);

		template<class Interface>
		constexpr GUID get_interface_guid(interface_wrapper<Interface>) noexcept
		{
			return get_interface_guid_impl<Interface>(has_get_guid<Interface>{}, has_free_get_guid<Interface>{});
		}
	}
	using details::make_guid;

	namespace literals
	{
		constexpr GUID operator ""_guid(const char *str, size_t N)
		{
			using namespace details;
			using namespace std::string_literals;

			if (!(N == long_guid_form_length || N == short_guid_form_length))
				throw std::domain_error{ "String GUID of the form {XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX} or XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX is expected"s };
			if (N == long_guid_form_length && (str[0] != '{' || str[long_guid_form_length - 1] != '}'))
				throw std::domain_error{ "Missing opening or closing brace"s };

			return details::make_guid_helper(str + (N == long_guid_form_length ? 1 : 0));
		}
	}
}

using namespace belt::com::literals;
