#include "runtime.h"
#include "module.h"

#include <util/queue.h>
#include <util/stack.h>

namespace acto {
namespace core {

/** Контекс потока */
struct binding_context_t {
    // Список актеров ассоциированных
    // только с текущим потоком
    std::set< object_t* >   actors;
    // Счетчик инициализаций
    std::atomic<long>       counter;
};


/// -
extern TLS_VARIABLE object_t*   active_actor;
/// -
static TLS_VARIABLE binding_context_t* threadCtx = nullptr;


/**
 */
class runtime_t::impl {
    /// Тип множества актеров
    using Actors = std::set< object_t* >;

private:
    /// Критическая секция для доступа к полям
    std::mutex          m_cs;
    ///
    event_t             m_clean;
    /// Текущее множество актеров
    Actors              m_actors;

    main_module_t*      m_module;

private:
    //-------------------------------------------------------------------------
    void destroy_object_body(object_t* obj) {
        assert(!obj->unimpl && obj->impl);

        // TN: Эта процедура всегда должна вызываться внутри блокировки
        //     полей объекта, если ссылкой на объект могут владеть
        //     два и более потока

        m_module->destroy_object_body(obj->impl);

        obj->unimpl = true;
        delete obj->impl, obj->impl = nullptr;
        obj->unimpl = false;

        if (obj->waiters) {
            object_t::waiter_t* next = nullptr;
            object_t::waiter_t* it   = obj->waiters;

            while (it) {
                // TN: Необходимо читать значение следующего указателя
                //     заранее, так как пробуждение ждущего потока
                //     приведет к удалению текущего узла списка
                next = it->next;
                it->event->signaled();
                it   = next;
            }

            obj->waiters = nullptr;
        }
    }

public:
    impl() {
        m_clean.signaled();
    }

public:
    //-------------------------------------------------------------------------
    long acquire(object_t* const obj) {
        assert(obj != nullptr && obj->references > 0);
        // -
        return ++obj->references;
    }
    //-------------------------------------------------------------------------
    // Создать экземпляр объекта, связав его с соответсвтующей реализацией
    object_t* create_actor(base_t* const body, const int options) {
        assert(body != nullptr);

        object_t* const result = new core::object_t(body);

        result->references = 1;
        // Зарегистрировать объект в системе
        if (options & acto::aoBindToThread) {
            result->references += 1;
            result->binded      = true;

            threadCtx->actors.insert(result);
        }
        else {
            std::lock_guard<std::mutex> g(m_cs);

            result->exclusive = options & acto::aoExclusive;

            m_actors.insert(result);

            m_clean.reset();
        }
        // -
        return result;
    }
    //-------------------------------------------------------------------------
    void deconstruct_object(object_t* const obj) {
        assert(obj != 0);

        bool deleting = false;
        {
            std::lock_guard<std::recursive_mutex> g(obj->cs);
            // 1. Если объект уже находится в состоянии "осовобождаемы",
            //    то он будет неминуемо удален и более ничего делать не нежно
            if (obj->freeing)
                return;
            // 2. Перевести объект в состояние "удаляемый", что в частности
            //    ведет к невозможности послать сообщения данному объекту
            if (!obj->deleting)
                obj->deleting = true;
            // 3.
            if (!obj->scheduled && !obj->unimpl) {
                assert(!obj->has_messages());

                if (obj->impl)
                    destroy_object_body(obj);
                // -
                if (!obj->freeing && (0 == obj->references)) {
                    obj->freeing = true;
                    deleting     = true;
                }
            }
        }

        if (deleting) {
            // Удалить регистрацию объекта
            if (!obj->binded) {
                std::lock_guard<std::mutex> g(m_cs);

                m_actors.erase(obj);

                if (m_actors.size() == 0)
                    m_clean.signaled();
            }
            // -
            delete obj;
        }
    }
    //-------------------------------------------------------------------------
    void handle_message(std::unique_ptr<msg_t> msg) {
        assert(msg->target != nullptr);

        m_module->handle_message(std::move(msg));
    }
    //-----------------------------------------------------------------------------
    void join(object_t* const obj) {
        if (active_actor == obj) {
            return;
        }

        std::unique_ptr< object_t::waiter_t > node;
        event_t event;

        {
            std::lock_guard<std::recursive_mutex> g(obj->cs);

            if (obj->impl) {
                node.reset(new object_t::waiter_t());

                node->event  = &event;
                node->next   = obj->waiters;
                obj->waiters = node.get();
                event.reset();
            }
        }

        if (node.get() != nullptr) {
            event.wait();
        }
    }
    //-----------------------------------------------------------------------------
    long release(object_t* const obj) {
        assert(obj != 0);
        assert(obj->references > 0);

        const long result = --obj->references;

        if (result == 0)
            deconstruct_object(obj);

        return result;
    }
    //-----------------------------------------------------------------------------
    void register_module(main_module_t* const inst) {
        m_module = inst;
    }
    //-----------------------------------------------------------------------------
    void reset() {
        // 1. Инициировать процедуру удаления для всех оставшихся объектов
        {
            std::lock_guard<std::mutex> g(m_cs);

            Actors  temporary(m_actors);

            for (Actors::iterator i = temporary.begin(); i != temporary.end(); ++i)
                deconstruct_object(*i);
        }

        m_clean.wait();

        // 2.
        {
            event_t ev;
            ev.reset();
            m_module->shutdown(ev);
        }

        assert(m_actors.size() == 0);
    }
    //-------------------------------------------------------------------------
    // Послать сообщение указанному объекту
    void send(object_t* const sender, object_t* const target, msg_t* const msg) {
        assert(msg    != nullptr);
        assert(target != nullptr);

        msg->sender = sender;
        msg->target = target;

        acquire(target);

        if (sender)
            acquire(sender);

        //
        // Отправить пакет на обработку
        //
        m_module->send_message(msg);
    }
};


///////////////////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
runtime_t::runtime_t()
    : m_pimpl(new impl())
{
}
//-----------------------------------------------------------------------------
runtime_t::~runtime_t() {
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
long runtime_t::acquire(object_t* const obj) {
    return m_pimpl->acquire(obj);
}
//-----------------------------------------------------------------------------
object_t* runtime_t::create_actor(base_t* const body, const int options) {
    return m_pimpl->create_actor(body, options);
}
//-----------------------------------------------------------------------------
void runtime_t::create_thread_binding() {
    if (!threadCtx) {
        threadCtx = new binding_context_t();
        threadCtx->counter = 1;
    } else {
        ++threadCtx->counter;
    }
}
//-----------------------------------------------------------------------------
void runtime_t::deconstruct_object(object_t* const obj) {
    m_pimpl->deconstruct_object(obj);
}
//-----------------------------------------------------------------------------
void runtime_t::destroy_thread_binding() {
    if (threadCtx) {
        if (--threadCtx->counter == 0) {
            this->process_binded_actors(threadCtx->actors, true);

            delete threadCtx, threadCtx = 0;
        }
    }
}
//-----------------------------------------------------------------------------
void runtime_t::handle_message(std::unique_ptr<msg_t> msg) {
    m_pimpl->handle_message(std::move(msg));
}
//-----------------------------------------------------------------------------
void runtime_t::join(object_t* const obj) {
    m_pimpl->join(obj);
}
//-----------------------------------------------------------------------------
void runtime_t::process_binded_actors() {
    assert(threadCtx != nullptr);

    this->process_binded_actors(threadCtx->actors, false);
}
//-----------------------------------------------------------------------------
long runtime_t::release(object_t* const obj) {
    return m_pimpl->release(obj);
}
//-----------------------------------------------------------------------------
void runtime_t::register_module(main_module_t* const inst) {
    m_pimpl->register_module(inst);
    inst->startup(this);
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
    for (std::set<object_t*>::iterator i = actors.begin(); i != actors.end(); ++i) {
        object_t* const actor = *i;

        while (msg_t* const package = actor->select_message())
            this->handle_message(std::unique_ptr<msg_t>(package));

        if (need_delete)
            this->deconstruct_object(actor);
    }

    if (need_delete) {
        for (std::set<object_t*>::iterator i = actors.begin(); i != actors.end(); ++i)
            this->release(*i);

        actors.clear();
    }
}

} // namespace core
} // namespace acto
