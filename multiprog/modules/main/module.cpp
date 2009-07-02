
#include <core/runtime.h>

#include "module.h"
#include "worker.h"

namespace acto {

namespace core {

/// Объект, от имени которого посылается сообщение
TLS_VARIABLE object_t* active_actor;

static void do_handle_message(package_t* const package);


/**
 */
class main_module_t::impl {
    // -
    typedef generics::queue_t< object_t >       HeaderQueue;
    // -
    typedef generics::mpsc_stack_t< worker_t >  WorkerStack;

    //
    struct workers_t {
        // Текущее кол-во потоков
        atomic_t        count;
        // Текущее кол-во удаляемых потоков
        atomic_t        deleting;
        // Текущее кол-во эксклюзивных потоков
        atomic_t        reserved;
        // Удаленные потоки
        WorkerStack     deleted;
        // Свободные потоки
        WorkerStack     idle;
    };

    // Максимальное кол-во рабочих потоков в системе
    static const size_t MAX_WORKERS = 512;

private:
    // Экземпляр GC потока
    thread_t*           m_cleaner;
    // -
    event_t             m_evclean;
    event_t             m_event;
    event_t             m_evworker;
    event_t             m_evnoworkers;
    /// Количество физических процессоров (ядер) в системе
    long                m_processors;
    /// Очередь объектов, которым пришли сообщения
    HeaderQueue         m_queue;
    /// Экземпляр системного потока
    thread_t*           m_scheduler;
    /// -
    workers_t           m_workers;
    /// -
    volatile bool       m_active;
    volatile bool       m_terminating;

private:
    void cleaner(void*) {
        while (m_active) {
            //
            // 1. Извлечение потоков, отмеченых для удаления
            //
            generics::stack_t< worker_t >   queue(m_workers.deleted.extract());
            // Удалить все потоки
            while (worker_t* const item = queue.pop()) {
                delete item;
                // Уведомить, что удалены все вычислительные потоки
                if (atomic_decrement(&m_workers.count) == 0)
                    m_evnoworkers.signaled();
            }
            //
            // 2.
            //
            m_evclean.wait(10 * 1000);
            m_evclean.reset();
        }
    }
    //-------------------------------------------------------------------------
    worker_t* create_worker() {
        if (atomic_increment(&m_workers.count) == 1)
            m_evnoworkers.reset();
        // -
        try {
            worker_t::Slots     slots;
            // -
            slots.deleted = fastdelegate::MakeDelegate(this, &impl::pushDelete);
            slots.idle    = fastdelegate::MakeDelegate(this, &impl::pushIdle);
            slots.push    = fastdelegate::MakeDelegate(this, &impl::push_object);
            slots.pop     = fastdelegate::MakeDelegate(this, &impl::pop_object);
            slots.handle.bind(&do_handle_message);
            // -
            return new core::worker_t(slots, thread_pool_t::instance());
        }
        catch (...) {
            if (atomic_decrement(&m_workers.count) == 0)
                m_evnoworkers.signaled();
            return 0;
        }
    }
    //-------------------------------------------------------------------------
    void execute(void*) {
        int newWorkerTimeout = 2;
        int lastCleanupTime  = clock();

        while (m_active) {
            while(!m_queue.empty()) {
                // Прежде чем извлекать объект из очереди, необходимо проверить,
                // что есть вычислительные ресурсы для его обработки
                worker_t* worker = m_workers.idle.pop();
                // -
                if (!worker) {
                    // Если текущее количество потоков меньше оптимального,
                    // то создать новый поток
                    if (m_workers.count < (m_workers.reserved + (m_processors << 1)))
                        worker = create_worker();
                    else {
                        // Подождать некоторое время осовобождения какого-нибудь потока
                        m_evworker.reset();
                        // -
                        const WaitResult result = m_evworker.wait(newWorkerTimeout * 1000);
                        // -
                        worker = m_workers.idle.pop();
                        // -
                        if (!worker && (result == WR_TIMEOUT)) {
                            // -
                            newWorkerTimeout += 2;
                            // -
                            if (m_workers.count < MAX_WORKERS)
                                worker = create_worker();
                            else {
                                m_evworker.reset();
                                m_evworker.wait();
                            }
                        }
                        else
                            if (newWorkerTimeout > 2)
                                newWorkerTimeout -= 2;
                    }
                }
                // -
                if (worker) {
                    if (object_t* const obj = m_queue.pop()) {
                        worker->assign(obj, (CLOCKS_PER_SEC >> 2));
                    }
                    else
                        m_workers.idle.push(worker);
                }

                yield();
            }

            // -
            if (m_terminating || (m_event.wait(10 * 1000) == WR_TIMEOUT)) {
                thread_pool_t::instance()->collect_all();
                m_workers.deleted.push(m_workers.idle.extract());
                m_evclean.signaled();
                // -
                yield();
            }
            else {
                if ((clock() - lastCleanupTime) > (10 * CLOCKS_PER_SEC)) {
                    thread_pool_t::instance()->collect_one();
                    if (worker_t* const item = m_workers.idle.pop()) {
                        m_workers.deleted.push(item);
                        m_evclean.signaled();
                    }
                    lastCleanupTime = clock();
                }
            }
            // -
            m_event.reset();
        }
    }
    //-------------------------------------------------------------------------
    void pushDelete(object_t* const obj) {
        runtime_t::instance()->push_deleted(obj);
    }
    //-------------------------------------------------------------------------
    void pushIdle(worker_t* const worker) {
        assert(worker != 0);
        // -
        m_workers.idle.push(worker);
        // -
        m_evworker.signaled();
        m_event.signaled();
    }

public:
    impl()
        : m_active     (false)
        , m_processors (1)
        , m_cleaner    (NULL)
        , m_scheduler  (NULL)
        , m_terminating(false)
    {
        // 1.
        m_active      = true;
        m_processors  = NumberOfProcessors();
        m_terminating = false;

        m_workers.count    = 0;
        m_workers.deleting = 0;
        m_workers.reserved = 0;

        // 2.
        m_evnoworkers.signaled();
        // 3.
        m_cleaner   = new thread_t(fastdelegate::MakeDelegate(this, &impl::cleaner), 0);
        m_scheduler = new thread_t(fastdelegate::MakeDelegate(this, &impl::execute), 0);
    }

