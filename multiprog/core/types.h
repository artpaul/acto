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

#ifndef acto_core_types_h_D3A964BDF70B498293D671F01F4DF126
#define acto_core_types_h_D3A964BDF70B498293D671F01F4DF126

#include <vector>
#include <set>

#include <generic/delegates.h>
#include <generic/intrlist.h>
#include <generic/stack.h>

#include <system/event.h>

#include "message.h"

namespace acto {

// Индивидуальный поток для актера
const int aoExclusive    = 0x01;
// Привязать актера к текущему системному потоку.
// Не имеет эффекта, если используется в контексте потока,
// созданного самой библиотекой.
const int aoBindToThread = 0x02;


namespace core {

// -
struct package_t;

/**
*/
class actor_body_t {
public:
    virtual ~actor_body_t() { }
};


/**
 * Базовый класс для модуля системы
 */
class module_t {
public:
    /// -
    virtual void destroy_object_body(actor_body_t* const /*body*/) {  }
    ///
    virtual void handle_message(package_t* const package) = 0;
    /// Отправить сообщение соответствующему объекту
    virtual void send_message(package_t* const package)   = 0;
    ///
    virtual void shutdown(event_t& event) = 0;
    /// -
    virtual void startup() = 0;
};


/**
 * Объект
 */
struct ACTO_API object_t : public intrusive_t< object_t > {
    struct waiter_t : public intrusive_t< waiter_t > {
        event_t*    event;
    };

    typedef generics::mpsc_stack_t<package_t> atomic_stack_t;
    typedef generics::stack_t<package_t>      intusive_stack_t;

    // Критическая секция для доступа к полям
    mutex_t             cs;

    // Реализация объекта
    actor_body_t*       impl;
    // Список сигналов для потоков, ожидающих уничтожения объекта
    waiter_t*           waiters;
    // Очередь сообщений, поступивших данному объекту
    atomic_stack_t      input_stack;
    intusive_stack_t    local_stack;
    // Count of references to object
    atomic_t            references;
    /// Модуль в рамках которого создан объект
    const ui32          module    : 4;
    // Флаги состояния текущего объекта
    ui32                binded    : 1;
    ui32                deleting  : 1;
    ui32                exclusive : 1;
    ui32                freeing   : 1;
    ui32                scheduled : 1;
    ui32                unimpl    : 1;

public:
    object_t(actor_body_t* const impl_, const ui8 module_);

    /// Поставить сообщение в очередь
    void enqueue(package_t* const msg);
    /// Есть ли сообщения
    bool has_messages() const;
    /// Выбрать сообщение из очереди
    package_t* select_message();
};


/** Транспортный пакет для сообщения */
struct ACTO_API package_t : public intrusive_t< package_t > {
    // Данные сообщения
    msg_t* const        data;
    // Отправитель сообщения
    object_t*           sender;
    // Получатель сообщения
    object_t*           target;
    // Код типа сообщения
    const TYPEID        type;

public:
    package_t(msg_t* const data_, const TYPEID type_);
    ~package_t();
};

} // namespace core

} // namespace acto

#endif // acto_core_types_h_D3A964BDF70B498293D671F01F4DF126
