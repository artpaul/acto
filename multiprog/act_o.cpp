
#include <acto.h>


namespace acto {

static atomic_t startup_counter = 0;


//-----------------------------------------------------------------------------
// Desc:
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
// Desc:
//-----------------------------------------------------------------------------
ACTO_API void shutdown() {
    if (startup_counter > 0) {
        // Заврешить работу ядра
        if (atomic_decrement(&startup_counter) == 0) {
            // -
            core::finalize();
        }
    }
}

//-----------------------------------------------------------------------------
// Desc: Инициализировать библиотеку
//-----------------------------------------------------------------------------
ACTO_API void startup() {
    if (atomic_increment(&startup_counter) == 1) {
        // Инициализировать ядро
        core::initialize();
    }
}



///////////////////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
object_t::object_t() :
    m_object(0)
{
}
//-----------------------------------------------------------------------------
object_t::object_t(core::object_t* const an_object) :
    m_object(an_object)
{
    if (m_object)
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
actor_t::actor_t(core::object_t* const an_object) :
    object_t(an_object)
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
