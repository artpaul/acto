
#include <generic/stack.h>
#include <generic/queue.h>

#include <system/thread.h>

#include "alloc.h"
#include "runtime.h"
#include "worker.h"

namespace acto {

namespace core {


/**
 */
class runtime_t::impl {
    // Тип множества актеров
    typedef std::set< object_t* >               Actors;
    // -
    typedef generics::queue_t< object_t >       HeaderQueue;
    // Стек заголовков объектов
    typedef generics::mpsc_stack_t< object_t >  HeaderStack;
    // Очередь вычислителей
    typedef generics::queue_t< worker_t >       WorkerQueue;
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
    static const unsigned int MAX_WORKERS = 512;

private:
/* Внутренние данные планировщика*/
    volatile bool       m_active;
    volatile bool       m_terminating;
    // Количество физических процессоров (ядер) в системе
    long                m_processors;
    // Экземпляр GC потока
    thread_t*           m_cleaner;
    // -
    HeaderStack         m_deleted;
    // Очередь объектов, которым пришли сообщения
    HeaderQueue         m_queue;
    // Экземпляр системного потока
    thread_t*           m_scheduler;
    // -
    event_t             m_evclean;
    event_t             m_event;
    event_t             m_evworker;
    // -
    event_t             m_evnoactors;
    event_t             m_evnoworkers;
    // Параметры потоков
    workers_t           m_workers;

/* Защищаемые блокировкой данные */
    // Критическая секция для доступа к полям
    mutex_t             m_cs;
    // Текущее множество актеров
    Actors              m_actors;

private:
    //-------------------------------------------------------------------------
    // Цикл выполнения планировщика
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
            generics::stack_t< object_t >   objects(m_deleted.extract());
            // Удлаить заголовки объектов
            while (object_t* const item = objects.pop())
                destruct_actor(item);
            //
            // 3.
            //
            m_evclean.wait(10 * 1000);
            m_evclean.reset();
        }
    }
    //-------------------------------------------------------------------------
    // -
    package_t* create_package(object_t* const target, msg_t* const data, const TYPEID type) {
        assert(target != 0);

        // 1. Создать экземпляр пакета
        package_t* const result = allocator_t::allocate< package_t >(data, type);
        // 2.
        result->sender = determine_sender();
        result->target = target;
        // 3.
        acquire(result->target);
        // -
        if (result->sender)
            acquire(result->sender);
        // -
        return result;
    }
    //-------------------------------------------------------------------------
    // -
    worker_t* create_worker() {
        if (atomic_increment(&m_workers.count) > 0)
            m_evnoworkers.reset();
        // -
        try {
            worker_t::Slots     slots;
            // -
            slots.deleted = fastdelegate::MakeDelegate(this, &impl::pushDelete);
            slots.handle  = worker_t::HandlePackage(&base_t::handle_message);
            slots.idle    = fastdelegate::MakeDelegate(this, &impl::pushIdle);
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
    // Деструткор для пользовательских объектов (актеров)
    void destruct_actor(object_t* const actor) {
        assert(actor != 0);
        assert(actor->impl == 0);
        assert(actor->references == 0);

        // -
        if (actor->thread != 0)
            atomic_decrement(&m_workers.reserved);

        // Удалить регистрацию объекта
        if (!actor->binded) {
            MutexLocker   lock(m_cs);
            // -
            m_actors.erase(actor);
            // -
            delete actor;
            // -
            if (m_actors.empty())
                m_evnoactors.signaled();
        }
        else
            delete actor;
    }
    //-------------------------------------------------------------------------
    // Определить отправителя сообщения
    object_t* determine_sender() {
        return threadCtx->sender;
    }
    //-------------------------------------------------------------------------
    // Цикл выполнения планировщика
    void execute(void*) {
        // -
        u_int   newWorkerTimeout = 2;
        int     lastCleanupTime  = clock();

        while (m_active) {
            while(!m_queue.empty()) {
                // Прежде чем извлекать объект из очереди, необходимо проверить,
                // что есть вычислительные ресурсы для его обработки
                worker_t*   worker = m_workers.idle.pop();
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
    // -
    void pushDelete(object_t* const obj) {
        assert(obj != 0);
        // -
        m_deleted.push(obj);
        // -
        m_evclean.signaled();
    }
    //-------------------------------------------------------------------------
    // -
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
        m_processors = NumberOfProcessors();
    }

public:
    //-------------------------------------------------------------------------
    void acquire(object_t* const obj) {
        assert(obj != 0);
        // -
        atomic_increment(&obj->references);
    }
    //-------------------------------------------------------------------------
    // Создать экземпляр объекта, связав его с соответсвтующей реализацией
    object_t* create_actor(base_t* const body, const int options = 0) {
        assert(body != 0);

        // Флаг соблюдения предусловий
        bool        preconditions = false;
        // Зарезервированный поток
        worker_t*   worker        = 0;

        // !!! aoExclusive aoBindToThread - не могут быть вместе

        // Создать для актера индивидуальный поток
        if (options & acto::aoExclusive)
            worker = create_worker();

        // 2. Проверка истинности предусловий
        preconditions = true &&
            // Поток зарзервирован
            (options & acto::aoExclusive) ? (worker != 0) : (worker == 0);

        // 3. Создание актера
        if (preconditions) {
            try {
                // -
                object_t* const result = new core::object_t(worker);
                // -
                result->impl       = body;
                result->references = 1;
                result->scheduled  = (worker != 0) ? true : false;
                // Зарегистрировать объект в системе
                if (options & acto::aoBindToThread) {
                    result->binded = true;
                    // -
                    threadCtx->actors.insert(result);
                }
                else {
                    MutexLocker  lock(m_cs);
                    // -
                    m_actors.insert(result);
                    m_evnoactors.reset();
                }
                // -
                if (worker) {
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
            }
        }
        return 0;
    }
    //-------------------------------------------------------------------------
    // Уничтожить объект
    void destroy_object(object_t* const obj) {
        assert(obj != 0);

        // -
        bool deleting = false;
        // -
        {
            MutexLocker lock(obj->cs);
            // Если объект уже удаляется, то делать более ничего не надо.
            if (!obj->deleting) {
                // Запретить более посылать сообщения объекту
                obj->deleting = true;
                // Если объект не обрабатывается, то его можно начать разбирать
                if (!obj->scheduled) {
                    if (obj->impl)
                        destroy_object_body(obj);
                    // -
                    if (!obj->freeing && (obj->references == 0)) {
                        obj->freeing = true;
                        deleting     = true;
                    }
                }
                else
                    // Если это эксклюзивный объект, то разбудить соответствующий
                    // поток, так как у эксклюзивных объектов всегда установлен флаг scheduled
                    if (obj->thread)
                        obj->thread->wakeup();
            }
        }
        // Объект полностью разобран и не имеет ссылок - удалить его
        if (deleting)
            pushDelete(obj);
    }
    //-----------------------------------------------------------------------------
    void destroy_object_body(object_t* obj) {
        assert(!obj->unimpl && obj->impl);

        // TN: Эта процедура всегда должна вызываться
        //     внутри блокировки полей объекта

        obj->unimpl = true;
        delete obj->impl, obj->impl = NULL;
        obj->unimpl = false;

        if (obj->waiters) {
            object_t::waiter_t* next = NULL;
            object_t::waiter_t* it   = obj->waiters;

            while (it) {
                // TN: Необходимо читать значение следующего указателя
                //     заранее, так как пробуждение ждущего потока
                //     приведет к удалению текущего узла списка
                next = it->next;
                it->event->signaled();
                it   = next;
            }

            obj->waiters = NULL;
        }
    }
    //-----------------------------------------------------------------------------
    // -
    void join(object_t* const obj) {
        std::auto_ptr< object_t::waiter_t > node;
        event_t event;

        if (threadCtx->sender != obj) {
            MutexLocker lock(obj->cs);

            if (obj->impl) {
                node.reset(new object_t::waiter_t());

                node->event  = &event;
                node->next   = obj->waiters;
                obj->waiters = node.get();
                event.reset();
            }
        }
        // -
        if (node.get() != NULL)
            event.wait();
    }
    //-----------------------------------------------------------------------------
    // -
    object_t* pop_object() {
        return m_queue.pop();
    }
    //-----------------------------------------------------------------------------
    // -
    void push_object(object_t* obj) {
        m_queue.push(obj);
    }
    //-----------------------------------------------------------------------------
    // -
    long release(object_t* const obj) {
        assert(obj != 0);
        assert(obj->references > 0);
        // -
        bool       deleting = false;
        const long result   = atomic_decrement(&obj->references);

        if (result == 0) {
            // TN: Если ссылок на объект более нет, то только один поток имеет
            //     доступ к объекту - тот, кто осовбодил последнюю ссылку.
            //     Поэтому блокировку можно не использовать.
            if (obj->deleting) {
                // -
                if (!obj->scheduled && !obj->freeing && !obj->unimpl) {
                    obj->freeing = true;
                    deleting     = true;
                }
            }
            else {
                // Ссылок на объект более нет и его необходимо удалить
                obj->deleting = true;
                // -
                if (!obj->scheduled) {
                    assert(obj->impl != 0);
                    // -
                    destroy_object_body(obj);
                    // -
                    obj->freeing = true;
                    deleting     = true;
                }
            }
        }
        // Окончательное удаление объекта
        if (deleting)
            pushDelete(obj);
        // -
        return result;
    }
    //-----------------------------------------------------------------------------
    // Послать сообщение указанному объекту
    void send(object_t* const target, msg_t* const msg, const TYPEID type) {
        bool undelivered = true;

        // 1. Создать пакет
        core::package_t* const package = create_package(target, msg, type);
        // 2.
        {
            // Эксклюзивный доступ
            MutexLocker lock(target->cs);

            // Если объект отмечен для удалдения,
            // то ему более нельзя посылать сообщения
            if (!target->deleting) {
                // 1. Поставить сообщение в очередь объекта
                target->enqueue(package);
                // 2. Подобрать для него необходимый поток
                if (target->thread != NULL)
                    target->thread->wakeup();
                else {
                    if (!target->binded && !target->scheduled) {
                        target->scheduled = true;
                        // 1. Добавить объект в очередь
                        m_queue.push(target);
                        // 2. Разбудить планировщик для обработки поступившего задания
                        m_event.signaled();
                    }
                }
                undelivered = false;
            }
        }
        // 3.
        if (undelivered)
            delete package;
    }
    //-----------------------------------------------------------------------------
    // Завершить выполнение
    void shutdown() {
        // 1. Инициировать процедуру удаления для всех оставшихся объектов
        {
            MutexLocker lock(m_cs);
            // -
            for (Actors::iterator i = m_actors.begin(); i != m_actors.end(); ++i)
                destroy_object(*i);
        }
        m_event.signaled();
        m_evworker.signaled();
        m_evclean.signaled();

        // 2. Дождаться, когда все объекты будут удалены
        m_evnoactors.wait();

        // 3. Дождаться, когда все потоки будут удалены
        m_terminating = true;
        // -
        m_event.signaled();
        m_evnoworkers.wait();

        // 4. Удалить системные потоки
        {
            m_active = false;
            // -
            m_event.signaled();
            m_evclean.signaled();
            // -
            m_scheduler->join();
            m_cleaner->join();
            // -
            delete m_scheduler, m_scheduler = 0;
            delete m_cleaner, m_cleaner = 0;
        }

        assert(m_workers.count == 0);
        assert(m_actors.size() == 0); 
    }
    //-----------------------------------------------------------------------------
    // Начать выполнение
    void startup() {
        assert(m_active    == false);
        assert(m_scheduler == NULL && m_cleaner == NULL);

        // 1.
        m_active      = true;
        m_terminating = false;
        // 2.
        m_evnoactors.signaled();
        m_evnoworkers.signaled();
        // 3.
        m_cleaner   = new thread_t(fastdelegate::MakeDelegate(this, &impl::cleaner), 0);
        m_scheduler = new thread_t(fastdelegate::MakeDelegate(this, &impl::execute), 0);
    }
};


///////////////////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
runtime_t::runtime_t() 
    : m_pimpl(new impl())
{
    // -
}
//-----------------------------------------------------------------------------
runtime_t::~runtime_t() {
    // must be defined for delete m_pimpl
}
//-----------------------------------------------------------------------------
runtime_t* runtime_t::instance() {
    static runtime_t value;

    return &value;
}

///////////////////////////////////////////////////////////////////////////////
//                            PUBLIC METHODS                                 //
///////////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
void runtime_t::acquire(object_t* const obj) {
    m_pimpl->acquire(obj);
}
//-----------------------------------------------------------------------------
object_t* runtime_t::create_actor(base_t* const body, const int options) {
    return m_pimpl->create_actor(body, options);
}
//-----------------------------------------------------------------------------
void runtime_t::destroy_object(object_t* const obj) {
    m_pimpl->destroy_object(obj);
}
//-----------------------------------------------------------------------------
void runtime_t::destroy_object_body(object_t* obj) {
    m_pimpl->destroy_object_body(obj);
}
//-----------------------------------------------------------------------------
void runtime_t::join(object_t* const obj) {
    m_pimpl->join(obj);
}
//-----------------------------------------------------------------------------
object_t* runtime_t::pop_object() {
    return m_pimpl->pop_object();
}
//-----------------------------------------------------------------------------
void runtime_t::push_object(object_t* obj) {
    m_pimpl->push_object(obj);
}
//-----------------------------------------------------------------------------
long runtime_t::release(object_t* const obj) {
    return m_pimpl->release(obj);
}
//-----------------------------------------------------------------------------
void runtime_t::send(object_t* const target, msg_t* const msg, const TYPEID type) {
    m_pimpl->send(target, msg, type);
}
//-----------------------------------------------------------------------------
void runtime_t::shutdown() {
    m_pimpl->shutdown();
}
//-----------------------------------------------------------------------------
void runtime_t::startup() {
    m_pimpl->startup();
}

} // namespace core

} // namespace acto
