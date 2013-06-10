#pragma once

#include <util/intrlist.h>
#include <util/stack.h>
#include <util/event.h>
#include <util/platform.h>

#include <functional>
#include <mutex>
#include <set>
#include <typeinfo>
#include <typeindex>
#include <vector>

namespace acto {

class actor_ref;

/// Индивидуальный поток для актера
const int aoExclusive    = 0x01;

/// Привязать актера к текущему системному потоку.
/// Не имеет эффекта, если используется в контексте потока,
/// созданного самой библиотекой.
const int aoBindToThread = 0x02;


namespace core {

class  main_module_t;
class  runtime_t;
class  worker_t;
struct package_t;


/**
 */
class actor_body_t {
public:
    virtual ~actor_body_t()
    { }

    virtual void consume_package(const std::unique_ptr<package_t>&) = 0;
};


/**
 * Базовый класс для модуля системы
 */
class module_t {
public:
    virtual ~module_t() { }

    /// -
    virtual void destroy_object_body(actor_body_t* const /*body*/) {  }

    ///
    virtual void handle_message(package_t* const package) = 0;

    /// Отправить сообщение соответствующему объекту
    virtual void send_message(package_t* const package)   = 0;

    ///
    virtual void shutdown(event_t& event) = 0;

    /// -
    virtual void startup(runtime_t*) = 0;
};


/**
 * Объект
 */
struct object_t : public intrusive_t< object_t > {
    struct waiter_t : public intrusive_t< waiter_t > {
        event_t*    event;
    };

    typedef generics::mpsc_stack_t<package_t> atomic_stack_t;
    typedef generics::stack_t<package_t>      intusive_stack_t;

    // Критическая секция для доступа к полям
    std::recursive_mutex cs;

    // Реализация объекта
    actor_body_t*       impl;
    // Список сигналов для потоков, ожидающих уничтожения объекта
    waiter_t*           waiters;
    // Очередь сообщений, поступивших данному объекту
    atomic_stack_t      input_stack;
    intusive_stack_t    local_stack;
    // Count of references to object
    std::atomic<long>   references;
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


/**
 * Базовый тип для сообщений
 */
struct msg_t {
    std::type_index tid;

public:
    msg_t()
        : tid(typeid(msg_t))
    { }

    virtual ~msg_t()
    { }
};


/**
 * Транспортный пакет для сообщения
 */
struct package_t : public intrusive_t< package_t > {
    // Данные сообщения
    const std::unique_ptr<msg_t>
                            data;
    // Отправитель сообщения
    object_t*               sender;
    // Получатель сообщения
    object_t*               target;
    // Код типа сообщения
    const std::type_index   type;

public:
    package_t(msg_t* const data_, const std::type_index& type_);
    ~package_t();
};


/**
 * Базовый класс для локальных актеров.
 */
class base_t : public actor_body_t {
    friend class main_module_t;

    class handler_t {
    public:
        virtual ~handler_t()
        { }

        virtual void invoke(object_t* const sender, msg_t* const msg) const = 0;
    };

    ///
    struct HandlerItem {
        // Тип обработчика
        const std::type_index       type;
        // Обработчик
        std::unique_ptr<handler_t>  handler;

    public:
        inline HandlerItem(const std::type_index& t, handler_t* h)
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
    class mem_handler_t : public handler_t {
    public:
        typedef std::function< void (C*, actor_ref&, const MsgT&) > delegate_t;

    public:
        mem_handler_t(const delegate_t& delegate_, C* c)
            : m_delegate( delegate_ )
            , m_c       (c)
        {
        }

        // Вызвать обработчик
        virtual void invoke(object_t* const sender, msg_t* const msg) const {
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
    class fun_handler_t : public handler_t {
    public:
        typedef std::function< void (actor_ref&, const MsgT&) > delegate_t;

    public:
        fun_handler_t(const delegate_t& delegate_)
            : m_delegate( delegate_ )
        {
        }

        // Вызвать обработчик
        virtual void invoke(object_t* const sender, msg_t* const msg) const {
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
                std::type_index(typeid(MsgT)));
        }

    /// Установка обработчика для сообщения данного типа
    template <typename MsgT>
        inline void Handler(std::function< void (actor_ref& sender, const MsgT& msg) > func) {
            // Установить обработчик
            set_handler(
                // Обрабочик
                new fun_handler_t< MsgT >(func),
                // Тип сообщения
                std::type_index(typeid(MsgT)));
        }

    /// Сброс обработчика для сообщения данного типа
    template < typename MsgT >
        inline void Handler() {
            // Сбросить обработчик указанного типа
            set_handler(nullptr, std::type_index(typeid(MsgT)));
        }

private:
    virtual void consume_package(const std::unique_ptr<package_t>& p);

    void set_handler(handler_t* const handler, const std::type_index& type);

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

} // namespace core
} // namespace acto
