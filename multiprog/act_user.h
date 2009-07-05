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

#ifndef act_user_h_02821F1061B24ad28024E630DDF1DC9E
#define act_user_h_02821F1061B24ad28024E630DDF1DC9E

#include <core/runtime.h>
#include <modules/main/module.h>

namespace acto {

namespace detail {

template <typename Impl>
inline core::object_t* make_instance(const actor_t& context, const int options) {
    return core::main_module_t::instance()->make_instance< Impl >(context, options);
}

} // namespace detail

///////////////////////////////////////////////////////////////////////////////
//                         ИНТЕРФЕЙС БИБЛИОТЕКИ                              //
///////////////////////////////////////////////////////////////////////////////

using core::msg_t;
using core::message_class_t;


/**
 * Пользовательский объект (актер)
 */
class actor_t {
    friend inline core::object_t* dereference(actor_t& object);
    friend void join(actor_t& obj);
    friend void destroy(actor_t& object);

private:
    core::object_t* volatile  m_object;

    /// Присваивает новое значение текущему объекту
    void assign(const actor_t& rhs);
    ///
    bool same(const actor_t& rhs) const;
    ///
    template <typename T>
    void send_message(T* const msg) const {
        if (m_object) {
            assert(msg != NULL);

            if (msg->meta == NULL)
                msg->meta = core::get_metaclass< T >();
            // Отправить сообщение
            core::runtime_t::instance()->send(core::main_module_t::determine_sender(), m_object, msg);
        }
    }

public:
    actor_t();
    // -
    explicit actor_t(core::object_t* const an_object, const bool acquire = true);
    // -
    actor_t(const actor_t& rhs);

    ~actor_t();

public:
    /// Инициализирован ли текущий объект
    bool assigned() const;

    // Послать сообщение объекту
    template <typename MsgT>
    inline void send(const MsgT& msg) const {
        this->send_message< MsgT >(new MsgT(msg));
    }

    template <typename MsgT>
    inline void send(const core::msg_box_t< MsgT >& box) const {
        this->send_message< MsgT >(*box);
    }

    // Послать сообщение объекту
    template <typename MsgT>
    inline void send() const {
        this->send_message< MsgT >(new MsgT());
    }

    template <typename MsgT, typename P1>
    inline void send(P1 p1) const {
        this->send_message< MsgT >(new MsgT(p1));
    }

    template <typename MsgT, typename P1, typename P2>
    inline void send(P1 p1, P2 p2) const {
        this->send_message< MsgT >(new MsgT(p1, p2));
    }

    template <typename MsgT, typename P1, typename P2, typename P3>
    inline void send(P1 p1, P2 p2, P3 p3) const {
        this->send_message< MsgT >(new MsgT(p1, p2, p3));
    }

/* Операторы */
public:
    actor_t& operator = (const actor_t& rhs);
    // -
    bool operator == (const actor_t& rhs) const;
    // -
    bool operator != (const actor_t& rhs) const;
};


/**
 * Базовый класс для реализации пользовательских объектов (актеров)
 */
class implementation_t : public core::base_t {
    friend class core::main_module_t;

protected:
    // Ссылка на контекстный объект для данного
    actor_t     context;
    // Ссылка на самого себя
    actor_t     self;
};


// Desc:
struct msg_destroy : public msg_t { };

// Desc:
struct msg_time : public msg_t { };


///////////////////////////////////////////////////////////////////////////////
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

/* Уничтожить указанный объект */
ACTO_API void destroy(actor_t& object);

//
ACTO_API void finalize_thread();

// Включить в текущем потоке возможность взаимодействия
// с ядром библиотеки acto
ACTO_API void initialize_thread();

/* Дождаться завершения работы указанног актера */
ACTO_API void join(actor_t& obj);

// Обработать все сообщения для объектов,
// привязанных к текущему системному потоку (опция aoBindToThread)
ACTO_API void process_messages();

/* Завершить использование библиотеки */
ACTO_API void shutdown();

/* Инициализировать библиотеку */
ACTO_API void startup();


//-----------------------------------------------------------------------------
template <typename T>
inline actor_t instance() {
    actor_t a;
    return actor_t(detail::make_instance< T >(a, 0), false);
}
//-----------------------------------------------------------------------------
template <typename T>
inline actor_t instance(actor_t& context) {
    return actor_t(detail::make_instance< T >(context, 0), false);
}
//-----------------------------------------------------------------------------
template <typename T>
inline actor_t instance(const int options) {
    actor_t a;
    return actor_t(detail::make_instance< T >(a, options), false);
}
//-----------------------------------------------------------------------------
template <typename T>
inline actor_t instance(actor_t& context, const int options) {
    return actor_t(detail::make_instance< T >(context, options), false);
}

} // namespace acto

#endif // _act_user_h_02821F1061B24ad28024E630DDF1DC9E
