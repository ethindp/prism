//-------------------------------------------------------------------------------------------------------
// moderncom - Part of HHD Software Belt library
// Copyright (C) 2017 HHD Software Ltd.
// Written by Alexander Bessonov
//
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


#pragma once
#include <atomic>
#include <type_traits>
#include <mutex>

#if defined(_DEBUG)
#include <vector>
#endif

#include "impl/vector.h"
#include "impl/errors.h"

#include "com_ptr.h"

namespace belt::com
{
	namespace details
	{
		struct ModuleCount
		{
			static std::atomic<int> lock_count;
		};

		inline __declspec(selectany) std::atomic<int> ModuleCount::lock_count{};

#pragma region object
		struct supports_aggregation_t {};
		struct singleton_factory_t {};
		struct smart_singleton_factory_t {};
		struct increments_module_count_t {};
		struct enable_leak_detection_t {};

		struct delayed_t {};
		constexpr const delayed_t delayed = {};

		template<class Interface>
		struct __declspec(empty_bases)embeds_interface_id {};

		// get_first
		template<class T>
		concept has_get_first = requires
		{
			T::get_first();
		};

		template<class T>
		concept has_implements = requires
		{
			typename T::can_query;
		};

		// increments_module_count
		template<class T>
		concept has_increments_module_count = requires
		{
			typename T::increments_module_count_t;
			requires std::same_as<typename T::increments_module_count_t, increments_module_count_t>;
		};

		// supports_aggregation
		template<class T, class>
		struct has_supports_aggregation_impl : std::false_type {};

		template<class T>
		struct has_supports_aggregation_impl<T, std::void_t<std::is_same<typename T::supports_aggregation_t, supports_aggregation_t>>> : std::true_type {};

		template<class T>
		using has_supports_aggregation = has_supports_aggregation_impl<T, void>;

		// enable_leak_detector
		template<class T, class>
		struct has_enable_leak_detector_impl : std::false_type {};

		template<class T>
		struct has_enable_leak_detector_impl<T, std::void_t<std::is_same<typename T::enable_leak_detection_t, enable_leak_detection_t>>> : std::true_type {};

		template<class T>
		using has_enable_leak_detector_helper = has_enable_leak_detector_impl<T, void>;

		template<class T>
		using has_enable_leak_detector = typename has_enable_leak_detector_helper<T>::type;

		// singleton
		template<class T, class>
		struct has_singleton_factory_impl : std::false_type {};

		template<class T>
		struct has_singleton_factory_impl<T, std::void_t<std::is_same<typename T::singleton_factory_t, singleton_factory_t>>> : std::true_type {};

		template<class T>
		using has_singleton_factory = has_singleton_factory_impl<T, void>;

		template<class T, class>
		struct has_smart_singleton_factory_impl : std::false_type {};

		template<class T>
		struct has_smart_singleton_factory_impl<T, std::void_t<std::is_same<typename T::smart_singleton_factory_t, smart_singleton_factory_t>>> : std::true_type {};

		template<class T>
		using has_smart_singleton_factory = has_smart_singleton_factory_impl<T, void>;

		//

		template<class Interface, class T>
		inline void *query_single([[maybe_unused]] T *pobj, [[maybe_unused]] const GUID &iid) noexcept
		{
			if constexpr (has_implements<Interface>)
				return Interface::query_self(pobj, iid);
			else if constexpr (std::is_same_v<Interface, IUnknown>)
				return nullptr;
			else if (iid == get_interface_guid(interface_wrapper<Interface>{}))
			{
				auto ret = static_cast<Interface *>(pobj);
				ret->AddRef();
				return ret;
			}
			else
				return nullptr;
		}

		template<class T, class...Interfaces>
		inline void *query(T *pobj, const GUID &iid, mpl::vector<Interfaces...>) noexcept
		{
			void *result{ nullptr };
			(void)(... || (nullptr != (result = query_single<Interfaces>(pobj, iid))));
			return result;
		}

		template<class...Interfaces>
		struct __declspec(empty_bases)extends_base : public Interfaces...
		{
			template<class Derived>
			static void *query_children(Derived *pobject, const GUID &riid) noexcept
			{
				return query(pobject, riid, mpl::vector<Interfaces... > {});
			}
		};

		// extends marks derived as pure interface
		template<class ThisInterface, class...Interfaces>
		struct __declspec(empty_bases)extends : public extends_base<Interfaces...>
		{
			struct can_query
			{
				using type = mpl::vector<Interfaces...>;
			};

