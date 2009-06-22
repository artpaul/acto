
#ifndef message_h_EA18085F0CF7480fA57FBF6C0E610820
#define message_h_EA18085F0CF7480fA57FBF6C0E610820

#include <map>
#include <string>
#include <typeinfo>

#include <system/platform.h>
#include <system/atomic.h>
#include <system/mutex.h>


namespace acto {

namespace core {

// Идентификатор типов
typedef long    TYPEID;

/** Базовый тип для сообщений */
struct ACTO_API msg_t {
    TYPEID    tid;

public:
    msg_t() : tid(0) { }
    virtual ~msg_t() { }
};

/** */
class message_map_t {
    // Тип множества зарегистрированных типов сообщений
    typedef std::map< std::string, TYPEID > Types;

public:
    message_map_t();

    static message_map_t* instance() {
        static message_map_t value;

        return &value;
    }

public:
    TYPEID  get_typeid(const char* const type_name);

private:
    // Критическая секция для доступа к полям
    mutex_t             m_cs;
    // Генератор идентификаторов
    volatile TYPEID     m_counter;
    // Типы сообщений
    Types               m_types;
};


/** */
template <typename T>
class type_box_t {
    // Уникальный идентификатор типа
    const TYPEID        m_id;

public:
    // Оборачиваемый тип
    typedef T           type_type;
    // Тип идентификатора
    typedef TYPEID      value_type;

public:
    type_box_t()
        : m_id( message_map_t::instance()->get_typeid(typeid(T).name()) )
    {
    }

    type_box_t(const type_box_t& rhs)
        : m_id( rhs.m_id )
    {
    }

    bool operator == (const value_type& rhs) const {
        return (m_id == rhs);
    }

    template <typename U>
        bool operator == (const type_box_t< U >& rhs) const {
            return (m_id == rhs.m_id);
        }

    // Преобразование к типу идентификатора
    operator value_type () const {
        return m_id;
    }
};


/** */
template <typename T>
class msg_box_t {
    T*  m_msg;
public:
    explicit msg_box_t(T* val) : m_msg(val) { }

    inline T* operator * () const {
        return m_msg;
    }
};


/** */
template <typename MsgT>
class message_class_t {
    TYPEID  m_tid;

    inline msg_box_t< MsgT > _assign_info(MsgT* const msg) const {
        msg->tid = m_tid;
        return msg_box_t< MsgT >(msg);
    }

public:
    message_class_t() {
        m_tid = type_box_t< MsgT >();
    }

    msg_box_t< MsgT > create() const {
        return _assign_info(new MsgT());
    }

    template <typename P1>
        msg_box_t< MsgT > create(P1 p1) const {
            return _assign_info(new MsgT(p1));
        }

    template <typename P1, typename P2>
        msg_box_t< MsgT > create(P1 p1, P2 p2) const {
            return _assign_info(new MsgT(p1, p2));
        }

    template <typename P1, typename P2, typename P3>
        msg_box_t< MsgT > create(P1 p1, P2 p2, P3 p3) const {
            return _assign_info(new MsgT(p1, p2, p3));
        }

    template <typename P1, typename P2, typename P3, typename P4>
        msg_box_t< MsgT > create(P1 p1, P2 p2, P3 p3, P4 p4) const {
            return _assign_info(new MsgT(p1, p2, p3, p4));
        }

    template <typename P1, typename P2, typename P3, typename P4, typename P5>
        msg_box_t< MsgT > create(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5) const {
            return _assign_info(new MsgT(p1, p2, p3, p4, p5));
        }
};

} // namespace core

} // namespace acto

#endif // message_h_EA18085F0CF7480fA57FBF6C0E610820
