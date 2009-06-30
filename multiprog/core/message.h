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

#ifndef message_h_EA18085F0CF7480fA57FBF6C0E610820
#define message_h_EA18085F0CF7480fA57FBF6C0E610820

#include <map>
#include <string>
#include <typeinfo>

#include <system/platform.h>
#include <system/atomic.h>
#include <system/mutex.h>

#include "serialization.h"

namespace acto {

namespace core {

namespace detail {

/** */
class dumy_serializer_t : public serializer_t {
};

} // namespace detail


// Идентификатор типов
typedef long    TYPEID;


/**
 * Метакласс классов сообщений
 */
struct msg_metaclass_t {
    TYPEID                      tid;
    std::auto_ptr<serializer_t> serializer;
};


/**
 * Базовый тип для сообщений
 */
struct msg_t {
    msg_metaclass_t*    meta;

public:
    msg_t() : meta(NULL) { }
    virtual ~msg_t() { }
};


/**
 * Таблица классов сообщений
 */
class message_map_t {
    // Тип множества зарегистрированных типов сообщений
    typedef std::map< std::string, msg_metaclass_t* > Types;

public:
    message_map_t();
    ~message_map_t();

    static message_map_t* instance() {
        static message_map_t value;

        return &value;
    }

public:
    /// Получить уникальный идентификатор типа для сообщения по его имени
    inline TYPEID  get_typeid(const char* const type_name) {
        return this->get_metaclass< detail::dumy_serializer_t >(type_name)->tid;
    }
    /// -
    inline msg_metaclass_t* get_metaclass(const char* const type_name) {
        return this->get_metaclass< detail::dumy_serializer_t >(type_name);
    }
    /// -
    template <typename Serializer>
    msg_metaclass_t* get_metaclass(const char* const type_name) {
        std::string name(type_name);
        MutexLocker lock(m_cs);

        // Найти этот тип
        Types::const_iterator i = m_types.find(name);
        // -
        if (i != m_types.end())
            return (*i).second;
        else {
            msg_metaclass_t* const meta = new msg_metaclass_t();

            meta->tid = atomic_increment(&m_counter);
            meta->serializer.reset(new Serializer());

            m_types[name] = meta;
            // -
            return meta;
        }
    }

private:
    /// Критическая секция для доступа к полям
    mutex_t             m_cs;
    /// Генератор идентификаторов
    volatile TYPEID     m_counter;
    /// Типы сообщений
    Types               m_types;
};

///
template <typename MsgT>
inline TYPEID get_message_type() {
    return message_map_t::instance()->get_typeid(typeid(MsgT).name());
}
///
template <typename MsgT>
inline msg_metaclass_t* get_metaclass() {
    return message_map_t::instance()->get_metaclass(typeid(MsgT).name());
}

/** */
template <typename T>
class msg_box_t {
    T* const    m_msg;
public:
    explicit msg_box_t(T* val) : m_msg(val) { }

    inline T* operator * () const {
        return m_msg;
    }
};


/** */
template <typename MsgT, typename Serializer = detail::dumy_serializer_t >
class message_class_t {
    msg_metaclass_t* const  m_meta;

    inline msg_box_t< MsgT > _assign_info(MsgT* const msg) const {
        msg->meta = m_meta;
        return msg_box_t< MsgT >(msg);
    }

public:
    message_class_t() 
        : m_meta(message_map_t::instance()->get_metaclass< Serializer >(typeid(MsgT).name()))
    {
        // -
    }

    inline msg_box_t< MsgT > create() const {
        return _assign_info(new MsgT());
    }

    template <typename P1>
    inline msg_box_t< MsgT > create(P1 p1) const {
        return _assign_info(new MsgT(p1));
    }

    template <typename P1, typename P2>
    inline msg_box_t< MsgT > create(P1 p1, P2 p2) const {
        return _assign_info(new MsgT(p1, p2));
    }

    template <typename P1, typename P2, typename P3>
    inline msg_box_t< MsgT > create(P1 p1, P2 p2, P3 p3) const {
        return _assign_info(new MsgT(p1, p2, p3));
    }

    template <typename P1, typename P2, typename P3, typename P4>
    inline msg_box_t< MsgT > create(P1 p1, P2 p2, P3 p3, P4 p4) const {
        return _assign_info(new MsgT(p1, p2, p3, p4));
    }

    template <typename P1, typename P2, typename P3, typename P4, typename P5>
    inline msg_box_t< MsgT > create(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5) const {
        return _assign_info(new MsgT(p1, p2, p3, p4, p5));
    }
};

} // namespace core

} // namespace acto

#endif // message_h_EA18085F0CF7480fA57FBF6C0E610820
