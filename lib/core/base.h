#pragma once

#include <util/event.h>
#include <util/intrlist.h>
#include <util/platform.h>
#include <util/stack.h>

#include <functional>
#include <mutex>
#include <set>
#include <typeindex>
#include <typeinfo>
#include <vector>
#include <unordered_map>

namespace acto {

/// Индивидуальный поток для актера
static constexpr int aoExclusive = 0x01;

/// Привязать актера к текущему системному потоку.
/// Не имеет эффекта, если используется в контексте потока,
/// созданного самой библиотекой.
static constexpr int aoBindToThread = 0x02;

namespace core {

class base_t;
class main_module_t;
class runtime_t;
class worker_t;
struct msg_t;

/**
 * Объект
 */
struct object_t : public intrusive_t< object_t > {
    struct waiter_t : public intrusive_t< waiter_t > {
        event_t*    event;
    };

    using atomic_stack_t = generics::mpsc_stack_t<msg_t> ;
    using intusive_stack_t = generics::stack_t<msg_t>;

    // Критическая секция для доступа к полям
    std::recursive_mutex cs;

    // Реализация объекта
    base_t*             impl;
    // Список сигналов для потоков, ожидающих уничтожения объекта
    waiter_t*           waiters;
    // Очередь сообщений, поступивших данному объекту
    atomic_stack_t      input_stack;
    intusive_stack_t    local_stack;
    // Count of references to object
    std::atomic<long>   references;
    // Флаги состояния текущего объекта
    ui32                binded    : 1;
    ui32                deleting  : 1;
    ui32                exclusive : 1;
    ui32                freeing   : 1;
    ui32                scheduled : 1;
    ui32                unimpl    : 1;

public:
    object_t(base_t* const impl_);

    /// Поставить сообщение в очередь
    void enqueue(msg_t* const msg);

    /// Есть ли сообщения
    bool has_messages() const;

    /// Выбрать сообщение из очереди
    msg_t* select_message();
};


/**
 * Базовый тип для сообщений
 */
struct msg_t : intrusive_t<msg_t> {
    // Код типа сообщения
    const std::type_index type;
    // Отправитель сообщения
    object_t* sender{nullptr};
    // Получатель сообщения
    object_t* target{nullptr};

public:
    msg_t(const std::type_index& idx);

    virtual ~msg_t();
};

template <typename T>
struct msg_wrap_t
    : public msg_t
    , private T
{
    inline msg_wrap_t(const T& d)
        : msg_t(typeid(T))
        , T(d)
    { }

    inline msg_wrap_t(T&& d)
        : msg_t(typeid(T))
        , T(std::move(d))
    { }

    inline const T& data() const {
        return *static_cast<const T*>(this);
    }
};

} // namespace code


/**
 * Пользовательский объект (актер)
 */
class actor_ref {
    friend void join(actor_ref& obj);
    friend void destroy(actor_ref& object);

public:
    actor_ref();

    explicit actor_ref(core::object_t* const an_object, const bool acquire = true);

    actor_ref(const actor_ref& rhs);
    actor_ref(actor_ref&& rhs);

    ~actor_ref();

public:
    actor_ref& operator = (const actor_ref& rhs);
    actor_ref& operator = (actor_ref&& rhs);

    bool operator == (const actor_ref& rhs) const;

    bool operator != (const actor_ref& rhs) const;

    /// Инициализирован ли текущий объект
    bool assigned() const;

    core::object_t* data() const {
        return m_object;
    }

    /// Sends a message to the actor.
    template <typename MsgT>
    inline void send(MsgT&& msg) const {
        if (m_object) {
            send_message(
                new core::msg_wrap_t<typename std::remove_reference<MsgT>::type>(std::forward<MsgT>(msg))
            );
        }
    }

    /// Sends a message to the actor.
    template <typename MsgT, typename ... P>
    inline void send(P&& ... p) const {
        if (m_object) {
            send_message(
                new core::msg_wrap_t<MsgT>(MsgT(std::forward<P>(p) ... ))
            );
        }
    }

private:
    ///
    bool same(const actor_ref& rhs) const;
    ///
    void send_message(core::msg_t* const msg) const;

private:
    core::object_t* m_object;
};

namespace core {

/**
 * Базовый класс для локальных актеров.
 */
class base_t {
    friend class main_module_t;

    class handler_t {
    public:
        virtual ~handler_t() = default;

        virtual void invoke(object_t* const sender, const msg_t* const msg) const = 0;
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
        using delegate_t = std::function< void (C*, actor_ref&, const MsgT&) >;

    public:
        mem_handler_t(const delegate_t& delegate_, C* c)
            : m_delegate(delegate_)
            , m_c(c)
        {
        }

        // Вызвать обработчик
        virtual void invoke(object_t* const sender, const msg_t* const msg) const {
            actor_ref actor(sender);

            m_delegate(m_c, actor, static_cast<const msg_wrap_t<MsgT>*>(msg)->data());
        }

    private:
        /// Делегат, хранящий указатель на
        /// метод конкретного объекта.
        const delegate_t m_delegate;

        C* const m_c;
    };

    template <typename MsgT>
    class fun_handler_t : public handler_t {
    public:
        using delegate_t = std::function< void (actor_ref&, const MsgT&) >;

    public:
        fun_handler_t(const delegate_t& delegate_)
            : m_delegate( delegate_ )
        {
        }

        // Вызвать обработчик
        virtual void invoke(object_t* const sender, const msg_t* const msg) const {
            actor_ref actor(sender);

            m_delegate(actor, static_cast< const msg_wrap_t<MsgT>* >(msg)->data());
        }

    private:
        /// Делегат, хранящий указатель на
        /// метод конкретного объекта.
        const delegate_t m_delegate;
    };

public:
    virtual ~base_t() = default;

protected:
    /// Завершить собственную работу
    void die();

    /// Установка обработчика для сообщения данного типа
    template < typename MsgT, typename ClassName >
        inline void Handler( void (ClassName::*func)(actor_ref& sender, const MsgT& msg) ) {
            // Установить обработчик
            set_handler(
                // Тип сообщения
                std::type_index(typeid(MsgT)),
                // Обрабочик
                std::make_unique<mem_handler_t<MsgT, ClassName>>(func, static_cast<ClassName*>(this)));
        }

    /// Установка обработчика для сообщения данного типа
    template <typename MsgT>
        inline void Handler(std::function< void (actor_ref& sender, const MsgT& msg) > func) {
            // Установить обработчик
            set_handler(
                // Тип сообщения
                std::type_index(typeid(MsgT)),
                // Обрабочик
                std::make_unique<fun_handler_t<MsgT>>(func));
        }

    /// Сброс обработчика для сообщения данного типа
    template < typename MsgT >
        inline void Handler() {
            // Сбросить обработчик указанного типа
            set_handler(std::type_index(typeid(MsgT)), nullptr);
        }

private:
    void consume_package(const std::unique_ptr<msg_t>& p);

    void set_handler(const std::type_index& type, std::unique_ptr<handler_t> h);

private:
    /// Карта обработчиков сообщений
    using Handlers = std::unordered_map<std::type_index, std::unique_ptr<handler_t>>;

    /// List of message handlers.
    Handlers m_handlers;
    /// Поток, в котором должен выполнятся объект
    core::worker_t* m_thread{nullptr};
    ///
    bool m_terminating{false};
};

} // namespace core
} // namespace acto
