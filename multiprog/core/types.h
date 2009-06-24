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

// -
class actor_t;

// Индивидуальный поток для актера
const int aoExclusive    = 0x01;
// Привязать актера к текущему системному потоку.
// Не имеет эффекта, если используется в контексте потока,
// созданного самой библиотекой.
const int aoBindToThread = 0x02;


namespace core {

// -
class  worker_t;
struct i_handler;
struct object_t;
struct package_t;


// Desc:
struct ACTO_API i_handler {
    // Идентификатор типа сообщения
    const TYPEID    m_type;

public:
    i_handler(const TYPEID type_);

public:
    virtual void invoke(object_t* const sender, msg_t* const msg) const = 0;
};


/** Обертка для вызова обработчика сообщения конкретного типа */
template <typename MsgT>
class handler_t : public i_handler {
public:
    typedef fastdelegate::FastDelegate< void (acto::actor_t&, const MsgT&) >    delegate_t;

public:
    handler_t(const delegate_t& delegate_, type_box_t< MsgT >& type_)
        : i_handler ( type_ )
        , m_delegate( delegate_ )
    {
    }

    // Вызвать обработчик
    virtual void invoke(object_t* const sender, msg_t* const msg) const {
        acto::actor_t   actor(sender);

        m_delegate(actor, *static_cast< const MsgT* const >(msg));
    }

private:
    /// Делегат, хранящий указатель на
    /// метод конкретного объекта.
    const delegate_t    m_delegate;
};


/**
 * Базовый класс для всех актеров, как внутренних, так и внешних (пользовательских).
 */
class ACTO_API base_t {
    ///
    struct HandlerItem {
        // Обработчик
        i_handler*          handler;
        // Тип обработчика
        const TYPEID        type;

    public:
        HandlerItem(const TYPEID type_) : type( type_ ), handler( 0 ) { }
    };

    /// Карта обработчиков сообщений
    typedef std::vector< HandlerItem* >         Handlers;

private:
    // Карта обработчиков сообщений для данного объекта
    Handlers        m_handlers;
    bool            m_terminating;

public:
    struct msg_destroy : public msg_t {
        // Объект, который необходимо удалить
        object_t*   object;
    };

public:
    base_t();
    virtual ~base_t();

    static void handle_message(package_t* const package);

protected:
    /// Завершить собственную работу
    void terminate();

    /// Установка обработчика для сообщения данного типа
    template < typename MsgT, typename ClassName >
        inline void Handler( void (ClassName::*func)(acto::actor_t& sender, const MsgT& msg) ) {
            // Тип сообщения
            type_box_t< MsgT >                       a_type     = type_box_t< MsgT >();
            // Метод, обрабатывающий сообщение
            typename handler_t< MsgT >::delegate_t   a_delegate = fastdelegate::MakeDelegate(this, func);
            // Обрабочик
            handler_t< MsgT >* const                 handler    = new handler_t< MsgT >(a_delegate, a_type);

            // Установить обработчик
            set_handler(handler, a_type);
        }
    /// Сброс обработчика для сообщения данного типа
    template < typename MsgT >
        inline void Handler() {
            // Тип сообщения
            type_box_t< MsgT >	a_type = type_box_t< MsgT >();
            // Сбросить обработчик указанного типа
            set_handler(0, a_type);
        }

private:
    void set_handler(i_handler* const handler, const TYPEID type);
};


// Desc: Объект
struct ACTO_API object_t : public intrusive_t< object_t > {
    struct waiter_t : public intrusive_t< waiter_t > {
        event_t*    event;
    };

    typedef generics::mpsc_stack_t<package_t> atomic_stack_t; 
    typedef generics::stack_t<package_t>      intusive_stack_t; 

    // Критическая секция для доступа к полям
    mutex_t             cs;

    // Реализация объекта
    base_t*             impl;
    // Поток, в котором должен выполнятся объект
    worker_t* const     thread;
    // Список сигналов для потоков, ожидающих уничтожения объекта
    waiter_t*           waiters;
    // Очередь сообщений, поступивших данному объекту
    atomic_stack_t      input_stack;
    intusive_stack_t    local_stack;
    // Count of references to object
    atomic_t            references;

    // Флаги состояния текущего объекта
    unsigned int        binded    : 1;
    unsigned int        deleting  : 1;
    unsigned int        freeing   : 1;
    unsigned int        scheduled : 1;
    unsigned int        unimpl    : 1;

public:
    object_t(worker_t* const thread_);

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


/** Контекс потока */
struct thread_context_t {
    // Список актеров ассоциированных
    // только с текущим потоком
    std::set< object_t* >   actors;
    // Счетчик инициализаций
    int                     counter;
    // -
    bool                    is_core;
    // Текущий активный объект в данном потоке
    object_t*               sender;
};

extern TLS_VARIABLE thread_context_t* threadCtx;

} // namespace core 

} // namespace acto

#endif // acto_core_types_h_D3A964BDF70B498293D671F01F4DF126
