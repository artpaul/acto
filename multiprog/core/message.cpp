#include "message.h"
#include <algorithm>

namespace acto {
namespace core {

///////////////////////////////////////////////////////////////////////////////

message_map_t::message_map_t()
    : m_counter(50000)
{
}

message_map_t::~message_map_t() {
    std::lock_guard<std::mutex> g(m_cs);

    for (Types::iterator i = m_types.begin(); i != m_types.end(); ++i) {
        delete (*i).second;
    }
}

message_map_t* message_map_t::instance() {
    static message_map_t value;

    return &value;
}

const msg_metaclass_t* message_map_t::find_metaclass(const TYPEID tid) const {
    std::lock_guard<std::mutex> g(m_cs);

    for (Types::const_iterator ti = m_types.begin(); ti != m_types.end(); ++ti) {
        if (ti->second->tid == tid) {
            return ti->second;
        }
    }

    return nullptr;
}

} // namespace core
} // namespace acto
