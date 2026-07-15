//-------------------------------------------------------------------------------------------------------
// moderncom - Part of HHD Software Belt library
// Copyright (C) 2017 HHD Software Ltd.
// Written by Alexander Bessonov
//
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#pragma once

#include "interfaces.h"

// Support for DllGetClassObject
namespace belt::com
{
	namespace details
	{
		class __declspec(novtable) Factory :
			public object<
			Factory,
			IClassFactory
			>
		{
			CLSID classId;
		public:
			Factory(const CLSID &classId) noexcept :
				classId{ classId }
			{}

			virtual HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppvObject) noexcept override
			{
				return belt::com::create_object(classId, riid, ppvObject, pUnkOuter);
			}

			virtual HRESULT STDMETHODCALLTYPE LockServer(BOOL fLock) noexcept override
			{
				if (fLock)
					ModuleCount::lock_count.fetch_add(1, std::memory_order_relaxed);
				else
					ModuleCount::lock_count.fetch_sub(1, std::memory_order_relaxed);

				return S_OK;
			}
		};
	}

	inline HRESULT DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) noexcept
	{
		try
		{
			auto factory = details::Factory::create_instance(rclsid).to_ptr<IUnknown>();
			return factory->QueryInterface(riid, ppv);
		}
		catch (const std::bad_alloc &)
		{
			return E_OUTOFMEMORY;
		}
		catch (const corsl::hresult_error &e)
		{
			return e.code();
		}
		catch (...)
		{
			return E_FAIL;
		}
	}

	inline HRESULT DllCanUnloadNow() noexcept
	{
		return details::ModuleCount::lock_count.load(std::memory_order_relaxed) ? S_FALSE : S_OK;
	}
}