    ~impl() {
        m_event.signaled();
        m_evworker.signaled();
        m_evclean.signaled();

        // Дождаться, когда все потоки будут удалены
        m_terminating = true;
        // -
        m_event.signaled();
        m_evnoworkers.wait();

        m_active = false;
        // -
        m_event.signaled();
        m_evclean.signaled();
        // -
        m_scheduler->join();
        m_cleaner->join();
        // -
        delete m_scheduler;
        delete m_cleaner;

        assert(m_workers.count == 0 && m_workers.deleting == 0 && m_workers.reserved == 0);
    }

    object_t* create_actor(base_t* const body, const int options) {
        assert(body != NULL);

        // Флаг соблюдения предусловий
        bool        preconditions = false;
        // Зарезервированный поток
        worker_t*   worker        = NULL;

        // !!! aoExclusive aoBindToThread - не могут быть вместе

        // Создать для актера индивидуальный поток
        if (options & acto::aoExclusive)
            worker = this->create_worker();

        // 2. Проверка истинности предусловий
        preconditions = true &&
            // Поток зарзервирован
            (options & acto::aoExclusive) ? (worker != 0) : (worker == 0);

        // 3. Создание актера
        if (preconditions) {
            try {
                object_t* const result = runtime_t::instance()->create_actor(body, options, 0);
               
                if (worker) {
                    result->scheduled  = true;
                    // -
                    body->m_thread     = worker;
                    // -
                    atomic_increment(&m_workers.reserved);
                    // -
                    worker->assign(result, 0);
                }
                // -
                return result;
            }
            catch (...) {
                // Поставить в очередь на удаление
                if (worker != 0)
                    m_workers.deleted.push(worker);
                delete body;
            }
        }
        return NULL;
    }

    void destroy_object_body(actor_body_t* const body) {
        assert(body != NULL);

        base_t* const impl = static_cast<base_t*>(body);

        if (impl->m_thread != NULL)
            atomic_decrement(&m_workers.reserved);
    }

    object_t* pop_object() {
        return m_queue.pop();
    }

