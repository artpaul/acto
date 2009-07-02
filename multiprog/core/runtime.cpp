
#include <generic/stack.h>
#include <generic/queue.h>

#include <system/thread.h>

#include "alloc.h"
#include "runtime.h"

namespace acto {

namespace core {

/** Контекс потока */
struct binding_context_t {
    // Список актеров ассоциированных
    // только с текущим потоком
    std::set< object_t* >   actors;
    // Счетчик инициализаций
    atomic_t                counter;
};


/// -
extern TLS_VARIABLE object_t*   active_actor;
/// -
static TLS_VARIABLE binding_context_t* threadCtx = NULL;


/**
 */
class runtime_t::impl {
    /// Тип множества актеров
    typedef std::set< object_t* >   Actors;

    /// Максимальное кол-во возможных модулей
    static const size_t     MODULES_COUNT = 8;

private:
    // Критическая секция для доступа к полям
    mutex_t             m_cs;
    // Текущее множество актеров
    Actors              m_actors;
    /// Список зарегистрированных модулей
    module_t*           m_modules[MODULES_COUNT];

private:
    //-------------------------------------------------------------------------
    // Деструткор для пользовательских объектов (актеров)
    void destruct_actor(object_t* const actor) {
        assert(actor != 0);
        assert(actor->impl == 0);
        assert(actor->references == 0);

        // Удалить регистрацию объекта
        if (!actor->binded) {
            MutexLocker   lock(m_cs);
            // -
            m_actors.erase(actor);
        }
        // -
        delete actor;
    }

public:
    impl() {
        for (size_t i = 0; i < MODULES_COUNT; ++i)
            m_modules[i] = NULL;
    }

public:
    //-------------------------------------------------------------------------
    void acquire(object_t* const obj) {
        assert(obj != NULL && obj->references > 0);
        // -
        atomic_increment(&obj->references);
    }
    //-------------------------------------------------------------------------
    // Создать экземпляр объекта, связав его с соответсвтующей реализацией
    object_t* create_actor(actor_body_t* const body, const int options, const ui8 module) {
        assert(body != NULL);

        object_t* const result = new core::object_t(body, module);
        // -
        result->references = 1;
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
        }
        // -
        return result;
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
            }
        }
        // Объект полностью разобран и не имеет ссылок - удалить его
        if (deleting)
            push_deleted(obj);
    }
    //-----------------------------------------------------------------------------
    void destroy_object_body(object_t* obj) {
        assert(!obj->unimpl && obj->impl);

        // TN: Эта процедура всегда должна вызываться внутри блокировки 
        //     полей объекта, если ссылкой на объект могут владеть 
        //     два и более потока

        // -
        m_modules[obj->module]->destroy_object_body(obj->impl);
        // -
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
    //-------------------------------------------------------------------------
    void handle_message(package_t* const package) {
        assert(package->target != NULL);

        const ui8 id = package->target->module;

        m_modules[id]->handle_message(package);
    }
    //-----------------------------------------------------------------------------
    // -
    void join(object_t* const obj) {
        std::auto_ptr< object_t::waiter_t > node;
        event_t event;

        if (active_actor != obj) {
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
    //-------------------------------------------------------------------------
    void push_deleted(object_t* const obj) {
        this->destruct_actor(obj);
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
            push_deleted(obj);
        // -
        return result;
    }
    //-----------------------------------------------------------------------------
    void register_module(module_t* const inst, const ui8 id) {
        assert(inst != NULL && m_modules[id] == NULL);

        m_modules[id] = inst;
        // -
        inst->startup();
    }
    //-----------------------------------------------------------------------------
    void reset() {
        // 1. Инициировать процедуру удаления для всех оставшихся объектов
        {
            MutexLocker lock(m_cs);
            // -
            for (Actors::iterator i = m_actors.begin(); i != m_actors.end(); ++i)
                destroy_object(*i);
        }

        // 2.
        for (size_t i = 0; i < MODULES_COUNT; ++i) {
            if (m_modules[i] != NULL) {
                event_t ev;

                ev.reset();
                m_modules[i]->shutdown(ev);
            }
            m_modules[i] = NULL;
        }

        // -
        assert(m_actors.size() == 0);
    }
    //-------------------------------------------------------------------------
    // Послать сообщение указанному объекту
    void send(object_t* const sender, object_t* const target, msg_t* const msg) {
        assert(msg    != NULL && msg->meta != NULL);
        assert(target != NULL && m_modules[target->module] != NULL);

        //
        // Создать пакет
        //

        // 1. Создать экземпляр пакета
        package_t* const package = allocator_t::allocate< package_t >(msg, msg->meta->tid);
        // 2.
        package->sender = sender;
        package->target = target;
        // 3.
        acquire(target);
        // -
        if (sender)
            acquire(sender);

        //
        // Отправить пакет на обработку
        //
        m_modules[target->module]->send_message(package);
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
object_t* runtime_t::create_actor(actor_body_t* const body, const int options, const ui8 module) {
    return m_pimpl->create_actor(body, options, module);
}
//-----------------------------------------------------------------------------
void runtime_t::create_thread_binding() {
    if (!threadCtx) {
        threadCtx = new binding_context_t();
        threadCtx->counter = 1;
    }
    else
        atomic_increment(&threadCtx->counter);
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
void runtime_t::destroy_thread_binding() {
    if (threadCtx) {
        if (atomic_decrement(&threadCtx->counter) == 0) {
            this->process_binded_actors(threadCtx->actors, true);
            // -
            delete threadCtx, threadCtx = 0;
        }
    }
}
//-----------------------------------------------------------------------------
void runtime_t::handle_message(package_t* const package) {
    m_pimpl->handle_message(package);
}
//-----------------------------------------------------------------------------
void runtime_t::join(object_t* const obj) {
    m_pimpl->join(obj);
}
//-----------------------------------------------------------------------------
void runtime_t::process_binded_actors() {
    assert(threadCtx != NULL);

    this->process_binded_actors(threadCtx->actors, false);
}
//-----------------------------------------------------------------------------
void runtime_t::push_deleted(object_t* const obj) {
    m_pimpl->push_deleted(obj);
}
//-----------------------------------------------------------------------------
long runtime_t::release(object_t* const obj) {
    return m_pimpl->release(obj);
}
//-----------------------------------------------------------------------------
void runtime_t::register_module(module_t* const inst, const ui8 id) {
    m_pimpl->register_module(inst, id);
}
//-----------------------------------------------------------------------------
void runtime_t::send(object_t* const sender, object_t* const target, msg_t* const msg) {
    m_pimpl->send(sender, target, msg);
}
//-----------------------------------------------------------------------------
void runtime_t::shutdown() {
    this->destroy_thread_binding();
    // -
    m_pimpl->reset();
}
//-----------------------------------------------------------------------------
void runtime_t::startup() {
    this->create_thread_binding();
}

//-----------------------------------------------------------------------------
void runtime_t::process_binded_actors(std::set<object_t*>& actors, const bool need_delete) {
    if (thread_t::is_library_thread())
        // Данная функция не предназначена для внутренних потоков,
        // так как они управляются ядром библиотеки
        return;
    else {
        for (std::set<object_t*>::iterator i = actors.begin(); i != actors.end(); ++i) {
            object_t* const actor = *i;
            // -
            while (package_t* const package = actor->select_message()) {
                this->handle_message(package);
                delete package;
            }

            if (need_delete)
                this->destroy_object(actor);
        }
        // -
        if (need_delete)
            actors.clear();
    }
}

} // namespace core

} // namespace acto
