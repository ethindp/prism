//-------------------------------------------------------------------------------------------------------
// moderncom - Part of HHD Software Belt library
// Copyright (C) 2017 HHD Software Ltd.
// Written by Alexander Bessonov
//
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


#pragma once

namespace corsl
{
	namespace details
	{
		// Simplified version of hresult_error
		// May be used on Vista+
		class hresult_error
		{
			HRESULT m_code{ E_FAIL };

		public:
			constexpr hresult_error() = default;

			explicit constexpr hresult_error(const HRESULT code) noexcept :
				m_code{ code }
			{
			}

			constexpr HRESULT code() const noexcept
			{
				return m_code;
			}

			constexpr bool is_aborted() const noexcept
			{
				return m_code == HRESULT_FROM_WIN32(ERROR_OPERATION_ABORTED);
			}
		};

		[[noreturn]] inline void throw_error(HRESULT hr)
		{
			throw hresult_error{ hr };
		}

		[[noreturn]] inline void throw_win32_error(DWORD err)
		{
			throw_error(HRESULT_FROM_WIN32(err));
		}

		[[noreturn]] inline void throw_last_error()
		{
			throw_win32_error(GetLastError());
		}

		inline void check_hresult(HRESULT error)
		{
			if (FAILED(error))
				throw_error(error);
		}

		//inline void check_win32(DWORD error)
		//{
		//	if (error)
		//		throw hresult_error{ HRESULT_FROM_WIN32(error) };
		//}

		//inline void check_io(BOOL result)
		//{
		//	if (!result)
		//	{
		//		auto err = GetLastError();
		//		if (err != ERROR_IO_PENDING)
		//			throw hresult_error{ HRESULT_FROM_WIN32(err) };
		//	}
		//}

		//inline void check_win32_api(BOOL res)
		//{
		//	if (!res)
		//		throw hresult_error{ HRESULT_FROM_WIN32(GetLastError()) };
		//}
	}

	using details::hresult_error;

	using details::throw_error;
	using details::throw_win32_error;
	using details::throw_last_error;
	using details::check_hresult;
	//using details::check_win32;
	//using details::check_io;
	//using details::check_win32_api;
}
