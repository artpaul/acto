///////////////////////////////////////////////////////////////////////////////
//                           The act-o Library                               //
//---------------------------------------------------------------------------//
// Copyright © 2007 - 2009                                                   //
//     Pavel A. Artemkin (acto.stan@gmail.com)                               //
// ------------------------------------------------------------------ -------//
// License:                                                                  //
//     Code covered by the MIT License.                                      //
//     The authors make no representations about the suitability of this     //
//     software for any purpose. It is provided "as is" without express or   //
//     implied warranty.                                                     //
///////////////////////////////////////////////////////////////////////////////

#ifndef acto_runtime_h_9789b1fc3b144e629327ce3279b9ee89
#define acto_runtime_h_9789b1fc3b144e629327ce3279b9ee89

#include "types.h"

namespace acto {

namespace core {

/**
 * Данные среды выполнения
 */
class runtime_t {
public:
    runtime_t();
    ~runtime_t();

    static runtime_t* instance();

public:
    // -
    void        acquire(object_t* const obj);
    // Создать экземпляр объекта, связав его с соответсвтующей реализацией
    object_t*   create_actor(base_t* const body, const int options = 0);
    // Уничтожить объект
    void        destroy_object(object_t* const object);
    // -
    void        destroy_object_body(object_t* obj);
    // -
    void        join(object_t* const obj);
    // -
    object_t*   pop_object();
    // -
    void        push_object(object_t* obj);
    // -
    long        release(object_t* const obj);
    // Послать сообщение указанному объекту
    void        send(object_t* const target, msg_t* const msg, const TYPEID type);
    // Завершить выполнение
    void        shutdown();
    // Начать выполнение
    void        startup();

private:
    class impl;

    std::auto_ptr< impl >   m_pimpl;
};

} // namepsace core

} // namespace acto

#endif // acto_runtime_h_9789b1fc3b144e629327ce3279b9ee89

