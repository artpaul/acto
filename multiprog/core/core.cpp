
#include <system/platform.h>
#include <system/thread.h>

#include "core.h"
#include "module.h"
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
i_handler::i_handler(const TYPEID type_)
    : m_type(type_)
{
}


///////////////////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
object_t::object_t(worker_t* const thread_, const ui8 module_)
    : impl      (NULL)
    , thread    (thread_)
    , waiters   (NULL)
    , references(0)
    , module    (module_)
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
        runtime_t::instance()->register_module(main_module_t::instance(), 0);
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
        runtime_t::instance()->handle_message(package);
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