			template<class Derived>
			static void *query_self(Derived *pobject, const GUID &iid) noexcept
			{
				if (get_interface_guid(interface_wrapper<ThisInterface>{}) == iid)
				{
					auto ret = static_cast<ThisInterface *>(pobject);
					ret->AddRef();
					return ret;
				}
				else
					return extends_base<Interfaces...>::query_children(pobject, iid);
			}
		};

		template<class T>
		inline constexpr auto get_first_impl(interface_wrapper<T>, std::true_type) noexcept
		{
			return T::get_first();
		}

		template<class T>
		inline constexpr auto get_first_impl(interface_wrapper<T> v, std::false_type) noexcept
		{
			return v;
		}

		template<class T>
		inline constexpr auto get_first([[maybe_unused]] interface_wrapper<T> v) noexcept
		{
			if constexpr (has_get_first<T>)
				return T::get_first();
			else
				return v;
		}

		template<class ThisClass, class FirstInterface, class...RestInterfaces>
		struct __declspec(empty_bases)intermediate : public extends_base<FirstInterface, RestInterfaces...>
		{
			struct can_query
			{
				using type = mpl::vector<FirstInterface, RestInterfaces...>;
			};

			template<class Derived>
			static void *query_self(Derived *pobject, const GUID &iid) noexcept
			{
				return extends_base<FirstInterface, RestInterfaces...>::query_children(pobject, iid);
			}

			static constexpr auto get_first() noexcept
			{
				return details::get_first(interface_wrapper<FirstInterface>{});
			}
		};

		template<class ThisClass>
		struct __declspec(empty_bases)eats_all : public extends_base<>
		{
			struct can_query
			{
				using type = mpl::vector<>;
			};

			template<class Derived>
			static void *query_self(Derived *pobject, const GUID &iid) noexcept
			{
				return static_cast<ThisClass *>(pobject)->on_eat_all(iid);
			}
		};

		template<class ThisClass, class...Interfaces>
		struct __declspec(empty_bases)aggregates
		{
			struct can_query
			{
				using type = mpl::vector<Interfaces...>;
			};

			template<class Derived>
			static void *query_self(Derived *pobject, const GUID &iid) noexcept
			{
				void *result{ nullptr };
				(... || (iid == get_interface_guid(interface_wrapper<Interfaces>{}) ? (result = static_cast<ThisClass *>(pobject)->on_query(interface_wrapper<Interfaces>{})), true : false));
				return result;
			}
		};

		template<class...Interfaces>
		struct __declspec(empty_bases)also	// no inheriting from interfaces!
		{
			struct can_query
			{
				using type = mpl::vector<Interfaces...>;
			};

			template<class Derived>
			static void *query_self(Derived *pobject, const GUID &iid) noexcept
			{
				return query(pobject, iid, mpl::vector<Interfaces... > {});
			}
		};

		template<class T, class...Args>
		concept has_legacy_final_construct = requires(T obj, Args &&...args)
		{
			{obj.FinalConstruct(std::forward<Args>(args)...)}->std::same_as<HRESULT>;
		};

		template<class T,class...Args>
		concept has_final_construct = requires(T obj, Args &&...args)
		{
			{obj.final_construct(std::forward<Args>(args)...)}->std::same_as<HRESULT>;
		};

		//template<class T, class Pack, class Enable>
		//struct has_final_construct_impl : std::false_type {};

		//template<class T, class...Args>
		//struct has_final_construct_impl<T, mpl::vector<Args...>, decltype(void(std::declval<T &>().FinalConstruct(std::declval<Args>()...)))> : std::true_type {};

		//template<class T, class...Args>
		//using has_final_construct = has_final_construct_impl<T, mpl::vector<Args...>, void>;

		template<class T>
		concept has_legacy_final_release = requires(T val)
		{
			val.FinalRelease();
		};

		template<class T, class Holder = T>
		concept has_final_release = requires(std::unique_ptr<Holder> instance)
		{
			T::final_release(std::move(instance));
		};

		template<class T, class Enable>
		struct has_on_release_impl : std::false_type {};

		template<class T>
		struct has_on_release_impl<T, decltype(void(std::declval<const T &>().on_release(std::declval<int>())))> : std::true_type {};

		template<class T>
		using has_on_release = has_on_release_impl<T, void>;

		template<class T, class Enable>
		struct has_on_add_ref_impl : std::false_type {};

