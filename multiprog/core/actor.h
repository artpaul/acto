#pragma once

#include "message.h"
#include "types.h"

#include <functional>
#include <vector>

namespace acto {

namespace core {
    class main_module_t;
    class worker_t;
} // core

class actor_ref;

/**
 * Базовый класс для локальных актеров.
 */
class base_t : public core::actor_body_t {
    friend class core::main_module_t;

    class i_handler {
    public:
        virtual ~i_handler()
        { }

        virtual void invoke(core::object_t* const sender, msg_t* const msg) const = 0;
    };

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
        mem_handler_t(const delegate_t& delegate_, C* c)
            : m_delegate( delegate_ )
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
        handler_t(const delegate_t& delegate_)
            : m_delegate( delegate_ )
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
    base_t();
    virtual ~base_t();

protected:
    /// Завершить собственную работу
    void die();

    /// Установка обработчика для сообщения данного типа
    template < typename MsgT, typename ClassName >
        inline void Handler( void (ClassName::*func)(actor_ref& sender, const MsgT& msg) ) {
            // Установить обработчик
            set_handler(
                // Обрабочик
                new mem_handler_t< MsgT, ClassName >(func, static_cast<ClassName*>(this)),
                // Тип сообщения
                core::get_message_type< MsgT >());
        }

    /// Установка обработчика для сообщения данного типа
    template <typename MsgT>
        inline void Handler(std::function< void (actor_ref& sender, const MsgT& msg) > func) {
            // Установить обработчик
            set_handler(
                // Обрабочик
                new handler_t< MsgT >(func),
                // Тип сообщения
                core::get_message_type< MsgT >());
        }

    /// Сброс обработчика для сообщения данного типа
    template < typename MsgT >
        inline void Handler() {
            // Сбросить обработчик указанного типа
            set_handler(nullptr, core::get_message_type< MsgT >());
        }

private:
    void handle_message(const std::unique_ptr<core::package_t>&);

    //i_handler* find_handler(const TYPEID type) const;

    void set_handler(i_handler* const handler, const TYPEID type);

private:
    /// Карта обработчиков сообщений
    typedef std::vector<HandlerItem> Handlers;

    // Карта обработчиков сообщений для данного объекта
    Handlers        m_handlers;
    // Поток, в котором должен выполнятся объект
    core::worker_t* m_thread;
    // -
    bool            m_terminating;
};

} // namesapce acto