    void push_object(object_t* obj) {
        m_queue.push(obj);

        m_event.signaled();
    }
};


//-----------------------------------------------------------------------------
void do_handle_message(package_t* const package) {
    assert(package != NULL && package->target != NULL);

    object_t* const obj = package->target;
    i_handler* handler  = 0;
    base_t* const impl  = static_cast<base_t*>(obj->impl);

    assert(obj->module == 0);

    // 1. Найти обработчик соответствующий данному сообщению
    for (base_t::Handlers::iterator i = impl->m_handlers.begin(); i != impl->m_handlers.end(); ++i) {
        if (package->type == (*i)->type) {
            handler = (*i)->handler;
            break;
        }
    }
    // 2. Если соответствующий обработчик найден, то вызвать его
    try {
        if (handler) {
            assert(active_actor == NULL);
            // TN: Данный параметр читает только функция determine_sender,
            //     которая всегда выполняется в контексте этого потока.
            active_actor = obj;
            // -
            handler->invoke(package->sender, package->data);
            // -
            active_actor = NULL;

            if (impl->m_terminating)
                runtime_t::instance()->destroy_object(obj);
        }
    }
    catch (...) {
        active_actor = NULL;
    }
}


///////////////////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
base_t::base_t()
    : m_thread(NULL)
    , m_terminating(false)
{
    // -
}
//-----------------------------------------------------------------------------
base_t::~base_t() {
    if (m_thread != NULL)
        m_thread->wakeup();

    for (Handlers::iterator i = m_handlers.begin(); i != m_handlers.end(); i++) {
        // Удалить обработчик
        if ((*i)->handler)
            delete (*i)->handler;
        // Удалить элемент списка
        delete (*i);
    }
}
//-----------------------------------------------------------------------------
void base_t::terminate() {
    this->m_terminating = true;
}
//-----------------------------------------------------------------------------
void base_t::set_handler(i_handler* const handler, const TYPEID type) {
    for (Handlers::iterator i = m_handlers.begin(); i != m_handlers.end(); ++i) {
        if ((*i)->type == type) {
            // 1. Удалить предыдущий обработчик
            if ((*i)->handler)
                delete (*i)->handler;
            // 2. Установить новый
            (*i)->handler = handler;
            // -
            return;
        }
    }
    // Запись для данного типа сообщения еще не существует
    {
        HandlerItem* const item = new HandlerItem(type);
        // -
        item->handler = handler;
        // -
        m_handlers.push_back(item);
    }
}



///////////////////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
main_module_t::main_module_t()
    : m_pimpl(NULL)
{

}
//-----------------------------------------------------------------------------
main_module_t::~main_module_t() {
    // -
}
//-----------------------------------------------------------------------------
object_t* main_module_t::determine_sender() {
    return active_actor;
}
//-----------------------------------------------------------------------------
void main_module_t::destroy_object_body(actor_body_t* const body) {
    m_pimpl->destroy_object_body(body);
}
//-----------------------------------------------------------------------------
void main_module_t::handle_message(package_t* const package) {
    do_handle_message(package);
}
//-----------------------------------------------------------------------------
void main_module_t::send_message(package_t* const package) {
    object_t* const target      = package->target;
    bool            undelivered = true;

    // Эксклюзивный доступ
    MutexLocker lock(target->cs);

    // Если объект отмечен для удалдения,
    // то ему более нельзя посылать сообщения
    if (!target->deleting && target->impl) {
        // 1. Поставить сообщение в очередь объекта
        target->enqueue(package);
        // 2. Подобрать для него необходимый поток
        if (static_cast<base_t*>(target->impl)->m_thread != NULL)
            static_cast<base_t*>(target->impl)->m_thread->wakeup();
        else {
            if (!target->binded && !target->scheduled) {
                target->scheduled = true;
                // Добавить объект в очередь
                m_pimpl->push_object(target);
            }
        }
        undelivered = false;
    }

    if (undelivered)
        delete package;
}
//-----------------------------------------------------------------------------
void main_module_t::shutdown(event_t& event) {
    m_pimpl.reset(NULL);
    event.signaled();
}
//-----------------------------------------------------------------------------
void main_module_t::startup() {
    m_pimpl.reset(new impl());
}

//-----------------------------------------------------------------------------
object_t* main_module_t::create_actor(base_t* const body, const int options) {
    return m_pimpl->create_actor(body, options);
}

} // namespace core

} // namespace acto
