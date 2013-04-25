#include "actor.h"

namespace acto {

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

void base_t::handle_message(const std::unique_ptr<core::package_t>& p) {
    for (Handlers::const_iterator hi = m_handlers.begin(); hi != m_handlers.end(); ++hi) {
        if (hi->type == p->type) {
            hi->handler->invoke(p->sender, p->data.get());
            return;
        }
    }

}

void base_t::set_handler(i_handler* const handler, const TYPEID type) {
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

} // namespace acto