		template<class T>
		struct has_on_add_ref_impl<T, decltype(void(std::declval<const T &>().on_add_ref(std::declval<int>())))> : std::true_type {};

		template<class T>
		using has_on_add_ref = has_on_add_ref_impl<T, void>;

		template<class Derived>
		class __declspec(empty_bases)contained_value final : public Derived
		{
			IUnknown *pOuterUnknown;

		public:
			template<class...Args>
			contained_value(IUnknown *pOuterUnknown, Args &&...args) :
				pOuterUnknown{ pOuterUnknown },
				Derived{ std::forward<Args>(args)... }
			{}

			virtual ULONG STDMETHODCALLTYPE AddRef() noexcept override
			{
				return pOuterUnknown->AddRef();
			}

			virtual ULONG STDMETHODCALLTYPE Release() noexcept override
			{
				return pOuterUnknown->Release();
			}

			virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) noexcept override
			{
				return pOuterUnknown->QueryInterface(riid, ppvObject);
			}

			HRESULT RealQueryInterface(REFIID riid, void **ppvObject) noexcept
			{
				return Derived::QueryInterface(riid, ppvObject);
			}
		};

		struct ref_count_base
		{
			std::atomic<int> _rc_refcount{};

			void safe_increment() noexcept
			{
				_rc_refcount.fetch_add(10, std::memory_order_relaxed);
			}

			void safe_decrement() noexcept
			{
				_rc_refcount.fetch_sub(10, std::memory_order_relaxed);
			}
		};

#if defined(_DEBUG)
		using no_count_base = ref_count_base;
#else
		struct no_count_base
		{
			static void safe_increment() noexcept
			{
			}

			static void safe_decrement() noexcept
			{
			}
		};
#endif

		template<class Enabled>
		struct usage_map_base
		{
			static void add_cookie() noexcept
			{}

			static void remove_cookie() noexcept
			{}
		};

#if BELT_HAS_LEAK_DETECTION
		template<>
		struct usage_map_base<std::true_type>
		{
			std::vector<leak_detection *> umb_usages;
			belt::srwlock umb_lock;

			void add_cookie() noexcept
			{
				try
				{
					auto cookie = new leak_detection;

					{
						std::scoped_lock l{ umb_lock };
						umb_usages.emplace_back(cookie);
					}
					set_current_cookie(cookie->ordinal);
				}
				catch (...)
				{
				}
			}

			void remove_cookie() noexcept
			{
				try
				{
					auto cookie = get_current_cookie();
					if (cookie)
					{
						std::scoped_lock l{ umb_lock };
						auto it = std::find_if(umb_usages.begin(), umb_usages.end(), [cookie](auto * leak)
							{
								return leak->ordinal == cookie;
							});
						if (umb_usages.end() != it)
						{
							delete *it;
							umb_usages.erase(it);
						}
						else
							assert(false && "Cookie is not found in a map");
					}
				}
				catch (...)
				{
				}
			}
		};
#endif

		template<class Derived, class Base>
		struct __declspec(empty_bases) final_construct_support : Base, usage_map_base<has_enable_leak_detector<Derived>>
		{
			template<class...Args>
			void do_final_construct([[maybe_unused]] Derived &obj, [[maybe_unused]] Args &&...args)
			{
				static_assert(!has_legacy_final_construct<Derived, Args...>, "Legacy FinalConstruct is no longer supported. Replace with new final_construct (syntax does not change).");
				if constexpr (has_final_construct<Derived, Args...>)
				{
					Base::safe_increment();
					auto hr = obj.final_construct(std::forward<Args>(args)...);
					if (FAILED(hr))
						corsl::throw_error(hr);
					Base::safe_decrement();
				}
				if constexpr (has_increments_module_count<Derived>)
					ModuleCount::lock_count.fetch_add(1, std::memory_order_relaxed);
			}

			template<class Holder>
			static void do_final_release(std::unique_ptr<Holder> obj, [[maybe_unused]] std::atomic<int> &refcount) noexcept
			{
				static_assert(!has_legacy_final_release<Derived>, "Legacy FinalRelease no longer supported. Use new style final_release instead");
				if constexpr (has_final_release<Derived, Holder>)
				{
					refcount.store(1, std::memory_order_relaxed);	// allow for safe QueryInterface for an overloaded final_release function
					Derived::final_release(std::move(obj));
				}
				else
				{
					obj.reset();
				}

				if constexpr (has_increments_module_count<Derived>)
					ModuleCount::lock_count.fetch_sub(1, std::memory_order_relaxed);
			}

