#pragma once

#include "noncopyable.h"

namespace acto {
	struct delete_t {
		template <typename T>
		static inline void destroy(T* ptr) throw () {
			delete ptr;
		}
	};

	template <typename Base, class T>
	class pointer_common_t {
	public:
		inline T* operator -> () const throw () {
			return as_t();
		}

	protected:
		inline T* as_t() const throw () {
			return (static_cast<const Base*>(this))->get();
		}

		static inline T* do_release(T*& t) throw () {
			T* ret = t; t = 0; return ret;
		}
	};

	template <typename Base, typename T>
	class pointer_base_t : public pointer_common_t<Base, T> {
	public:
		inline T& operator * () const throw () {
			return *(this->as_t());
		}
	};

	/*
	* void*-like pointers does not have operator*
	*/
	template <typename Base>
	class pointer_base_t<Base, void> : public pointer_common_t<Base, void> {
	};


	template <typename T, typename D = delete_t>
	class holder_t
		: public pointer_base_t<holder_t<T, D>, T>
		, public non_copyable_t 
	{
	public:
		inline holder_t() throw () 
			: m_ptr(0)
		{
		}

		inline holder_t(T* t) throw () 
			: m_ptr(t)
		{
		}

		inline ~holder_t() throw () {
			do_destroy();
		}

		inline void destroy() throw () {
			reset(0);
		}

		inline T* release() throw () {
            return this->do_release(m_ptr);
        }

		inline void reset(T* const t) throw () {
			if (t != m_ptr) {
				do_destroy(); m_ptr = t;
			}
		}

		inline T* get() const throw () {
			return m_ptr;
		}

		inline bool valid() const throw () {
			return m_ptr != 0;
		}

	private:
		inline void do_destroy() throw () {
			if (m_ptr) {
				D::destroy(m_ptr);
			}
		}

	private:
		T*	m_ptr;
	};

} // namespace acto