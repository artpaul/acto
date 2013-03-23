#include "actor.h"

namespace acto {

///////////////////////////////////////////////////////////////////////////////

i_handler::i_handler(const TYPEID type_)
    : m_type(type_)
{
}

///////////////////////////////////////////////////////////////////////////////

base_t::base_t()
    : m_thread(NULL)
    , m_terminating(false)
{
}

base_t::~base_t() {
    for (Handlers::iterator i = m_handlers.begin(); i != m_handlers.end(); i++) {
        delete (*i);
    }
}

void base_t::die() {
    this->m_terminating = true;
}

void base_t::set_handler(i_handler* const handler, const TYPEID type) {
    for (Handlers::iterator i = m_handlers.begin(); i != m_handlers.end(); ++i) {
        if ((*i)->type == type) {
            (*i)->handler.reset(handler);
            return;
        }
    }

    // Запись для данного типа сообщения еще не существует
    m_handlers.push_back(new HandlerItem(type, handler));
}

///////////////////////////////////////////////////////////////////////////////

} // namespace acto
