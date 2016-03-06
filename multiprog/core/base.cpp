#include "base.h"
#include "module.h"
#include "runtime.h"

namespace acto {
namespace core {

///////////////////////////////////////////////////////////////////////////////

object_t::object_t(base_t* const impl_)
    : impl      (impl_)
    , waiters   (NULL)
    , references(0)
    , binded    (false)
    , deleting  (false)
    , exclusive (false)
    , freeing   (false)
    , scheduled (false)
    , unimpl    (false)
{
    next = NULL;
}

void object_t::enqueue(package_t* const msg) {
    input_stack.push(msg);
}

bool object_t::has_messages() const {
    return !local_stack.empty() || !input_stack.empty();
}

package_t* object_t::select_message() {
    if (package_t* const p = local_stack.pop()) {
        return p;
    } else {
        local_stack.push(input_stack.extract());

        return local_stack.pop();
    }
}

///////////////////////////////////////////////////////////////////////////////

package_t::package_t(const msg_t* const data_, const std::type_index& type_)
    : data  (data_)
    , sender(NULL)
    , type  (type_)
{
}

package_t::~package_t() {
    // Освободить ссылки на объекты
    if (sender) {
        runtime_t::instance()->release(sender);
    }
    runtime_t::instance()->release(target);
}

///////////////////////////////////////////////////////////////////////////////

base_t::base_t()
    : m_thread(nullptr)
    , m_terminating(false)
{
}

base_t::~base_t()
{ }

void base_t::die() {
    this->m_terminating = true;
}

void base_t::consume_package(const std::unique_ptr<package_t>& p) {
    for (Handlers::const_iterator hi = m_handlers.begin(); hi != m_handlers.end(); ++hi) {
        if (hi->type == p->type) {
            hi->handler->invoke(p->sender, p->data.get());
            return;
        }
    }
}

void base_t::set_handler(handler_t* const handler, const std::type_index& type) {
    for (Handlers::iterator hi = m_handlers.begin(); hi != m_handlers.end(); ++hi) {
        if (hi->type == type) {
            hi->handler.reset(handler);
            return;
        }
    }

    // Запись для данного типа сообщения еще не существует
    m_handlers.push_back(HandlerItem(type, handler));
}

///////////////////////////////////////////////////////////////////////////////

} // namespace core
} // namespace acto
