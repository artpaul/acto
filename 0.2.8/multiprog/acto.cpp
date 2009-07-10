
#include <core/runtime.h>

#include "act_user.h"

namespace acto {

static atomic_t startup_counter = 0;

//-----------------------------------------------------------------------------
ACTO_API void destroy(actor_t& object) {
    if (core::object_t* const obj = object.m_object) {
        object.m_object = NULL;
        // Освободить ссылку на объект и удалить его
        if (core::runtime_t::instance()->release(obj) > 0)
            core::runtime_t::instance()->deconstruct_object(obj);
    }
}
//-----------------------------------------------------------------------------
ACTO_API void finalize_thread() {
    core::runtime_t::instance()->destroy_thread_binding();
}
//-----------------------------------------------------------------------------
ACTO_API void initialize_thread() {
    core::runtime_t::instance()->create_thread_binding();
}
//-----------------------------------------------------------------------------
ACTO_API void join(actor_t& obj) {
    if (obj.m_object)
        core::runtime_t::instance()->join(obj.m_object);
}
//-----------------------------------------------------------------------------
ACTO_API void process_messages() {
    core::runtime_t::instance()->process_binded_actors();
}
//-----------------------------------------------------------------------------
ACTO_API void shutdown() {
    if (startup_counter > 0) {
        // Заврешить работу ядра
        if (atomic_decrement(&startup_counter) == 0) {
            // -
            core::runtime_t::instance()->shutdown();
        }
    }
}
//-----------------------------------------------------------------------------
ACTO_API void startup() {
    if (atomic_increment(&startup_counter) == 1) {
        //
        // Инициализировать ядро
        //
        core::runtime_t::instance()->startup();
        core::runtime_t::instance()->register_module(core::main_module_t::instance(), 0);
    }
}


///////////////////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
actor_t::actor_t()
    : m_object(NULL)
{
}
//-----------------------------------------------------------------------------
actor_t::actor_t(core::object_t* const an_object, const bool acquire)
    : m_object(an_object)
{
    if (m_object && acquire)
        core::runtime_t::instance()->acquire(m_object);
}
//-----------------------------------------------------------------------------
actor_t::actor_t(const actor_t& rhs)
    : m_object(rhs.m_object)
{
    if (m_object)
        core::runtime_t::instance()->acquire(m_object);
}
//-----------------------------------------------------------------------------
actor_t::~actor_t() {
    if (m_object)
        core::runtime_t::instance()->release(m_object);
}

//-----------------------------------------------------------------------------
bool actor_t::assigned() const {
    return (m_object != NULL);
}
//-----------------------------------------------------------------------------
void actor_t::assign(const actor_t& rhs) {
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
bool actor_t::same(const actor_t& rhs) const {
    return (m_object == rhs.m_object);
}

//-----------------------------------------------------------------------------
actor_t& actor_t::operator = (const actor_t& rhs) {
    if (this != &rhs)
        this->assign(rhs);
    // -
    return *this;
}
//-----------------------------------------------------------------------------
bool actor_t::operator == (const actor_t& rhs) const {
    return this->same(rhs);
}
//-----------------------------------------------------------------------------
bool actor_t::operator != (const actor_t& rhs) const {
    return !this->same(rhs);
}

}; // namespace acto
