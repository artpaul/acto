#include "act_user.h"

#include <core/runtime.h>
#include <extension/services.h>

#include <atomic>

namespace acto {

///////////////////////////////////////////////////////////////////////////////

static std::atomic_long startup_counter(0);

///////////////////////////////////////////////////////////////////////////////

void destroy(actor_ref& object) {
    if (core::object_t* const obj = object.m_object) {
        object.m_object = NULL;
        // Освободить ссылку на объект и удалить его
        if (core::runtime_t::instance()->release(obj) > 0)
            core::runtime_t::instance()->deconstruct_object(obj);
    }
}

void finalize_thread() {
    core::runtime_t::instance()->destroy_thread_binding();
}

void initialize_thread() {
    core::runtime_t::instance()->create_thread_binding();
}

void join(actor_ref& obj) {
    if (obj.m_object) {
        core::runtime_t::instance()->join(obj.m_object);
    }
}

void process_messages() {
    core::runtime_t::instance()->process_binded_actors();
}

void shutdown() {
    if (startup_counter > 0) {
        // Заврешить работу ядра
        if (--startup_counter == 0) {
            // -
            core::runtime_t::instance()->shutdown();
        }
    }
}

void startup() {
    if (++startup_counter == 1) {
        //
        // Инициализировать ядро
        //
        core::runtime_t::instance()->startup();
        core::runtime_t::instance()->register_module(core::main_module_t::instance(), 0);
    }
}

///////////////////////////////////////////////////////////////////////////////

actor_ref::actor_ref()
    : m_object(NULL)
{
}

actor_ref::actor_ref(core::object_t* const an_object, const bool acquire)
    : m_object(an_object)
{
    if (m_object && acquire)
        core::runtime_t::instance()->acquire(m_object);
}

actor_ref::actor_ref(const actor_ref& rhs)
    : m_object(rhs.m_object)
{
    if (m_object)
        core::runtime_t::instance()->acquire(m_object);
}

actor_ref::~actor_ref() {
    if (m_object)
        core::runtime_t::instance()->release(m_object);
}

bool actor_ref::assigned() const {
    return (m_object != NULL);
}

void actor_ref::assign(const actor_ref& rhs) {
    // 1.
    if (rhs.m_object)
        core::runtime_t::instance()->acquire(rhs.m_object);
    // 2.
    if (m_object)
        core::runtime_t::instance()->release(m_object);
    // -
    m_object = rhs.m_object;
}

bool actor_ref::same(const actor_ref& rhs) const {
    return (m_object == rhs.m_object);
}

actor_ref& actor_ref::operator = (const actor_ref& rhs) {
    if (this != &rhs)
        this->assign(rhs);

    return *this;
}

bool actor_ref::operator == (const actor_ref& rhs) const {
    return this->same(rhs);
}

bool actor_ref::operator != (const actor_ref& rhs) const {
    return !this->same(rhs);
}

///////////////////////////////////////////////////////////////////////////////

} // namespace acto