			//
			static void debug_on_add_ref(const Derived &obj, int value, std::true_type) noexcept
			{
				obj.on_add_ref(value);
			}

			static void debug_on_add_ref(const Derived &, int, std::false_type) noexcept
			{
			}

			void debug_on_add_ref(const Derived &obj, int value) noexcept
			{
				usage_map_base<has_enable_leak_detector<Derived>>::add_cookie();
				debug_on_add_ref(obj, value, has_on_add_ref<Derived>{});
			}

			//
			static void debug_on_release(const Derived &obj, int value, std::true_type) noexcept
			{
				obj.on_release(value);
			}

			static void debug_on_release(const Derived &, int, std::false_type) noexcept
			{
			}

			void debug_on_release(const Derived &obj, int value) noexcept
			{
				usage_map_base<has_enable_leak_detector<Derived>>::remove_cookie();
				debug_on_release(obj, value, has_on_release<Derived>{});
			}
		};

		template<class Derived>
		class __declspec(empty_bases)aggvalue final: public final_construct_support<Derived, ref_count_base>, public IUnknown
		{
			contained_value<Derived> object;

			using final_construct_support<Derived, ref_count_base>::_rc_refcount;

		public:
			template<class...Args>
			aggvalue(IUnknown *pOuterUnknown, Args &&...args) :
				object{ pOuterUnknown, std::forward<Args>(args)... }
			{
				static_assert(!has_final_release<Derived> || has_final_release<Derived, aggvalue<Derived>>, "Class overrides final_release, but does not work with aggregate values. Consider taking templated holder if your object can be aggregated.");
				this->do_final_construct(object);
			}

			virtual ULONG STDMETHODCALLTYPE AddRef() noexcept override
			{
				return _rc_refcount.fetch_add(1, std::memory_order_relaxed) + 1;
			}

			virtual ULONG STDMETHODCALLTYPE Release() noexcept override
			{
				auto prev = _rc_refcount.fetch_sub(1, std::memory_order_relaxed);
				if (prev == 1)
				{
					this->do_final_release(std::unique_ptr<aggvalue>{this}, _rc_refcount);
				}
				return prev - 1;
			}

			virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) noexcept override
			{
				if (riid == get_interface_guid(interface_wrapper<IUnknown>{}))
				{
					*ppvObject = static_cast<IUnknown *>(this);
					AddRef();
					return S_OK;
				}
				else
				{
					// "real" query interface
					return object.RealQueryInterface(riid, ppvObject);
				}
			}

