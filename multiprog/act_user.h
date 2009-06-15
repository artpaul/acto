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
//---------------------------------------------------------------------------//
// File: act_user.h                                                          //
///////////////////////////////////////////////////////////////////////////////

#if !defined __multiprogs__act_user_h__
#define __multiprogs__act_user_h__


namespace acto {

///////////////////////////////////////////////////////////////////////////////
//                         ИНТЕРФЕЙС БИБЛИОТЕКИ                              //
///////////////////////////////////////////////////////////////////////////////


using core::msg_t;
using core::message_class_t;

/**
 * Базовый класс для всех интерфейсных объектов
 */
class object_t {
    // -
    friend inline core::object_t* dereference(object_t& object);
    friend void join(actor_t& obj);
    friend void destroy(object_t& object);

public:
    ~object_t();

public:
    // Инициализирован ли текущий объект
    bool assigned() const;

protected:
    object_t();
    object_t(core::object_t* const an_object);
    object_t(const object_t& rhs);

protected:
    // Присваивает новое значение текущему объекту
    void assign(const object_t& rhs);
    // -
    bool same(const object_t& rhs) const;

protected:
    core::object_t* volatile    m_object;
};



/**
 * Класс предназначен для создания пользовательских экземпляров актеров.
 */
template <typename ActorT>
class instance_t : public object_t {
public:
    // Стандартная инициализация
    instance_t();
    // Указание контекстного объекта для данного
    instance_t(actor_t& context);
    // Указание дополнительных параметров для актера
    instance_t(const int options);
    // -
    instance_t(actor_t& context, const int options);
};



// Desc: Пользовательский объект (актер)
class actor_t : public object_t {
public:
    actor_t();
    // -
    explicit actor_t(core::object_t* const an_object);
    // -
    actor_t(const actor_t& rhs);

    template <typename ActorT>
        actor_t(const instance_t< ActorT >& inst) : object_t(inst)
        {
            // -
        }

public:
    // Послать сообщение объекту
    template <typename MsgT>
        void send(const MsgT& msg) const {
            if (m_object)
                return core::runtime.send(m_object, new MsgT(msg), core::type_box_t< MsgT >());
        }

    template <typename MsgT>
        void send(const core::msg_box_t< MsgT >& box) const {
            if (m_object) {
                MsgT* const         msg = *box;
                const core::TYPEID  id  = msg->tid ? msg->tid : core::type_box_t< MsgT >();

                // Отправить сообщение
                return core::runtime.send(m_object, msg, id);
            }
        }

    // Послать сообщение объекту
    template <typename MsgT, typename P1>
        void send(P1 p1) const {
            if (m_object)
                return core::runtime.send(m_object, new MsgT(p1), core::type_box_t< MsgT >());
        }

/* Операторы */
public:
    actor_t& operator = (const actor_t& rhs);

    template <typename ActorT>
        actor_t& operator = (const instance_t< ActorT >& inst) {
            object_t::assign(inst);
            return *this;
        }

    // -
    bool operator == (const actor_t& rhs) const;
    // -
    bool operator != (const actor_t& rhs) const;
};



/**
 * Базовый класс для реализации пользовательских объектов (актеров)
 */
class implementation_t : public core::base_t {
    template <typename ActorT> friend class instance_t;

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
ACTO_API void destroy(object_t& object);

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


///////////////////////////////////////////////////////////////////////////////
//                                                                           //
///////////////////////////////////////////////////////////////////////////////


// Desc:
inline core::object_t* dereference(object_t& object) {
    return object.m_object;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
//-------------------------------------------------------------------------------------------------
template <typename ActorT>
    instance_t< ActorT >::instance_t() : object_t() {
        // 1.
        ActorT* const value = new ActorT();
        // 2. Создать объект ядра (счетчик ссылок увеличивается автоматически)
        m_object = core::runtime.createActor(value, 0);
        // -
        value->self = actor_t(m_object);
    }
//-------------------------------------------------------------------------------------------------
template <typename ActorT>
    instance_t< ActorT >::instance_t(actor_t& context) : object_t() {
        // 1.
        ActorT* const value = new ActorT();
        // 2. Создать объект ядра (счетчик ссылок увеличивается автоматически)
        m_object = core::runtime.createActor(value, 0);
        // -
        value->context = context;
        value->self    = actor_t(m_object);
    }
//-------------------------------------------------------------------------------------------------
template <typename ActorT>
    instance_t< ActorT >::instance_t(const int options) : object_t() {
        // 1.
        ActorT* const value = new ActorT();
        // 2. Создать объект ядра (счетчик ссылок увеличивается автоматически)
        m_object = core::runtime.createActor(value, options);
        // -
        value->self = actor_t(m_object);
    }
//-------------------------------------------------------------------------------------------------
template <typename ActorT>
    instance_t< ActorT >::instance_t(actor_t& context, const int options) {
        // 1.
        ActorT* const value = new ActorT();
        // 2. Создать объект ядра (счетчик ссылок увеличивается автоматически)
        m_object = core::runtime.createActor(value, options);
        // -
        value->context = context;
        value->self    = actor_t(m_object);
    }

}; // namespace acto

#endif // __multiprogs__act_user_h__
