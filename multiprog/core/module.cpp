#include "module.h"
#include "worker.h"

#include <core/runtime.h>
#include <atomic>
#include <thread>

namespace acto {
namespace core {

/// Объект, от имени которого посылается сообщение
TLS_VARIABLE object_t* active_actor;

void do_handle_message(package_t* const package);


/**
 */
class main_module_t::impl : public worker_t::worker_callback_i {
    // -
    typedef generics::queue_t< object_t >       HeaderQueue;
    // -
    typedef generics::mpsc_stack_t< worker_t >  WorkerStack;

    //
    struct workers_t {
        // Текущее кол-во потоков
        std::atomic<long>   count;
        // Текущее кол-во эксклюзивных потоков
        std::atomic<long>   reserved;
        // Свободные потоки
        WorkerStack         idle;
    };

    // Максимальное кол-во рабочих потоков в системе
    static const size_t MAX_WORKERS = 512;

private:
    ///
    event_t             m_event;
    event_t             m_evworker;
    event_t             m_evnoworkers;
    /// Количество физических процессоров (ядер) в системе
    long                m_processors;
    /// Очередь объектов, которым пришли сообщения
    HeaderQueue         m_queue;
    /// Экземпляр системного потока
    std::unique_ptr<std::thread>
                        m_scheduler;
    /// -
    workers_t           m_workers;
    /// -
    std::atomic<bool>   m_active;
    std::atomic<bool>   m_terminating;

private:
    static void execute(impl* const pthis) {
        int newWorkerTimeout = 2;
        int lastCleanupTime  = clock();

        while (pthis->m_active) {
            while(!pthis->m_queue.empty()) {
                pthis->dispatch_to_worker(newWorkerTimeout);
                yield();
            }

            if (pthis->m_terminating || (pthis->m_event.wait(60 * 1000) == WR_TIMEOUT)) {
                generics::stack_t< worker_t >   queue(pthis->m_workers.idle.extract());
                // Удалить все потоки
                while (worker_t* const item = queue.pop())
                    pthis->delete_worker(item);

                yield();
            }
            else {
                if ((clock() - lastCleanupTime) > (60 * CLOCKS_PER_SEC)) {
                    if (worker_t* const item = pthis->m_workers.idle.pop())
                        pthis->delete_worker(item);

                    lastCleanupTime = clock();
                }
            }
        }
    }

    void delete_worker(worker_t* const item) {
        delete item;
        // Уведомить, что удалены все вычислительные потоки
        if (--m_workers.count == 0) {
            m_evnoworkers.signaled();
        }
    }

    worker_t* create_worker() {
        worker_t* const result = new core::worker_t(this, thread_pool_t::instance());

        if (++m_workers.count == 1) {
            m_evnoworkers.reset();
        }

        return result;
    }

    void dispatch_to_worker(int& wait_timeout) {
        // Прежде чем извлекать объект из очереди, необходимо проверить,
        // что есть вычислительные ресурсы для его обработки
        worker_t* worker = m_workers.idle.pop();

        if (!worker) {
            // Если текущее количество потоков меньше оптимального,
            // то создать новый поток
            if (m_workers.count < (m_workers.reserved + (m_processors << 1))) {
                worker = create_worker();
            } else {
                // Подождать некоторое время осовобождения какого-нибудь потока
                const wait_result result = m_evworker.wait(wait_timeout * 1000);

                worker = m_workers.idle.pop();

                if (!worker && (result == WR_TIMEOUT)) {
                    wait_timeout += 2;

                    if (m_workers.count < MAX_WORKERS) {
                        worker = create_worker();
                    } else {
                        m_evworker.wait();
                    }
                } else {
                    if (wait_timeout > 2) {
                        wait_timeout -= 2;
                    }
                }
            }
        }

        if (worker) {
            if (object_t* const obj = m_queue.pop())
                worker->assign(obj, (CLOCKS_PER_SEC >> 2));
            else
                m_workers.idle.push(worker);
        }
    }

public:
    impl()
        : m_event(true)
        , m_evworker(true)
        , m_processors ( NumberOfProcessors() )
        , m_active     (true)
        , m_terminating(false)
    {
        // 1.
        m_workers.count    = 0;
        m_workers.reserved = 0;
        // 2.
        m_evnoworkers.signaled();
        // 3.
        m_scheduler.reset(new std::thread(&impl::execute, this));
    }