			Derived *get() noexcept
			{
				return &object;
			}
		};

		template<class DerivedNonMatchingName>
		class __declspec(empty_bases)value : public DerivedNonMatchingName, public final_construct_support<DerivedNonMatchingName, ref_count_base>
		{
		public:
			virtual ~value() = default;
			value(const value &o) :
				DerivedNonMatchingName{ static_cast<const DerivedNonMatchingName &>(o) }
			{
				this->do_final_construct(*this);
			}

			template<class...Args>
			value(Args &&...args) : DerivedNonMatchingName{ std::forward<Args>(args)... }
			{
				this->do_final_construct(*this);
			}

			template<class...Args>
			value(delayed_t, Args &&...args)
			{
				this->do_final_construct(*this, std::forward<Args>(args)...);
			}

			virtual ULONG STDMETHODCALLTYPE AddRef() noexcept override
			{
				auto ret = this->_rc_refcount.fetch_add(1, std::memory_order_relaxed) + 1;
				this->debug_on_add_ref(*static_cast<const DerivedNonMatchingName *>(this), ret);
				return ret;
			}

			virtual ULONG STDMETHODCALLTYPE Release() noexcept override
			{
				auto prev = this->_rc_refcount.fetch_sub(1, std::memory_order_relaxed);
				this->debug_on_release(*static_cast<const DerivedNonMatchingName *>(this), prev);

				if (prev == 1)
				{
					this->do_final_release(std::unique_ptr<DerivedNonMatchingName>{ this }, this->_rc_refcount);
				}

				return prev - 1;
			}
		};

		template<class DerivedNonMatchingName>
		class __declspec(empty_bases)smart_singleton_value final : public value<DerivedNonMatchingName>
		{
			std::shared_ptr<DerivedNonMatchingName> self{ static_cast<DerivedNonMatchingName *>(this), [](auto *) {} };
		public:
			std::weak_ptr<DerivedNonMatchingName> get_weak() const noexcept
			{
				return self;
			}
		};

		template<class Derived>
		class __declspec(empty_bases)value_on_stack : public Derived, public final_construct_support<Derived, no_count_base>
		{
		public:
			value_on_stack(const value_on_stack &) = delete;
			value_on_stack &operator =(const value_on_stack &) = delete;

			template<class...Args>
			value_on_stack(Args &&...args) : Derived{ std::forward<Args>(args)... }
			{
				static_assert(!has_final_release<Derived>, "Classes with final_release cannot be used on stack");
				this->do_final_construct(*this);
			}

			template<class...Args>
			value_on_stack(delayed_t, Args &&...args)
			{
				static_assert(!has_final_release<Derived>, "Classes with final_release cannot be used on stack");
				this->do_final_construct(*this, std::forward<Args>(args)...);
			}

#if defined(_DEBUG)
			~value_on_stack()
			{
				assert(0 == this->_rc_refcount.load(std::memory_order_relaxed) && "value_on_stack is destroyed while still being referenced!");
			}
#endif

			virtual ULONG STDMETHODCALLTYPE AddRef() noexcept override
			{
#if defined(_DEBUG)
				this->_rc_refcount.fetch_add(1, std::memory_order_relaxed);
#endif
				return 2;
			}

			virtual ULONG STDMETHODCALLTYPE Release() noexcept override
			{
#if defined(_DEBUG)
				this->_rc_refcount.fetch_sub(1, std::memory_order_relaxed);
#endif
				return 2;
			}
		};

		// trait checking

		template<template<class> class Trait, class First, class...Rest>
		constexpr bool check_trait_deep() noexcept;

		template<template<class> class Trait, class T>
		constexpr bool check_trait_vector() noexcept;

		template<template<class> class Trait, class...Interfaces>
		constexpr bool check_trait_vector(mpl::vector<Interfaces...>) noexcept
		{
			return check_trait_deep<Trait, Interfaces...>();
		}

		template<template<class> class Trait, class Class>
		constexpr bool check_trait_single(std::true_type) noexcept
		{
			if constexpr (Trait<Class>{})
				return true;
			else
				return check_trait_vector<Trait>(typename Class::can_query::type{});
		}

		template<template<class> class Trait, class Class>
		constexpr bool check_trait_single(std::false_type) noexcept
		{
			return Trait<Class>{};
		}

		template<template<class> class Trait, class First, class...Rest>
		constexpr bool check_trait_deep() noexcept
		{
			if constexpr (check_trait_single<Trait, First>(Trait<First>{}))
				return true;
			else if constexpr (sizeof...(Rest) != 0)
				return check_trait_deep<Trait, Rest...>();
			else
				return false;
		}

		template<class T>
		class object_holder
		{
			std::unique_ptr<T> value;

			//
			template<class Interface>
			com_ptr<Interface> get_impl(std::false_type) noexcept
			{
				return { static_cast<Interface *>(value.release()) };
			}

			template<class Interface>
			com_ptr<IUnknown> get_impl(std::true_type) noexcept
			{
				return value.release()->GetUnknown();
			}

		public:
			object_holder(std::unique_ptr<T> &&value) noexcept :
				value{ std::move(value) }
			{}

			template<class Interface>
			std::enable_if_t<std::is_convertible_v<T *, Interface *> || std::is_same_v<IUnknown, Interface>, com_ptr<Interface>> to_ptr() && noexcept
			{
				return get_impl<Interface>(std::is_same<IUnknown, Interface>{});
			}

			auto to_ptr() && noexcept
			{
				return std::move(*this).template to_ptr<typename T::DefaultInterface>();
			}

			T *obj() const noexcept
			{
				return value.get();
			}

			T *release() noexcept
			{
				return value.release();
			}
		};

		template<class Derived, class FirstInterface, class...OtherInterfaces>
		class __declspec(empty_bases)object : public extends_base<FirstInterface, OtherInterfaces...>
		{
			using fint_t = decltype(details::get_first(interface_wrapper<FirstInterface>{}));
			using FirstRealInterface = typename fint_t::type;
			static_assert(!std::is_same<FirstRealInterface, IUnknown>{}, "Do not directly derive from IUnknown");

			template<template<class> class Trait>
			static constexpr bool check_trait() noexcept
			{
				return Trait<Derived>::value || check_trait_deep<Trait, FirstInterface, OtherInterfaces...>();
			}

			// Derived may override the following functions
			static HRESULT pre_query_interface(REFIID, void **) noexcept
			{
				return E_NOINTERFACE;
			}

			static HRESULT post_query_interface(REFIID, void **ppres) noexcept
			{
				*ppres = nullptr;
				return E_NOINTERFACE;
			}

			// instance creation helpers
			template<class>
			com_ptr<IUnknown> create_copy_impl(std::true_type) const
			{
				auto pobject = std::make_unique<value<Derived>>(*static_cast<const value<Derived> *>(this));
				return pobject.release()->GetUnknown();
			}

			template<class Interface>
			com_ptr<Interface> create_copy_impl(std::false_type) const
			{
				auto pobject = std::make_unique<value<Derived>>(*static_cast<const value<Derived> *>(this));
				return static_cast<Interface *>(pobject.release());
			}
		public:
			virtual ~object() = default;
			using DefaultInterface = FirstRealInterface;

			IUnknown *GetUnknown() noexcept
			{
				return static_cast<FirstRealInterface *>(static_cast<Derived *>(this));
			}

			virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) noexcept override
			{
				auto *pobject = static_cast<Derived *>(this);
				auto hr = pobject->pre_query_interface(riid, ppvObject);
				if (SUCCEEDED(hr) || hr != E_NOINTERFACE)
					return hr;

				if (riid == get_interface_guid(interface_wrapper<IUnknown>{}))
				{
					auto pUnk = pobject->GetUnknown();
					*ppvObject = pUnk;
					pUnk->AddRef();
					return S_OK;
				}

				auto result = this->query_children(pobject, riid);
				if (result)
				{
					// AddRef has already been called
					*ppvObject = result;
					return S_OK;
				}
				else
					return pobject->post_query_interface(riid, ppvObject);
			}

			// Instance creation
			template<class...Args>
			static object_holder<value<Derived>> create_instance(Args &&...args)
			{
				static_assert(!check_trait<has_smart_singleton_factory>(), "Objects marked as single_cached_instance (AKA smart_singleton_factory) cannot be currently created using create_instance method");
				return { std::make_unique<value<Derived>>(std::forward<Args>(args)...) };
			}

			template<class...Args>
			static com_ptr<IUnknown> create_aggregate(IUnknown *pOuterUnknown, Args &&...args)
			{
				static_assert(check_trait<has_supports_aggregation>(), "Class is missing supports_aggregation type trait to support aggregation");
				auto pobject = std::make_unique<aggvalue<Derived>>(pOuterUnknown, std::forward<Args>(args)...);
				return static_cast<IUnknown *>(pobject.release());
			}

			template<class Interface = FirstInterface>
			std::enable_if_t<std::is_convertible_v<Derived *, Interface *> || std::is_same_v<IUnknown, Interface>, com_ptr<Interface>> create_copy() const
			{
				return create_copy_impl<Interface>(std::is_same<IUnknown, Interface>{});
			}

			static HRESULT factory_create_object(const GUID &iid, void **ppv, IUnknown *pOuterUnknown = nullptr) noexcept;
		protected:
			// for derived class to call
			auto addref() noexcept
			{
				return static_cast<value<Derived> *>(this)->AddRef();
			}

			auto release() noexcept
			{
				return static_cast<value<Derived> *>(this)->Release();
			}
		};

		template<class Derived, class FirstInterface, class...OtherInterfaces>
		inline HRESULT object<Derived, FirstInterface, OtherInterfaces...>::factory_create_object(const GUID &iid, void **ppv, IUnknown *pOuterUnknown) noexcept
		{
			try
			{
				HRESULT hr;
				if (pOuterUnknown == nullptr)
				{
					if constexpr (check_trait<has_singleton_factory>())
					{
						static_assert(!has_final_release<Derived>, "Singleton classes are not compatible with final_release");
						static value_on_stack<Derived> single_value;
						hr = single_value.QueryInterface(iid, ppv);
					}
					else if constexpr (check_trait<has_smart_singleton_factory>())
					{
						static srwlock init_lock;
						static std::weak_ptr<Derived> current_instance;
						std::scoped_lock l{ init_lock };
						if (auto instance = current_instance.lock())
							return instance->QueryInterface(iid, ppv);
						else
						{
							auto object=std::make_unique<smart_singleton_value<Derived>>();
							hr = object->QueryInterface(iid, ppv);
							if (SUCCEEDED(hr))
								current_instance = object.release()->get_weak();
						}
					}
					else
					{
						auto object = std::make_unique<value<Derived>>();
						hr = object->QueryInterface(iid, ppv);
						if (SUCCEEDED(hr))
							object.release();
					}
				}
				else
				{
					if constexpr (check_trait<has_supports_aggregation>())
					{
						assert(iid == get_interface_guid(interface_wrapper<IUnknown>{}) && "Only IUnknown may be queried for an object being created aggregated");
						auto object = std::make_unique<aggvalue<Derived>>(pOuterUnknown);
						hr = object->QueryInterface(iid, ppv);
						if (SUCCEEDED(hr))
							object.release();
					}
					else
					{
						assert(false && "Class does not define 'supports_aggregation' type trait");
						return CLASS_E_NOAGGREGATION;
					}
				}
				return hr;
			}
			catch (const std::bad_alloc &)
			{
				return E_OUTOFMEMORY;
			}
			catch (const corsl::hresult_error &o)
			{
				return o.code();
			}
			catch (...)
			{
				return E_FAIL;
			}
		}

