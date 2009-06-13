
#ifndef message_h_EA18085F0CF7480fA57FBF6C0E610820
#define message_h_EA18085F0CF7480fA57FBF6C0E610820

#include <map>
#include <string>

#include <system/platform.h>

namespace acto {

namespace core {

// Идентификатор типов
typedef long    TYPEID;


/** Метаинформация о сообщении */
struct msg_info_t {
    TYPEID      id;
};


/** Базовый тип для сообщений */
struct ACTO_API msg_t {
    const msg_info_t* const metainfo;

public:
    msg_t() : metainfo(NULL) { }
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
    section_t           m_cs;
    // Генератор идентификаторов
    volatile TYPEID     m_counter;
    // Типы сообщений
    Types               m_types;
};


/** */
template <typename T>
class type_box_t {
public:
    // Оборачиваемый тип
    typedef T           type_type;
    // Тип идентификатора
    typedef TYPEID      value_type;

public:
    type_box_t();
    type_box_t(const type_box_t& rhs);

    bool operator == (const value_type& rhs) const;

    template <typename U>
        bool operator == (const type_box_t< U >& rhs) const;

    // Преобразование к типу идентификатора
    operator value_type () const {
        return m_id;
    }

private:
    // Уникальный идентификатор типа
    const value_type    m_id;
};


/** */
template <typename MsgT>
class message_class_t {
    msg_info_t  m_info;

public:
    message_class_t() {
        m_info.id = type_box_t< MsgT >();
    }

    MsgT* create() const {
        MsgT* const result = new MsgT();

        const_cast<const msg_info_t*>(result->metainfo) = &m_info;

        return result;
    }
};



///////////////////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
template <typename T>
	type_box_t< T >::type_box_t() :
		m_id( message_map_t::instance()->get_typeid(typeid(T).raw_name()) )
	{
	}
//-----------------------------------------------------------------------------
template <typename T>
	type_box_t< T >::type_box_t(const type_box_t& rhs) :
		m_id( rhs.m_id )
	{
	}
//-----------------------------------------------------------------------------
template <typename T>
	bool type_box_t< T >::operator == (const value_type& rhs) const {
		return (m_id == rhs);
	}
//-----------------------------------------------------------------------------
template <typename T>
template <typename U>
	bool type_box_t< T >::operator == (const type_box_t< U >& rhs) const {
		return (m_id == rhs.m_id);
	}


} // namespace core

} // namespace acto

#endif // message_h_EA18085F0CF7480fA57FBF6C0E610820
