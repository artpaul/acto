#include "message.h"

namespace acto {

namespace core {


///////////////////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
message_map_t::message_map_t()
    : m_counter(50000)
{
}
//-----------------------------------------------------------------------------
message_map_t::~message_map_t() {
    MutexLocker lock(m_cs);

    for (Types::iterator i = m_types.begin(); i != m_types.end(); ++i) {
        delete (*i).second;
    }
}

message_map_t* message_map_t::instance() {
    static message_map_t value;

    return &value;
}

msg_metaclass_t* message_map_t::find_metaclass(const TYPEID tid) {
    MutexLocker lock(m_cs);

    for (Types::iterator i = m_types.begin(); i != m_types.end(); ++i) {
        if ((*i).second->tid == tid)
            return (*i).second;
    }

    return NULL;
}

} // namespace core

} // namespace acto
