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
    MutexLocker lock(m_cs);

    for (Types::iterator i = m_types.begin(); i != m_types.end(); ++i) {
        delete (*i).second;
    }
}

message_map_t* message_map_t::instance() {
    static message_map_t value;

    return &value;
}

const msg_metaclass_t* message_map_t::find_metaclass(const TYPEID tid) const {
    MutexLocker          lock(m_cs);
    Tids::const_iterator i = std::lower_bound(m_tids.begin(), m_tids.end(), tid, tid_compare_t());

    if (i != m_tids.end()) {
        return *i;
    }

    return 0;
}

} // namespace core
} // namespace acto
