
#ifndef acto_core_module_h_cbc7f72e6ef34806b94705e9abc98295
#define acto_core_module_h_cbc7f72e6ef34806b94705e9abc98295

#include "types.h"

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
 * Базовый класс для всех актеров, как внутренних, так и внешних (пользовательских).
 */
class ACTO_API base_t : public actor_body_t {
    friend class main_module_t;

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
public:
    static main_module_t* instance() {
        static main_module_t    value;

        return &value;
    }

public:
    /// -
    virtual void handle_message(package_t* const package);
    /// Отправить сообщение соответствующему объекту
    virtual void send_message(package_t* const package);
};

} // namespace core

} // namespace acto

#endif // acto_core_module_h_cbc7f72e6ef34806b94705e9abc98295
