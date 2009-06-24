
#include <core/core.h>
#include "act_user.h"
#include <extension/services.h>

namespace acto {

static atomic_t startup_counter = 0;

//-----------------------------------------------------------------------------
ACTO_API void destroy(object_t& object) {
    if (core::object_t* const obj = (core::object_t*)atomic_swap((atomic_t*)&object.m_object, NULL)) {
        // Освободить ссылку на объект и удалить его
        if (core::runtime_t::instance()->release(obj) > 0)
            core::runtime_t::instance()->destroy_object(obj);
    }
}
//-----------------------------------------------------------------------------
ACTO_API void finalize_thread() {
    core::finalizeThread();
}
//-----------------------------------------------------------------------------
ACTO_API void initialize_thread() {
    core::initializeThread(false);
}
//-----------------------------------------------------------------------------
ACTO_API void join(actor_t& obj) {
    if (obj.m_object)
        core::runtime_t::instance()->join(obj.m_object);
}
//-----------------------------------------------------------------------------
ACTO_API void process_messages() {
    core::processBindedActors();
}
//-----------------------------------------------------------------------------
ACTO_API void shutdown() {
    if (startup_counter > 0) {
        startup_counter--;
        // Заврешить работу ядра
        if (startup_counter == 0) {
            // -
            services::finalize();
            // -
            core::finalize();
        }
    }
}
//-----------------------------------------------------------------------------
ACTO_API void startup() {
    if (startup_counter == 0) {
        // Инициализировать ядро
        core::initialize();

        // Инициализировать сервисные компоненты
        services::initialize();
    }
    startup_counter++;
}



///////////////////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
object_t::object_t() :
    m_object(0)
{
}
//-----------------------------------------------------------------------------
object_t::object_t(core::object_t* const an_object, bool acquire) :
    m_object(an_object)
{
    if (m_object && acquire)
        core::runtime_t::instance()->acquire(m_object);
}
//-----------------------------------------------------------------------------
object_t::object_t(const object_t& rhs) :
    m_object(rhs.m_object)
{
    if (m_object)
        core::runtime_t::instance()->acquire(m_object);
}
//-----------------------------------------------------------------------------
object_t::~object_t() {
    if (m_object)
        core::runtime_t::instance()->release(m_object);
}
//-----------------------------------------------------------------------------
bool object_t::assigned() const {
    return (m_object != 0);
}
//-----------------------------------------------------------------------------
void object_t::assign(const object_t& rhs) {
    // 1.
    if (rhs.m_object)
        core::runtime_t::instance()->acquire(rhs.m_object);
    // 2.
    if (m_object)
        core::runtime_t::instance()->release(m_object);
    // -
    m_object = rhs.m_object;
}
//-----------------------------------------------------------------------------
bool object_t::same(const object_t& rhs) const {
    return (m_object == rhs.m_object);
}


///////////////////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
actor_t::actor_t() : object_t()
{
}
//-----------------------------------------------------------------------------
actor_t::actor_t(core::object_t* const an_object, bool acquire) :
    object_t(an_object, acquire)
{

}
//-----------------------------------------------------------------------------
actor_t::actor_t(const actor_t& rhs) :
    object_t(rhs)
{
}

//-----------------------------------------------------------------------------
actor_t& actor_t::operator = (const actor_t& rhs) {
    if (this != &rhs)
        object_t::assign( rhs );
    // -
    return *this;
}
//-----------------------------------------------------------------------------
bool actor_t::operator == (const actor_t& rhs) const {
    return object_t::same( rhs );
}
//-----------------------------------------------------------------------------
bool actor_t::operator != (const actor_t& rhs) const {
    return !object_t::same( rhs );
}

}; // namespace acto
