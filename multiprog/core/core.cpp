
#include <system/platform.h>
#include <system/thread.h>

#include "core.h"
#include "worker.h"
#include "thread_pool.h"

namespace acto {

namespace core {

using fastdelegate::FastDelegate;
using fastdelegate::MakeDelegate;

//
TLS_VARIABLE thread_context_t* threadCtx = NULL;


///////////////////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
base_t::base_t()
    : m_terminating(false)
{
    // -
}
//-----------------------------------------------------------------------------
base_t::~base_t() {
    for (Handlers::iterator i = m_handlers.begin(); i != m_handlers.end(); i++) {
        // Удалить обработчик
        if ((*i)->handler)
            delete (*i)->handler;
        // Удалить элемент списка
        delete (*i);
    }
}
//-----------------------------------------------------------------------------
void base_t::handle_message(package_t* const package) {
    object_t* const obj = package->target;
    i_handler* handler  = 0;
    base_t* const impl  = obj->impl;

    // 1. Найти обработчик соответствующий данному сообщению
    for (Handlers::iterator i = impl->m_handlers.begin(); i != impl->m_handlers.end(); ++i) {
        if (package->type == (*i)->type) {
            handler = (*i)->handler;
            break;
        }
    }
    // 2. Если соответствующий обработчик найден, то вызвать его
    try {
        if (handler) {
            // TN: Данный параметр читает только функция determine_sender,
            //     которая всегда выполняется в контексте этого потока.
            threadCtx->sender = obj;
            // -
            handler->invoke(package->sender, package->data);
            // -
            threadCtx->sender = 0;

            if (impl->m_terminating)
                runtime_t::instance()->destroy_object(obj);
        }
    }
    catch (...) {
        // -
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
i_handler::i_handler(const TYPEID type_)
    : m_type(type_)
{
}


///////////////////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
object_t::object_t(worker_t* const thread_)
    : impl      (NULL)
    , thread    (thread_)
    , waiters   (NULL)
    , references(0)
    , binded    (false)
    , deleting  (false)
    , freeing   (false)
    , scheduled (false)
    , unimpl    (false)
{
    next = NULL;
}
//-----------------------------------------------------------------------------
void object_t::enqueue(package_t* const msg) {
    input_stack.push(msg);
}
//-----------------------------------------------------------------------------
bool object_t::has_messages() const {
    return !local_stack.empty() || !input_stack.empty();
}
//-----------------------------------------------------------------------------
package_t* object_t::select_message() {
    if (package_t* const p = local_stack.pop()) {
        return p;
    }
    else {
        local_stack.push(input_stack.extract());
        // -
        return local_stack.pop();
    }
}


///////////////////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
package_t::package_t(msg_t* const data_, const TYPEID type_) :
    data  (data_),
    sender(0),
    type  (type_)
{
}
//-----------------------------------------------------------------------------
package_t::~package_t() {
    // Освободить ссылки на объекты
    if (sender)
        runtime_t::instance()->release(sender);
    // -
    runtime_t::instance()->release(target);
    // Удалить данные сообщения
    delete data;
}


///////////////////////////////////////////////////////////////////////////////
//                       ИНТЕРФЕЙСНЫЕ МЕТОДЫ ЯДРА                            //
///////////////////////////////////////////////////////////////////////////////

static atomic_t counter = 0;

//-----------------------------------------------------------------------------
// Desc: Инициализация библиотеки.
//-----------------------------------------------------------------------------
void initialize() {
    if (atomic_increment(&counter) > 0) {
        core::initializeThread(false);
        runtime_t::instance()->startup();
    }
}

void initializeThread(const bool isInternal) {
    // -
    if (!threadCtx) {
        threadCtx = new thread_context_t();
        threadCtx->counter = 1;
        threadCtx->sender  = 0;
        threadCtx->is_core = isInternal;
    }
    else
        threadCtx->counter++;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void finalize() {
    if (counter > 0) {
        if (atomic_decrement(&counter) == 0) {
            finalizeThread();
            runtime_t::instance()->shutdown();
        }
    }
}

static void processActorMessages(object_t* const actor) {
    while (package_t* const package = actor->select_message()) {
        base_t::handle_message(package);
        delete package;
    }
}

void finalizeThread() {
    if (threadCtx) {
        if (--threadCtx->counter == 0) {
            std::set<object_t*>::iterator   i;
            // -
            for (i = threadCtx->actors.begin(); i != threadCtx->actors.end(); ++i) {
                processActorMessages((*i));

                runtime_t::instance()->destroy_object(*i);
            }
            // -
            delete threadCtx, threadCtx = 0;
        }
    }
}

void processBindedActors() {
    if (thread_t::is_core_thread())
        // Данная функция не предназначена для внутренних потоков,
        // так как они управляются ядром библиотеки
        return;
    else {
        if (threadCtx) {
            std::set<object_t*>::iterator   i;
            // -
            for (i = threadCtx->actors.begin(); i != threadCtx->actors.end(); ++i)
                processActorMessages((*i));
        }
    }
}

}; // namespace core

}; // namespace acto
