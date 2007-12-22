
#include "multiprog.h"


namespace multiprog
{

namespace acto
{

static volatile unsigned int	startup_counter = 0;


//-------------------------------------------------------------------------------------------------
// Desc:
//-------------------------------------------------------------------------------------------------
ACTO_API void destroy(object_t& object)
{
    core::object_t* const obj = object.m_object;
    // -
    if (system::AtomicCompareExchangePointer((volatile PVOID*)&object.m_object, (void*)0, obj) != 0)
    {  
        // Освободить ссылку на объект и удалить его
        if (core::runtime.release(obj) > 0)
	        core::runtime.destroyObject(obj);
    }
}

//-------------------------------------------------------------------------------------------------
// Desc:
//-------------------------------------------------------------------------------------------------
ACTO_API void shutdown()
{
	if (startup_counter > 0) {
		startup_counter--;
		// Заврешить работу ядра
		if (startup_counter == 0) {
            core::finalize();
            // Системный уровень
            system::finalize();
		}
	}
}

//-------------------------------------------------------------------------------------------------
// Desc: Инициализировать библиотеку
//-------------------------------------------------------------------------------------------------
ACTO_API void startup()
{
	if (startup_counter == 0) {
        // Инициализация системного уровня
        system::initialize();
        
		// Инициализировать ядро
        core::initialize();
	}
	startup_counter++;
}



///////////////////////////////////////////////////////////////////////////////////////////////////
//-------------------------------------------------------------------------------------------------
object_t::object_t() :
	m_object( 0 )
{
}
//-------------------------------------------------------------------------------------------------
object_t::object_t(core::object_t* const an_object) :
	m_object( an_object )
{
	if (m_object) 
        core::runtime.acquire(m_object);
}
//-------------------------------------------------------------------------------------------------
object_t::object_t(const object_t& rhs) :
	m_object( rhs.m_object )
{
    if (m_object) 
        core::runtime.acquire(m_object);
}
//-------------------------------------------------------------------------------------------------
object_t::~object_t()
{
	if (m_object) 
        core::runtime.release(m_object);
}

//-------------------------------------------------------------------------------------------------
bool object_t::assigned() const
{
	return (m_object != 0);
}

//-------------------------------------------------------------------------------------------------
void object_t::assign(const object_t& rhs)
{
    // 1.
    if (rhs.m_object)
        core::runtime.acquire(rhs.m_object);
    // 2.
    if (m_object) 
        core::runtime.release(m_object);
    // -
	m_object = rhs.m_object;
}
//-------------------------------------------------------------------------------------------------
bool object_t::same(const object_t& rhs) const
{
	return (m_object == rhs.m_object);
}


///////////////////////////////////////////////////////////////////////////////////////////////////
//-------------------------------------------------------------------------------------------------
actor_t::actor_t() : object_t()
{
}
//-------------------------------------------------------------------------------------------------
actor_t::actor_t(core::object_t* const an_object) : 
    object_t(an_object)
{
	
}
//-------------------------------------------------------------------------------------------------
actor_t::actor_t(const actor_t& rhs) : 
    object_t(rhs)
{
}

//-------------------------------------------------------------------------------------------------
actor_t& actor_t::operator = (const actor_t& rhs)
{
	if (this != &rhs) 
        object_t::assign( rhs );
	// -
	return *this;
}
//-------------------------------------------------------------------------------------------------
bool actor_t::operator == (const actor_t& rhs) const
{
	return object_t::same( rhs );
}
//-------------------------------------------------------------------------------------------------
bool actor_t::operator != (const actor_t& rhs) const
{
	return !object_t::same( rhs );
}


}; // namespace acto

}; // namespace multiprog
