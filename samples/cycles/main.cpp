
///////////////////////////////////////////////////////////////////////////////
// Desc:                                                                     //
//    ������ ������ ������������ ��� ������������ round-robin ���������      //
//    ��������� �������� ������� ������� ��� ��������� ���������.            //
//                                                                           //
//    ����������� ������ ������� - Listener. ������ Listener ��������        //
//    ��������� � ������������� ��������� ���� ����������, ����� ����        //
//    �������� ��� ���� �������� ��������� msg_loop �� ��� ���, ���� ���     //
//    �� ����� ���� ������� ���������� ���������� �����. ����� ���������     //
//    ���������� ������ �������� ���-�� ��������, ������� �� ������.         //
//    ������ ���������� ��������� �� ������� ��� ��������� ������ ���������. //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////


#include <iostream>
#include <vector>

// ��� ������������� ���������� ���������� ����������
// ������ ���� ���� ����
#include <acto.h>

#ifdef ACTO_WIN
#   include <conio.h>
#endif

// ���-�� ������� � ����
static const size_t LISTENERS  = 30;

///////////////////////////////////////////////////////////////////////////////
//                       �������� ����� ���������                            //
///////////////////////////////////////////////////////////////////////////////

// -
struct msg_complete : public acto::msg_t {
    size_t  cycles;
};

// -
struct msg_loop  : public acto::msg_t { };

// -
struct msg_start : public acto::msg_t { };

// -
struct msg_stop  : public acto::msg_t { };


acto::message_class_t< msg_loop >   msg_loop_class;


///////////////////////////////////////////////////////////////////////////////
//                       �������� ����� �������                              //
///////////////////////////////////////////////////////////////////////////////

// Desc:
//    �����, ������� ��������� ���������� ������������ ���������,
//    ������� � ����� ��������� �����-�� ��������.
class Listener : public acto::implementation_t {
public:
    Listener() 
        : m_active(false)
        , m_counter(0)
    {
        Handler< msg_loop  >(&Listener::doLoop);
        Handler< msg_start >(&Listener::doStart);
        Handler< msg_stop  >(&Listener::doStop);
    }

public:
    //-------------------------------------------------------------------------
    void doLoop(acto::actor_t& sender, const msg_loop& msg) {
        if (m_active) {
            m_counter++;
            // ���������� ����
            self.send(msg_loop_class.create());
        }
    }
    //-------------------------------------------------------------------------
    void doStart(acto::actor_t& sender, const msg_start& msg) {
        m_active  = true;
        // ������ ����
        self.send(msg_loop_class.create());
    }
    //-------------------------------------------------------------------------
    void doStop(acto::actor_t& sender, const msg_stop& msg) {
        m_active = false;
        // -
        msg_complete    rval;
        rval.cycles = m_counter;
        // ������� ��������� ���� ���������� ����������
        context.send(rval);
        // -
        this->terminate();
    }

private:
    bool        m_active;
    size_t      m_counter;
};


// Desc:
//    ����������� ������. �������������� "����������"
//    � �������� ���������� �� ������.
class Analizer : public acto::implementation_t {
public:
    Analizer() {
        Handler< msg_complete >(&Analizer::doComplete);
        // -
        Handler< msg_start >   (&Analizer::doStart);
        Handler< msg_stop  >   (&Analizer::doStop);
    }

    ~Analizer() {
        for (size_t i = 0; i < m_listeners.size(); i++)
            acto::destroy(m_listeners[i]);
    }

private:
    //-------------------------------------------------------------------------
    void doComplete(acto::actor_t& sender, const msg_complete& msg) {
        std::cout << msg.cycles << std::endl;
    }
    //-------------------------------------------------------------------------
    void doStart(acto::actor_t& sender, const msg_start& msg) {
        for (size_t i = 0; i < LISTENERS; i++) {
            // -
            acto::actor_t   actor = acto::instance< Listener >(self);
            // -
            actor.send(msg_start());
            // -
            m_listeners.push_back(actor);
        }
    }
    //-------------------------------------------------------------------------
    void doStop(acto::actor_t& sender, const msg_stop& msg) {
        for (size_t i = 0; i < m_listeners.size(); i++) {
            m_listeners[i].send(msg_stop());
            // ����� ���������� ������ ������
            acto::join(m_listeners[i]);
        }
        this->terminate();
    }

private:
    std::vector< acto::actor_t > m_listeners;
};


//-----------------------------------------------------------------------------
int main() {
    // ���������������� ����������.
    acto::startup();
    {
        // -
        acto::actor_t   analizer = acto::instance< Analizer >(acto::aoExclusive);
        // -
        std::cout << "Statistic is being collected." << std::endl;
        std::cout << "Please wait some seconds..." << std::endl;
        // ��������� ���������� �������
        analizer.send(msg_start());
        // -
        acto::core::Sleep(5 * 1000);
        // ������������ ���������� � ������� ����������
        analizer.send(msg_stop());
        // -
        acto::join(analizer);
    }
    // ���������� �������
    acto::shutdown();

    std::cout << ":end" << std::endl;

#ifdef ACTO_WIN
    _getch();
#endif

    return 0;
}