#pragma endregion

#pragma region Auto factory support
		using create_function_t = HRESULT(*)(const GUID &iid, void **ppv, IUnknown *) noexcept;
		struct _OBJMAP_ENTRY
		{
			GUID clsid;
			create_function_t create;
		};

#pragma section("BIS$__a", read)
#pragma section("BIS$__z", read)
#pragma section("BIS$__b", read)
		extern "C"
		{
			__declspec(selectany) __declspec(allocate("BIS$__a")) _OBJMAP_ENTRY* __pobjObjEntryFirst = nullptr;
			__declspec(selectany) __declspec(allocate("BIS$__z")) _OBJMAP_ENTRY* __pobjObjEntryLast = nullptr;
		}

		inline HRESULT create_object(const GUID &clsid, const GUID &iid, void **ppv, IUnknown *pOuterUnknown = nullptr) noexcept
		{
			for (auto p = &__pobjObjEntryFirst + 1; p < &__pobjObjEntryLast; ++p)
			{
				if (*p && (*p)->clsid == clsid)
					return (*p)->create(iid, ppv, pOuterUnknown);
			}
			return REGDB_E_CLASSNOTREG;
		}

		template<class Interface>
		inline HRESULT create_object(const GUID &clsid, bcom::ptr<Interface> &result, IUnknown *pOuterUnknown = nullptr) noexcept
		{
			return create_object(clsid, get_interface_guid(interface_wrapper<Interface>{}), reinterpret_cast<void **>(result.put()), pOuterUnknown);
		}

		template<class Interface>
		inline bcom::ptr<Interface> create_object(const GUID &clsid, IUnknown *pOuterUnknown = nullptr)
		{
			bcom::ptr<Interface> result;
			corsl::check_hresult(create_object(clsid, get_interface_guid(details::interface_wrapper<Interface>{}), reinterpret_cast<void **>(result.put()), pOuterUnknown));
			return result;
		}
