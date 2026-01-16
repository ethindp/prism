//-------------------------------------------------------------------------------------------------------
// moderncom - Part of HHD Software Belt library
// Copyright (C) 2017 HHD Software Ltd.
// Written by Alexander Bessonov
//
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


#pragma once
#include <cassert>

#if !defined(BELT_COM_NO_LEAK_DETECTION) && defined(_DEBUG)
	#define BELT_HAS_LEAK_DETECTION 1
#else
	#define BELT_HAS_LEAK_DETECTION 0
#endif

#if !defined(BELT_COM_NO_CHECKED_REFS) && defined(_DEBUG)
	#define BELT_HAS_CHECKED_REFS 1
#else
	#define BELT_HAS_CHECKED_REFS 0
#endif


#if BELT_HAS_LEAK_DETECTION
#include <mutex>
#include <atomic>
// If you get a compilation error at the following line, do one of the following:
//		- Add boost 1.73.0 or later to your project
//		or
//		- #define BELT_COM_NO_LEAK_DETECTION before including any library's header to disable leak detection
#include <boost/stacktrace.hpp>
#endif

#if BELT_HAS_CHECKED_REFS
#include <vector>
#endif

#include "guid.h"
#include "impl/srwlock.h"
#include "impl/onexit.h"

#include <unknwn.h>

#include "impl/errors.h"

namespace belt::com
{
	namespace details
	{
		template<class Interface>
		class com_ptr;
#if BELT_HAS_LEAK_DETECTION

		struct leak_detection
		{
			int ordinal;
			boost::stacktrace::stacktrace stack;

			static int get_next() noexcept
			{
				static std::atomic<int> global_ordinal{};
				return ++global_ordinal;
			}

			leak_detection() noexcept :
				ordinal{ get_next() },
				stack{ boost::stacktrace::stacktrace() }
			{
			}
		};

		inline void init_leak_detection() noexcept
		{
			using namespace std::literals;
			SYSTEM_INFO si{};
			GetSystemInfo(&si);
			HANDLE section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, si.dwPageSize, (L"BELT_LEAK_DETECTION_SECTION."s + std::to_wstring(GetCurrentProcessId())).c_str());
			*reinterpret_cast<DWORD *>(MapViewOfFile(section, FILE_MAP_WRITE, 0, 0, si.dwPageSize)) = TlsAlloc();
			// We intentionally leave section mapped
		}

		inline DWORD get_slot() noexcept
		{
			using namespace std::literals;
			SYSTEM_INFO si{};
			GetSystemInfo(&si);
			HANDLE section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, si.dwPageSize, (L"BELT_LEAK_DETECTION_SECTION."s + std::to_wstring(GetCurrentProcessId())).c_str());
			assert(section && GetLastError() == ERROR_ALREADY_EXISTS && "init_leak_detection() must be called from EXE module as early as possible! It has not yet been called.");
			auto ptr = reinterpret_cast<DWORD *>(MapViewOfFile(section, FILE_MAP_WRITE, 0, 0, si.dwPageSize));
			assert(ptr);
			CloseHandle(section);
			auto ret = *ptr;
			UnmapViewOfFile(ptr);
			return ret;
		}

		inline void set_current_cookie(int cookie) noexcept
		{
			static DWORD slot = get_slot();
			TlsSetValue(slot, reinterpret_cast<void *>(static_cast<uintptr_t>(cookie)));
		}

		inline int get_current_cookie() noexcept
		{
			static DWORD slot = get_slot();
			auto retval = TlsGetValue(slot);
			TlsSetValue(slot, nullptr);
			return static_cast<int>(reinterpret_cast<uint64_t>(retval));
		}
#else
		inline void init_leak_detection() noexcept
		{
		}