    ~impl() {
        // Дождаться, когда все потоки будут удалены
        m_terminating = true;

        m_event.signaled();
        m_evnoworkers.wait();

        m_active = false;

        m_event.signaled();

        if (m_scheduler->joinable()) {
            m_scheduler->join();
        }

        assert(m_workers.count == 0 && m_workers.reserved == 0);
    }

public:
    object_t* create_actor(base_t* const body, const int options) {
        assert(body != NULL);

        object_t* const result = runtime_t::instance()->create_actor(body, options, 0);

        // Создать для актера индивидуальный поток
        if (options & acto::aoExclusive) {
            worker_t* const worker = this->create_worker();

            result->scheduled  = true;

            body->m_thread     = worker;

            ++m_workers.reserved;

            worker->assign(result, 0);
        }

        return result;
    }

    void destroy_object_body(actor_body_t* const body) {
        assert(body != NULL);

        base_t* const impl = static_cast<base_t*>(body);

        if (impl->m_thread != NULL) {
            --m_workers.reserved;

            impl->m_thread->wakeup();
        }
    }

    void handle_message(package_t* const package) {
        do_handle_message(package);
    }

    void push_delete(object_t* const obj) {
        runtime_t::instance()->deconstruct_object(obj);
    }

    void push_idle(worker_t* const worker) {
        assert(worker != 0);

        m_workers.idle.push(worker);

        m_evworker.signaled();
    }

    object_t* pop_object() {
        return m_queue.pop();
    }

    void push_object(object_t* const obj) {
        m_queue.push(obj);

        m_event.signaled();
    }
};


//-----------------------------------------------------------------------------
void do_handle_message(package_t* const p) {
    assert(p != NULL && p->target != NULL);

    const std::unique_ptr<package_t>
                        package(p);
    object_t* const     obj     = package->target;
    base_t* const       impl    = static_cast< base_t* >(obj->impl);
    i_handler*          handler = 0;

    assert(obj->module == 0);
    assert(obj->impl   != 0);

    // 1. Найти обработчик соответствующий данному сообщению
    for (base_t::Handlers::iterator i = impl->m_handlers.begin(); i != impl->m_handlers.end(); ++i) {
        if (package->type == (*i)->type) {
            handler = (*i)->handler.get();
            break;
        }
    }
    // 2. Если соответствующий обработчик найден, то вызвать его
    if (handler) {
        try {
            assert(active_actor == NULL);
            // TN: Данный параметр читает только функция determine_sender,
            //     которая всегда выполняется в контексте этого потока.
            active_actor = obj;

            handler->invoke(package->sender, package->data.get());

            active_actor = NULL;
        } catch (...) {
            active_actor = NULL;
        }

        if (impl->m_terminating) {
            runtime_t::instance()->deconstruct_object(obj);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

main_module_t::main_module_t()
    : m_pimpl(nullptr)
{

}

main_module_t::~main_module_t()
{ }

object_t* main_module_t::determine_sender() {
    return active_actor;
}

void main_module_t::destroy_object_body(actor_body_t* const body) {
    m_pimpl->destroy_object_body(body);
}

void main_module_t::handle_message(package_t* const package) {
    m_pimpl->handle_message(package);
}

void main_module_t::send_message(package_t* const p) {
    std::unique_ptr<package_t>
                    package(p);
    object_t* const target = package->target;
    bool            should_push = false;

    {
        // Эксклюзивный доступ
        std::lock_guard<std::recursive_mutex> g(target->cs);

        // Если объект отмечен для удалдения,
        // то ему более нельзя посылать сообщения
        if (!target->deleting) {
            assert(target->impl && target->module == 0);
            // 1. Поставить сообщение в очередь объекта
            target->enqueue(package.release());
            // 2. Подобрать для него необходимый поток
            if (!target->binded) {
                // Если с объектом связан эксклюзивный поток
                worker_t* const thread = static_cast< base_t* >(target->impl)->m_thread;

                if (thread != NULL) {
                    thread->wakeup();
                } else {
                    if (!target->scheduled) {
                        target->scheduled = true;
                        // Добавить объект в очередь
                        should_push = true;
                    }
                }
            }
        }
    }

    // Добавить объект в очередь
    if (should_push) {
        m_pimpl->push_object(target);
    }
}

void main_module_t::shutdown(event_t& event) {
    m_pimpl.reset(NULL);
    event.signaled();
}

void main_module_t::startup() {
    m_pimpl.reset(new impl());
}

object_t* main_module_t::create_actor(base_t* const body, const int options) {
    return m_pimpl->create_actor(body, options);
}

} // namespace core
} // namespace acto
