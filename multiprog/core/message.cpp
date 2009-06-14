
#include "message.h"

namespace acto {

namespace core {


///////////////////////////////////////////////////////////////////////////////

message_map_t::message_map_t()
    : m_counter(50000)
{
}

TYPEID  message_map_t::get_typeid(const char* const type_name) {
    // -
    std::string name(type_name);

    // -
    {
        // Эксклюзивный доступ
        MutexLocker lock(m_cs);
        // Найти этот тип
        Types::const_iterator i = m_types.find(name);
        // -
        if (i != m_types.end())
            return (*i).second;
    }

    // Зарегистрировать тип
    {
        // Идентификатор типа
        const TYPEID id = atomic_increment(&m_counter);

        // Эксклюзивный доступ
        MutexLocker lock(m_cs);
        // -
        m_types[name] = id;
        // -
        return id;
    }
}

} // namespace core

} // namespace acto
