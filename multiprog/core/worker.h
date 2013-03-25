#pragma once

#include <util/intrlist.h>
#include <util/thread_pool.h>

#include <atomic>
#include <ctime>

namespace acto {
namespace core {

	struct object_t;
	struct package_t;

	/**
	 * Системный поток
	 */
	class worker_t : public intrusive_t< worker_t > {
	public:
		class worker_callback_i {
		public:
			virtual void        handle_message(package_t* const) = 0;
			virtual void        push_delete(object_t* const) = 0;
			virtual void        push_idle  (worker_t* const) = 0;
			virtual void        push_object(object_t* const) = 0;
			virtual object_t*   pop_object() = 0;
		};

	public:
		worker_t(worker_callback_i* const slots, thread_pool_t* const pool);
		~worker_t();

	public:
		// Поместить сообщение в очередь
		void assign(object_t* const obj, const clock_t slice);
		// -
		void wakeup();

	private:
		static void execute(void* param);
		///
		bool check_deleting(object_t* const obj);
		///
		/// \return true  - если есть возможность обработать следующие сообщения
		///         false - если сообщений больше нет
		bool process();

	private:
		// Флаг активности потока
		std::atomic<bool>   m_active;
		object_t*           m_object;
		// -
		clock_t             m_start;
		clock_t             m_time;
		// -
		event_t             m_event;
		event_t             m_complete;
		// -
		worker_callback_i* const    m_slots;
	};

} // namespace core
} // namespace acto
