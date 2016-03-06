#pragma once

#include <core/module.h>
#include <core/runtime.h>
#include <core/services.h>

#include <util/platform.h>

namespace acto {

///////////////////////////////////////////////////////////////////////////////
//                         ИНТЕРФЕЙС БИБЛИОТЕКИ                              //
///////////////////////////////////////////////////////////////////////////////

using core::msg_t;


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

    ~actor_ref();

public:
    /// Инициализирован ли текущий объект
    bool assigned() const;

    core::object_t* data() const {
        return m_object;
    }

    // Послать сообщение объекту
    template <typename MsgT>
    inline void send(const MsgT& msg) const {
        send_message(
            new core::msg_wrap_t<MsgT>(msg)
        );
    }

    // Послать сообщение объекту
    template <typename MsgT>
    inline void send(MsgT&& msg) const {
        send_message(
            new core::msg_wrap_t<typename std::remove_reference<MsgT>::type>(std::forward<MsgT>(msg))
        );
    }

    // Послать сообщение объекту
    template <typename MsgT, typename ... P>
    inline void send(P&& ... p) const {
        send_message(
            new core::msg_wrap_t<MsgT>(MsgT(std::forward<P>(p) ... ))
        );
    }

public:
    actor_ref& operator = (const actor_ref& rhs);

    bool operator == (const actor_ref& rhs) const;

    bool operator != (const actor_ref& rhs) const;

private:
    core::object_t* volatile  m_object;

    /// Присваивает новое значение текущему объекту
    void assign(const actor_ref& rhs);
    ///
    bool same(const actor_ref& rhs) const;
    ///
    inline void send_message(const core::msg_t* const msg) const {
        if (m_object) {
            assert(msg != NULL);

            // Отправить сообщение
            core::runtime_t::instance()->send(core::main_module_t::determine_sender(), m_object, msg);
        }
    }
};


/**
 * Базовый класс для реализации пользовательских объектов (актеров)
 */
class actor : public core::base_t {
    friend class core::main_module_t;

protected:
    // Ссылка на контекстный объект для данного
    actor_ref   context;
    // Ссылка на самого себя
    actor_ref   self;
};


/** */
struct msg_destroy : public msg_t { };

/** */
struct msg_time : public msg_t { };


///////////////////////////////////////////////////////////////////////////////
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

/* Уничтожить указанный объект */
ACTO_API void destroy(actor_ref& object);

//
ACTO_API void finalize_thread();

// Включить в текущем потоке возможность взаимодействия
// с ядром библиотеки acto
ACTO_API void initialize_thread();

/* Дождаться завершения работы указанног актера */
ACTO_API void join(actor_ref& obj);

// Обработать все сообщения для объектов,
// привязанных к текущему системному потоку (опция aoBindToThread)
ACTO_API void process_messages();

/* Завершить использование библиотеки */
ACTO_API void shutdown();

/* Инициализировать библиотеку */
ACTO_API void startup();


namespace detail {

template <typename Impl>
inline core::object_t* make_instance(const actor_ref& context, const int options) {
    return core::main_module_t::instance()->make_instance< Impl >(context, options);
}

} // namespace detail

template <typename T>
inline actor_ref spawn() {
    actor_ref a;
    return actor_ref(detail::make_instance<T>(a, 0), false);
}

template <typename T>
inline actor_ref spawn(actor_ref& context) {
    return actor_ref(detail::make_instance<T>(context, 0), false);
}

template <typename T>
inline actor_ref spawn(const int options) {
    actor_ref a;
    return actor_ref(detail::make_instance<T>(a, options), false);
}

template <typename T>
inline actor_ref spawn(actor_ref& context, const int options) {
    return actor_ref(detail::make_instance<T>(context, options), false);
}

} // namespace acto