#pragma endregion
	}

	template<class Interface>
	inline constexpr auto get_interface_guid() noexcept
	{
		return details::get_interface_guid(details::interface_wrapper<Interface>{});
	}

	using details::extends;
	using details::object;
	using details::intermediate;
	using details::aggregates;
	using details::eats_all;
	using details::also;
	using details::create_object;
	using details::delayed;
	using details::value_on_stack;
	using details::interface_wrapper;

	struct __declspec(empty_bases)singleton_factory
	{
		using singleton_factory_t = details::singleton_factory_t;
	};

	struct __declspec(empty_bases)single_cached_instance
	{
		using smart_singleton_factory_t = details::smart_singleton_factory_t;
	};

	struct __declspec(empty_bases)supports_aggregation
	{
		using supports_aggregation_t = details::supports_aggregation_t;
	};

	struct __declspec(empty_bases)increments_module_count
	{
		using increments_module_count_t = details::increments_module_count_t;
	};

	struct __declspec(empty_bases)enable_leak_detection
	{
		using enable_leak_detection_t = details::enable_leak_detection_t;
	};
}

#define BELT_CLASS_GUID(id) static constexpr auto get_guid() noexcept { constexpr auto guid = belt::com::make_guid(id); return guid; }
#define BELT_CLASS_GUID_EXISTING(id) static constexpr auto get_guid() noexcept { return id; }

