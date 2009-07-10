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

#ifndef acto_core_module_h_cbc7f72e6ef34806b94705e9abc98295
#define acto_core_module_h_cbc7f72e6ef34806b94705e9abc98295

#include <system/event.h>
#include <system/thread.h>

#include <generic/queue.h>

#include <core/types.h>

namespace acto {

// -
class actor_t;


namespace core {

/** */
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
    handler_t(const delegate_t& delegate_, const TYPEID type_)
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
 * Базовый класс для локальных актеров.
 */
class ACTO_API base_t : public actor_body_t {
    friend class main_module_t;
    friend void do_handle_message(package_t* const package);

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
    // Поток, в котором должен выполнятся объект
    class worker_t* m_thread;
    // -
    bool            m_terminating;

public:
    struct msg_destroy : public msg_t {
        // Объект, который необходимо удалить
        object_t*   object;
    };

public:
    base_t();
    virtual ~base_t();

protected:
    /// Завершить собственную работу
    void terminate();

    /// Установка обработчика для сообщения данного типа
    template < typename MsgT, typename ClassName >
        inline void Handler( void (ClassName::*func)(acto::actor_t& sender, const MsgT& msg) ) {
            // Тип сообщения
            const TYPEID                            a_type     = get_message_type< MsgT >();
            // Метод, обрабатывающий сообщение
            typename handler_t< MsgT >::delegate_t  a_delegate = fastdelegate::MakeDelegate(this, func);
            // Обрабочик
            handler_t< MsgT >* const                handler    = new handler_t< MsgT >(a_delegate, a_type);

            // Установить обработчик
            set_handler(handler, a_type);
        }
    /// Сброс обработчика для сообщения данного типа
    template < typename MsgT >
        inline void Handler() {
            // Сбросить обработчик указанного типа
            set_handler(0, get_message_type< MsgT >());
        }

private:
    void set_handler(i_handler* const handler, const TYPEID type);
};


/**
 * Модуль, обеспечивающий обработку локальных актёров
 */
class main_module_t : public module_t {
    /// -
    core::object_t* create_actor(base_t* const body, const int options);

public:
    main_module_t();
    ~main_module_t();

    static main_module_t* instance() {
        static main_module_t    value;

        return &value;
    }

    /// Определить отправителя сообщения
    static object_t* determine_sender();

public:
    /// -
    virtual void destroy_object_body(actor_body_t* const body);
    /// -
    virtual void handle_message(package_t* const package);
    /// Отправить сообщение соответствующему объекту
    virtual void send_message(package_t* const package);
    /// -
    virtual void shutdown(event_t& event);
    /// -
    virtual void startup();

    /// Создать экземпляр актёра
    template <typename Impl>
    object_t* make_instance(const actor_t& context, const int options) {
        // 1.
        Impl* const value = new Impl();
        // 2. Создать объект ядра (счетчик ссылок увеличивается автоматически)
        core::object_t* const result = this->create_actor(value, options);
        // -
        if (result) {
            value->context = context;
            value->self    = actor_t(result);
        }
        // -
        return result;
    }

private:
    class impl;

    std::auto_ptr< impl >   m_pimpl;
};

} // namespace core

} // namespace acto

#endif // acto_core_module_h_cbc7f72e6ef34806b94705e9abc98295
