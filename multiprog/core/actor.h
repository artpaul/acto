#pragma once

#include "message.h"
#include "types.h"

#include <functional>
#include <vector>

namespace acto {

namespace core {
    class main_module_t;
    class worker_t;

    void do_handle_message(core::package_t* const p);
} // core

class actor_ref;

/** */
struct i_handler {
    // Идентификатор типа сообщения
    const TYPEID    m_type;

public:
    i_handler(const TYPEID type_);
    virtual ~i_handler();

public:
    virtual void invoke(core::object_t* const sender, msg_t* const msg) const = 0;
};


/**
 * Базовый класс для локальных актеров.
 */
class base_t : public core::actor_body_t {
    friend class core::main_module_t;
    friend void core::do_handle_message(core::package_t* const package);

    ///
    struct HandlerItem {
        // Тип обработчика
        const TYPEID    type;
        // Обработчик
        std::unique_ptr<i_handler>
                        handler;

    public:
        HandlerItem(const TYPEID t, i_handler* h)
            : type   (t)
            , handler(h)
        {
        }
    };

    /// Карта обработчиков сообщений
    typedef std::vector< HandlerItem* >         Handlers;

    /**
      * Обертка для вызова обработчика сообщения конкретного типа
      */
    template <
        typename MsgT,
        typename C
    >
    class mem_handler_t : public i_handler {
    public:
        typedef std::function< void (C*, actor_ref&, const MsgT&) > delegate_t;

    public:
        mem_handler_t(const delegate_t& delegate_, C* c, const TYPEID type_)
            : i_handler ( type_ )
            , m_delegate( delegate_ )
            , m_c       (c)
        {
        }

        // Вызвать обработчик
        virtual void invoke(core::object_t* const sender, msg_t* const msg) const {
            actor_ref actor(sender);

            m_delegate(m_c, actor, *static_cast< const MsgT* const >(msg));
        }

    private:
        /// Делегат, хранящий указатель на
        /// метод конкретного объекта.
        const delegate_t    m_delegate;

        C* const            m_c;
    };

    template <typename MsgT>
    class handler_t : public i_handler {
    public:
        typedef std::function< void (actor_ref&, const MsgT&) > delegate_t;

    public:
        handler_t(const delegate_t& delegate_, const TYPEID type_)
            : i_handler ( type_ )
            , m_delegate( delegate_ )
        {
        }

        // Вызвать обработчик
        virtual void invoke(core::object_t* const sender, msg_t* const msg) const {
            actor_ref actor(sender);

            m_delegate(actor, *static_cast< const MsgT* const >(msg));
        }

    private:
        /// Делегат, хранящий указатель на
        /// метод конкретного объекта.
        const delegate_t    m_delegate;
    };

public:
    struct msg_destroy : public msg_t {
        // Объект, который необходимо удалить
        core::object_t* object;
    };

public:
    base_t();
    virtual ~base_t();

protected:
    /// Завершить собственную работу
    void die();

    /// Установка обработчика для сообщения данного типа
    template < typename MsgT, typename ClassName >
        inline void Handler( void (ClassName::*func)(actor_ref& sender, const MsgT& msg) ) {
            // Тип сообщения
            const TYPEID                            a_type     = core::get_message_type< MsgT >();
            // Метод, обрабатывающий сообщение
            typename mem_handler_t< MsgT, ClassName >::delegate_t  a_delegate = func;
            // Обрабочик
            mem_handler_t< MsgT, ClassName >* const                handler    = new mem_handler_t< MsgT, ClassName >(a_delegate, static_cast<ClassName*>(this), a_type);

            // Установить обработчик
            set_handler(handler, a_type);
        }

    /// Установка обработчика для сообщения данного типа
    template <typename MsgT>
        inline void Handler(std::function< void (actor_ref& sender, const MsgT& msg) > func) {
            // Тип сообщения
            const TYPEID                            a_type     = core::get_message_type< MsgT >();
            // Метод, обрабатывающий сообщение
            typename handler_t< MsgT >::delegate_t  a_delegate = func;
            // Обрабочик
            handler_t< MsgT >* const                handler    = new handler_t< MsgT >(a_delegate, a_type);

            // Установить обработчик
            set_handler(handler, a_type);
        }

    /// Сброс обработчика для сообщения данного типа
    template < typename MsgT >
        inline void Handler() {
            // Сбросить обработчик указанного типа
            set_handler(0, core::get_message_type< MsgT >());
        }

private:
    void set_handler(i_handler* const handler, const TYPEID type);

private:
    // Карта обработчиков сообщений для данного объекта
    Handlers        m_handlers;
    // Поток, в котором должен выполнятся объект
    core::worker_t* m_thread;
    // -
    bool            m_terminating;
};

} // namesapce acto
