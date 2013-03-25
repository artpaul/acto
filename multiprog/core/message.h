#pragma once

#include "serialization.h"

#include <util/platform.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <typeinfo>
#include <unordered_map>

namespace acto {

/// Идентификатор типов
typedef unsigned long   TYPEID;


/**
 * Метакласс классов сообщений
 */
struct msg_metaclass_t {
    typedef msg_t* (*instance_constructor)();

    TYPEID                  tid;
    instance_constructor    make_instance;
    std::unique_ptr<serializer_t>
                            serializer;
};


/**
 * Базовый тип для сообщений
 */
struct msg_t {
    msg_metaclass_t*    meta;

public:
    msg_t()
        : meta(NULL)
    { }

    virtual ~msg_t() { }
};


namespace core {

/**
 * Таблица классов сообщений
 */
class message_map_t {
public:
    /** */
    class dumy_serializer_t : public serializer_t {
    public:
        virtual void read(msg_t* const msg, stream_t* const s) {
            // -
        }

        virtual void write(const msg_t* const msg, stream_t* const s) {
            // -
        }
    };

    message_map_t();
    ~message_map_t();

    static message_map_t* instance();

public:
    /// Найти метакласс по его идентификатору
    /// \return Если класса не существует, то возвращается 0
    const msg_metaclass_t* find_metaclass(const TYPEID tid) const;

    /// Получить уникальный идентификатор типа для сообщения по его имени
    inline TYPEID  get_typeid(const size_t code) {
        return this->get_metaclass< dumy_serializer_t >(code)->tid;
    }

    /// -
    inline msg_metaclass_t* get_metaclass(const size_t code) {
        return this->get_metaclass< dumy_serializer_t >(code);
    }

    /// -
    template <typename Serializer>
    msg_metaclass_t* get_metaclass(const size_t code) {
        std::lock_guard<std::mutex> g(m_cs);
        const Types::const_iterator i = m_types.find(code);

        if (i != m_types.end()) {
            return i->second;
        }

        msg_metaclass_t* const meta = new msg_metaclass_t();

        meta->tid = ++m_counter;
        meta->serializer.reset(new Serializer());

        m_types.insert(std::make_pair(code, meta));

        return meta;
    }

private:
    // Тип множества зарегистрированных типов сообщений
    typedef std::unordered_map< size_t, msg_metaclass_t* > Types;

    mutable std::mutex  m_cs;
    /// Генератор идентификаторов
    std::atomic<unsigned long>
                        m_counter;
    /// Типы сообщений
    Types               m_types;
};

///
template <typename MsgT>
inline TYPEID get_message_type() {
    return message_map_t::instance()->get_typeid(typeid(MsgT).hash_code());
}
///
template <typename MsgT>
inline msg_metaclass_t* get_metaclass() {
    return message_map_t::instance()->get_metaclass(typeid(MsgT).hash_code());
}

/** */
template <typename T>
class msg_box_t {
    T* const    m_msg;
public:
    explicit msg_box_t(T* val)
        : m_msg(val)
    { }

    inline T* operator * () const throw () {
        return m_msg;
    }
};


/** */
template <
    typename MsgT,
    typename Serializer = message_map_t::dumy_serializer_t
>
class message_class_t {
public:
    message_class_t()
        : m_meta(message_map_t::instance()->get_metaclass< Serializer >(typeid(MsgT).hash_code()))
    {
        if (m_meta->make_instance == NULL)
            m_meta->make_instance = &message_class_t::instance_constructor;
    }

    template <typename ... P>
    inline msg_box_t<MsgT> create(P&& ... p) const {
        MsgT* const msg = new MsgT(std::forward<P>(p) ... );

        msg->meta = m_meta;

        return msg_box_t<MsgT>(msg);
    }

private:
    static msg_t* instance_constructor() {
        msg_t* const result = new MsgT();
        result->meta = message_map_t::instance()->get_metaclass(typeid(MsgT).hash_code());
        return result;
    }

private:
    msg_metaclass_t* const  m_meta;
};

} // namespace core
} // namespace acto
