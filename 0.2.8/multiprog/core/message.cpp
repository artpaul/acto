
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

} // namespace core

} // namespace acto
