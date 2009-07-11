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
    ///
    void process_binded_actors(std::set<object_t*>& actors, const bool need_delete);

public:
    runtime_t();
    ~runtime_t();

    static runtime_t* instance();

public:
    /// Захватить ссылку на объект
    long        acquire(object_t* const obj);
    /// Создать экземпляр объекта, связав его с соответсвтующей реализацией
    object_t*   create_actor(actor_body_t* const body, const int options, const ui8 module);
    /// Создать контекст связи с текущим системным потоком
    void        create_thread_binding();
    /// Уничтожить объект
    void        deconstruct_object(object_t* const object);
    ///
    void        destroy_thread_binding();
    ///
    void        handle_message(package_t* const package);
    /// Ждать уничтожения тела объекта
    void        join(object_t* const obj);
    /// -
    void        process_binded_actors();
    /// -
    long        release(object_t* const obj);
    /// Зарегистрировать новый модуль
    void        register_module(module_t* const inst, const ui8 id);
    /// Послать сообщение указанному объекту
    void        send(object_t* const sender, object_t* const target, msg_t* const msg);
    /// Завершить выполнение
    void        shutdown();
    /// Начать выполнение
    void        startup();

private:
    class impl;

    std::auto_ptr< impl >   m_pimpl;
};

} // namepsace core

} // namespace acto

#endif // acto_runtime_h_9789b1fc3b144e629327ce3279b9ee89
