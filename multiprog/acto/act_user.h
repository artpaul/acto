///////////////////////////////////////////////////////////////////////////////////////////////////
//                                     The act_o Library                                         //
//                                                                                               //
//-----------------------------------------------------------------------------------------------//
// Copyright © 2007 - 2008                                                                       //
//     Pavel A. Artemkin (acto.stan@gmail.com)                                                   //
// ----------------------------------------------------------------------------------------------//
// License:                                                                                      //
//     Code covered by the MIT License.                                                          //
//     The authors make no representations about the suitability of this software                //
//     for any purpose. It is provided "as is" without express or implied warranty.              //
//-----------------------------------------------------------------------------------------------//
// File: act_user.h                                                                              //
///////////////////////////////////////////////////////////////////////////////////////////////////

#if !defined __multiprogs__act_user_h__
#define __multiprogs__act_user_h__


namespace multiprog {

namespace acto {

///////////////////////////////////////////////////////////////////////////////
//                         ИНТЕРФЕЙС БИБЛИОТЕКИ                              //
///////////////////////////////////////////////////////////////////////////////


using core::msg_t;


// Индивидуальный поток для актера
const int aoExclusive    = 0x01;
// Привязать актера к текущему системному потоку.
// Не имеет эффекта, если используется в контексте потока,
// созданного самой библиотекой.
const int aoBindToThread = 0x02;


// Desc: Базовый класс для всех интерфейсных объектов
class object_t {
    // -
    friend inline core::object_t* dereference(object_t& object);
    // -
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
    core::object_t*     m_object;
};



// Desc: Класс предназначен для создания пользовательских
//       экземпляров актеров.
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
        actor_t(const instance_t< ActorT >& inst);

public:
    // Послать сообщение объекту
    template <typename MsgT>
        void send(const MsgT& msg) const;

/* Операторы */
public:
    actor_t& operator = (const actor_t& rhs);

    template <typename ActorT>
        actor_t& operator = (const instance_t< ActorT >& inst);

    // -
    bool operator == (const actor_t& rhs) const;
    // -
    bool operator != (const actor_t& rhs) const;
};



// Desc: Базовый класс для реализации пользовательских объектов (актеров)
class implementation_t : public core::base_t {
    template <typename ActorT> friend class instance_t;

protected:
    // Ссылка на контекстный объект для данного
    actor_t     context;
    // Ссылка на самого себя
    actor_t     self;
};


///////////////////////////////////////////////////////////////////////////////
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

// Уничтожить указанный объект
ACTO_API void destroy(object_t& object);

//
ACTO_API void finalize_thread();

// Включить в текущем потоке возможность взаимодействия
// с ядром библиотеки acto
ACTO_API void initialize_thread();

// Обработать все сообщения для объектов,
// привязанных к текущему системному потоку (опция aoBindToThread)
ACTO_API void process_messages();

// Завершить использование библиотеки
ACTO_API void shutdown();

// Инициализировать библиотеку
ACTO_API void startup();


}; // namespace acto

}; // namespace multiprog


#endif // __multiprogs__act_user_h__