#endif
		struct attach_t {};

		template<class Interface>
		class ref;

		template<class Interface>
		class __declspec(empty_bases) com_ptr
		{
			friend class ref<Interface>;

			friend Interface *&internal_get(com_ptr<Interface> &obj) noexcept
			{
				return obj.p;
			}

			Interface *p{};
#if BELT_HAS_CHECKED_REFS
			std::vector<ref<Interface> *> weaks;
#endif

#if BELT_HAS_LEAK_DETECTION
			int cookie{};

			friend int &internal_get_cookie(com_ptr<Interface> &obj) noexcept
			{
				return obj.cookie;
			}
#endif

			// smart construction from other interfaces
			template<class OtherInterface>
			com_ptr(OtherInterface *punk, std::true_type) noexcept :
				p{ static_cast<Interface *>(punk) }
			{
				addref_pointer(p);
			}

			template<class OtherInterface>
			com_ptr(OtherInterface *punk, std::false_type) noexcept
			{
				if (punk)
				{
					punk->QueryInterface(get_interface_guid(interface_wrapper<Interface>{}), reinterpret_cast<void **>(&p));
					store_cookie();
				}
			}
			//

			//
			template<class OtherInterface>
			com_ptr(com_ptr<OtherInterface> &&o, std::false_type) noexcept
			{
				if (o)
				{
					if (SUCCEEDED(o->QueryInterface(get_interface_guid(interface_wrapper<Interface>{}), reinterpret_cast<void **>(&p))))
					{
						store_cookie();
						o.release();
					}
				}
			}

			template<class OtherInterface>
			com_ptr(com_ptr<OtherInterface> &&o, std::true_type) noexcept :
				p{ static_cast<Interface *>(o.get()) }
#if BELT_HAS_LEAK_DETECTION
				, cookie{ internal_get_cookie(o) }
#endif
			{
				internal_get(o) = nullptr;
#if BELT_HAS_LEAK_DETECTION
				internal_get_cookie(o) = 0;
#endif
			}
			//
			template<class OtherInterface>
			com_ptr(const com_ptr<OtherInterface> &o, std::false_type) noexcept
			{
				if (o)
				{
					o->QueryInterface(get_interface_guid(interface_wrapper<Interface>{}), reinterpret_cast<void **>(&p));
					store_cookie();
				}
			}

			template<class OtherInterface>
			com_ptr(const com_ptr<OtherInterface> &o, std::true_type) noexcept :
				p{ static_cast<Interface *>(o.get()) }
			{
				addref_pointer(p);
			}

#if BELT_HAS_LEAK_DETECTION
			void store_cookie() noexcept
			{
				cookie = get_current_cookie();
			}
#else
			static void store_cookie() noexcept
			{
			}
#endif

			void addref_pointer(IUnknown *pint) noexcept
			{
				if (pint)
				{
					pint->AddRef();
					store_cookie();
				}
			}

			void release_pointer(IUnknown *pint) noexcept
			{
				if (pint)
				{
#if BELT_HAS_LEAK_DETECTION
					set_current_cookie(std::exchange(cookie, 0));
#endif
					pint->Release();
				}
			}

		public:
			explicit operator bool() const noexcept
			{
				return !!p;
			}

			com_ptr() = default;
			com_ptr(std::nullptr_t) noexcept {}

			com_ptr(Interface *p) noexcept :
				p{ p }
			{
				addref_pointer(p);
			}

			// the following constructor does not call addref (attaching constructor)
			com_ptr(attach_t, Interface *p) noexcept :
				p{ p }
			{
				store_cookie();		// might get incorrect cookie, check
			}

			template<class OtherInterface>
			com_ptr(OtherInterface *punk) noexcept :
				com_ptr(punk, std::is_base_of<Interface, OtherInterface>{})
			{}

			template<class OtherInterface>
			com_ptr(const com_ptr<OtherInterface> &o) noexcept :
				com_ptr(o, std::is_base_of<Interface, OtherInterface>{})
			{}

			template<class OtherInterface>
			com_ptr(com_ptr<OtherInterface> &&o) noexcept :
				com_ptr(std::move(o), std::is_base_of<Interface, OtherInterface>{})
			{}

			void release() noexcept
			{
				if (p)
				{
					release_pointer(p);
					p = nullptr;
				}
			}

			void reset() noexcept
			{
				release();
			}

			~com_ptr() noexcept
			{
				release_pointer(p);
#if BELT_HAS_CHECKED_REFS
				assert(weaks.empty() && "There was ref<Interface> constructed from this com_ptr that outlived this object!");
#endif
			}

			com_ptr(const com_ptr &o) noexcept :
			p{ o.p }
			{
				addref_pointer(p);
			}

			template<class OtherInterface>
			com_ptr(ref<OtherInterface> o) noexcept;

			com_ptr &operator =(const com_ptr &o) noexcept
			{
				release_pointer(p);
				p = o.p;
				addref_pointer(p);
				return *this;
			}

			com_ptr(com_ptr &&o) noexcept :
				p{ o.p }
#if BELT_HAS_LEAK_DETECTION
				, cookie {o.cookie }
#endif
			{
				o.p = nullptr;
#if BELT_HAS_LEAK_DETECTION
				o.cookie = 0;
#endif
			}

			com_ptr &operator =(com_ptr &&o) noexcept
			{
#if BELT_HAS_LEAK_DETECTION
				std::swap(cookie, o.cookie);
#endif
				std::swap(p, o.p);
				return *this;
			}

			Interface *operator ->() const noexcept
			{
				return p;
			}

			// Comparison operators
			bool operator ==(const com_ptr &o) const noexcept
			{
				return p == o.p;
			}

			bool operator !=(const com_ptr &o) const noexcept
			{
				return p != o.p;
			}

			bool operator ==(Interface *pother) const noexcept
			{
				return p == pother;
			}

			bool operator !=(Interface *pother) const noexcept
			{
				return p != pother;
			}

			friend bool operator ==(Interface *p1, const com_ptr &p2) noexcept
			{
				return p1 == p2.p;
			}

			friend bool operator !=(Interface *p1, const com_ptr &p2) noexcept
			{
				return p1 != p2.p;
			}

			// Ordering operator
			bool operator<(const com_ptr &o) const noexcept
			{
				return p < o.p;
			}

			// Attach and detach operations
			void attach(Interface *p_) noexcept
			{
				assert(!p);
				p = p_;
				store_cookie();	// might get incorrect cookie
			}

			[[nodiscard]]
			Interface *detach() noexcept
			{
				Interface *cur{};
				std::swap(cur, p);
#if BELT_HAS_LEAK_DETECTION
				cookie = 0;
#endif

				return cur;
			}

			// Pointer accessors
			Interface *get() const noexcept
			{
				return p;
			}

#if BELT_HAS_LEAK_DETECTION
			int get_cookie() const noexcept
			{
				return cookie;
			}
#endif

			Interface **put() noexcept
			{
				assert(!p && "Using put on non-empty object is prohibited");
				return &p;
			}

			// Conversion operations
			template<class TargetInterface>
			auto as() const noexcept
			{
				return com_ptr<TargetInterface>{*this};
			}

			template<class T>
			HRESULT QueryInterface(T **ppresult) const
			{
				return p->QueryInterface(get_interface_guid(interface_wrapper<T>{}), reinterpret_cast<void **>(ppresult));
			}

			HRESULT CoCreateInstance(const GUID &clsid, IUnknown *pUnkOuter = nullptr, DWORD dwClsContext = CLSCTX_ALL) noexcept
			{
				assert(!p && "Calling CoCreateInstance on initialized object is prohibited");
				SCOPE_EXIT
				{
					store_cookie();
				};

				return ::CoCreateInstance(clsid, pUnkOuter, dwClsContext, get_interface_guid(interface_wrapper<Interface>{}), reinterpret_cast<void **>(&p));
			}

			HRESULT create_instance(const GUID &clsid, IUnknown *pUnkOuter = nullptr, DWORD dwClsContext = CLSCTX_ALL) noexcept
			{
				return CoCreateInstance(clsid, pUnkOuter, dwClsContext);
			}

			static com_ptr create(const GUID &clsid, IUnknown *pUnkOuter = nullptr, DWORD dwClsContext = CLSCTX_ALL)
			{
				com_ptr result;
				auto hr = result.CoCreateInstance(clsid, pUnkOuter, dwClsContext);
				if (SUCCEEDED(hr))
					return result;
				else
					corsl::throw_error(hr);
			}

#if BELT_HAS_CHECKED_REFS
			void add_weak(ref<Interface> *pweak)
			{
				weaks.push_back(pweak);
			}

			void remove_weak(ref<Interface> *pweak)
			{
				std::erase(weaks, pweak);
			}
#endif
		};

		constexpr const attach_t attach = {};


		template<class Interface>
		class __declspec(empty_bases) ref
		{
			Interface *p{};
#if BELT_HAS_CHECKED_REFS
			com_ptr<Interface> *parent{};
#endif
			struct move_tag {};

			template<class OtherInterface>
			ref(OtherInterface *p, std::true_type) noexcept :
				p{ static_cast<Interface *>(p) }
			{}

			template<class OtherInterface>
			ref(com_ptr<OtherInterface> &&o, std::true_type) noexcept :
				p{ static_cast<Interface *>(o.p) }
			{
#if BELT_HAS_CHECKED_REFS
				parent = &o;
				// We allow construction from temporary com_ptr, but in DEBUG build we make sure the com_ptr lives long enough
				o.add_weak(this);
#endif
			}

		public:
			explicit operator bool() const noexcept
			{
				return !!p;
			}

			ref() = default;
			ref(std::nullptr_t) noexcept {}

			ref(const com_ptr<Interface> &o) noexcept :
				p{ o.p }
			{
			}

			ref(com_ptr<Interface> &&o) noexcept :
				p{ o.p }
			{
#if BELT_HAS_CHECKED_REFS
				parent = &o;
				// We allow construction from temporary com_ptr, but in DEBUG build we make sure the com_ptr lives long enough
				o.add_weak(this);
#endif
			}

			// allow construction from derived interfaces
			template<class OtherInterface>
			ref(const com_ptr<OtherInterface> &o) noexcept :
				ref(o.get(), std::is_base_of<Interface, OtherInterface>{})
			{}

			template<class OtherInterface>
			ref(com_ptr<OtherInterface> &&o) noexcept :
				ref(move_tag{}, std::move(o), std::is_base_of<Interface, OtherInterface>{})
			{}

			template<class OtherInterface>
			ref(const ref<OtherInterface> &o) noexcept :
				ref(o.get(), std::is_base_of<Interface, OtherInterface>{})
			{}

#if BELT_HAS_CHECKED_REFS
			~ref()
			{
				if (parent)
					parent->remove_weak(this);
			}

			ref(const ref &o) :
				p{ o.p },
				parent{ o.parent }
			{
				if (parent)
					parent->add_weak(this);
			}

			ref(ref &&o) noexcept :
				p{ o.p },
				parent{ o.parent }
			{
				if (parent)
				{
					parent->remove_weak(&o);
					parent->add_weak(this);
				}
			}
#endif

			ref(Interface *p) noexcept :
				p{ p }
			{
			}

			template<class T>
			ref &operator =(const T &) = delete;

			Interface *operator ->() const noexcept
			{
				return p;
			}

			// Comparison operators
			bool operator ==(const ref &o) const noexcept
			{
				return p == o.p;
			}

			bool operator !=(const ref &o) const noexcept
			{
				return p != o.p;
			}

			bool operator ==(Interface *pother) const noexcept
			{
				return p == pother;
			}

			bool operator !=(Interface *pother) const noexcept
			{
				return p != pother;
			}

			friend bool operator ==(Interface *p1, const ref &p2) noexcept
			{
				return p1 == p2.p;
			}

			friend bool operator !=(Interface *p1, const ref &p2) noexcept
			{
				return p1 != p2.p;
			}

			bool operator ==(const com_ptr<Interface> &o) noexcept
			{
				return p == o.p;
			}

			bool operator !=(const com_ptr<Interface> &o) noexcept
			{
				return p != o.p;
			}

			// Ordering operator
			bool operator<(const ref &o) const noexcept
			{
				return p < o.p;
			}

			// Pointer accessors
			Interface *get() const noexcept
			{
				return p;
			}

			// Conversion operations
			template<class TargetInterface>
			auto as() const noexcept
			{
				return com_ptr<TargetInterface>{ p };
			}
		};

		template<class Interface>
		template<class OtherInterface>
		inline com_ptr<Interface>::com_ptr(ref<OtherInterface> o) noexcept : com_ptr{ o.get() }
		{
		}
	}

	using details::com_ptr;
	using details::ref;

	using details::attach;
	using details::init_leak_detection;
}

namespace bcom
{
	// Possibly guard by macro
	template<class T>
	using ptr = belt::com::com_ptr<T>;
	template<class T>
	using ref = belt::com::ref<T>;
}
