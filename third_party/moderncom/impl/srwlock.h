//-------------------------------------------------------------------------------------------------------
// moderncom - Part of HHD Software Belt library
// Copyright (C) 2017 HHD Software Ltd.
// Written by Alexander Bessonov
//
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#pragma once

namespace belt
{
	// Windows SRW lock wrapped in shared_mutex-friendly class
	class srwlock
	{
		SRWLOCK m_lock{};
	public:
		srwlock(const srwlock &) = delete;
		srwlock & operator=(const srwlock &) = delete;
		srwlock() noexcept = default;

		_Acquires_exclusive_lock_(&m_lock)
		void lock() noexcept
		{
			AcquireSRWLockExclusive(&m_lock);
		}

		_Acquires_shared_lock_(&m_lock)
		void lock_shared() noexcept
		{
			AcquireSRWLockShared(&m_lock);
		}

		_When_(return, _Acquires_exclusive_lock_(&m_lock))
		bool try_lock() noexcept
		{
			return 0 != TryAcquireSRWLockExclusive(&m_lock);
		}

		_Releases_exclusive_lock_(&m_lock)
		void unlock() noexcept
		{
			ReleaseSRWLockExclusive(&m_lock);
		}

		_Releases_shared_lock_(&m_lock)
		void unlock_shared() noexcept
		{
			ReleaseSRWLockShared(&m_lock);
		}
	};
}