#define BELT_DEFINE_CLASS(name,id) constexpr const auto name = belt::com::make_guid(id)

#define _BELT_GUID_HELPER(name, id) \
struct name; \
constexpr const auto msvc_get_guid_workaround_##name = belt::com::make_guid(id); \
inline constexpr auto get_guid(name *) noexcept { return msvc_get_guid_workaround_##name; } \
// end of macro

// The following macro keeps __declspec(uuid()) for backward compatibility
#define BELT_DEFINE_INTERFACE(name, id) \
_BELT_GUID_HELPER(name, id) \
struct __declspec(novtable) __declspec(empty_bases) __declspec(uuid(id)) name : belt::com::extends<name, IUnknown> \
// end of macro

#define BELT_DEFINE_INTERFACE_BASE(name,base,id) \
_BELT_GUID_HELPER(name, id) \
struct __declspec(novtable) __declspec(empty_bases) __declspec(uuid(id)) name : belt::com::extends<name, base> \
// end of macro

#if !defined(_M_IA64)
#pragma comment(linker, "/merge:BIS=.rdata")
#endif

#ifndef BELT_OBJ_ENTRY_PRAGMA

#if defined(_M_IX86)
#define BELT_OBJ_ENTRY_PRAGMA(class) __pragma(comment(linker, "/include:___p2objMap_" #class));
#elif defined(_M_IA64) || defined(_M_AMD64) || defined(_M_ARM) || defined(_M_ARM64)
#define BELT_OBJ_ENTRY_PRAGMA(class) __pragma(comment(linker, "/include:__p2objMap_" #class));
#else
#error Unknown Platform. define BELT_OBJ_ENTRY_PRAGMA
#endif

#endif

#define BELT_OBJ_ENTRY_AUTO(class) \
	const belt::com::details::_OBJMAP_ENTRY __objxMap_##class = {belt::com::get_interface_guid<class>(), &class::factory_create_object}; \
	extern "C" __declspec(allocate("BIS$__b")) __declspec(selectany) const belt::com::details::_OBJMAP_ENTRY* const __p2objMap_##class = &__objxMap_##class; \
	BELT_OBJ_ENTRY_PRAGMA(class) \
	// end of macro

#define BELT_OBJ_ENTRY_AUTO2(clsid, class) \
	const belt::com::details::_OBJMAP_ENTRY __objxMap_##class = {clsid, &class::factory_create_object}; \
	extern "C" __declspec(allocate("BIS$__b")) __declspec(selectany) const belt::com::details::_OBJMAP_ENTRY* const __p2objMap_##class = &__objxMap_##class; \
	BELT_OBJ_ENTRY_PRAGMA(class) \
	// end of macro

#define BELT_OBJ_ENTRY_AUTO2_NAMED(clsid, class, name) \
	const belt::com::details::_OBJMAP_ENTRY __objxMap_##class##name = {clsid, &class::factory_create_object}; \
	extern "C" __declspec(allocate("BIS$__b")) __declspec(selectany) const belt::com::details::_OBJMAP_ENTRY* const __p2objMap_##class##name = &__objxMap_##class##name; \
	BELT_OBJ_ENTRY_PRAGMA(class##name) \
	// end of macro

// ATL interoperability to support RegisterServer/UnregisterServer

#if defined(__ATLBASE_H__)
inline void WINAPI StubObjectMain(bool)
{
}

inline const struct ATL::_ATL_CATMAP_ENTRY *StubGetCats()
{
	return nullptr;
}

inline __declspec(selectany) ATL::_ATL_OBJMAP_CACHE stub_cache{};

#define BELT_OBJ_ENTRY_AUTO_ATL_COMPAT(clsid, class) \
	__declspec(selectany) ATL::_ATL_OBJMAP_CACHE __objCache__##class = { nullptr, 0 }; \
	const ATL::_ATL_OBJMAP_ENTRY_EX __objMap_##class = {&clsid, class::UpdateRegistry, nullptr, nullptr, &stub_cache, nullptr, &StubGetCats, &StubObjectMain }; \
	extern "C" __declspec(allocate("ATL$__m")) __declspec(selectany) const ATL::_ATL_OBJMAP_ENTRY_EX* const __pobjMap_##class = &__objMap_##class; \
	OBJECT_ENTRY_PRAGMA(class)

#endif
