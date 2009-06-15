
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

// ������������� �����
typedef long    TYPEID;

/** ������� ��� ��� ��������� */
struct ACTO_API msg_t {
    TYPEID    tid;

public:
    msg_t() : tid(0) { }
    virtual ~msg_t() { }
};

/** */
class message_map_t {
    // ��� ��������� ������������������ ����� ���������
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
    // ����������� ������ ��� ������� � �����
    mutex_t             m_cs;
    // ��������� ���������������
    volatile TYPEID     m_counter;
    // ���� ���������
    Types               m_types;
};


/** */
template <typename T>
class type_box_t {
public:
    // ������������� ���
    typedef T           type_type;
    // ��� ��������������
    typedef TYPEID      value_type;

public:
    type_box_t();
    type_box_t(const type_box_t& rhs);

    bool operator == (const value_type& rhs) const;

    template <typename U>
        bool operator == (const type_box_t< U >& rhs) const;

    // �������������� � ���� ��������������
    operator value_type () const {
        return m_id;
    }

private:
    // ���������� ������������� ����
    const value_type    m_id;
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



///////////////////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
template <typename T>
	type_box_t< T >::type_box_t() :
		m_id( message_map_t::instance()->get_typeid(typeid(T).name()) )
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
