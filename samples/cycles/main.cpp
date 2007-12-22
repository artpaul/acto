
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


// ��� ������������� ���������� ���������� ����������
// ������ ���� ���� ���� 
#include <multiprog.h>

#include <iostream>
#include <vector>
#include <conio.h>


// ���-�� ������� � ����
static const size_t LISTENERS  = 30;

using namespace multiprog;


///////////////////////////////////////////////////////////////////////////////
//                       �������� ����� ���������                            //
///////////////////////////////////////////////////////////////////////////////

// -
struct msg_complete : public acto::msg_t
{
    size_t          cycles;
};

// -
struct msg_loop  : public acto::msg_t { };

// -
struct msg_start : public acto::msg_t { };

// -
struct msg_stop  : public acto::msg_t { };


///////////////////////////////////////////////////////////////////////////////
//                       �������� ����� �������                              //
///////////////////////////////////////////////////////////////////////////////

// Desc:
//    �����, ������� ��������� ���������� ������������ ���������,
//    ������� � ����� ��������� �����-�� ��������.
class Listener : public acto::implementation_t
{
public:
    Listener() :
        m_active(false),
        m_counter(0)
    {
        Handler< msg_loop  >(&Listener::doLoop);
        Handler< msg_start >(&Listener::doStart);
        Handler< msg_stop  >(&Listener::doStop);
    }

public:
    //-------------------------------------------------------------------------
    void doLoop(acto::actor_t& sender, const msg_loop& msg)
    {
        if (m_active) {
            m_counter++;
            // ���������� ����
            self.send(msg_loop());
        }
    }
    //-------------------------------------------------------------------------
    void doStart(acto::actor_t& sender, const msg_start& msg)
    {
        m_active  = true;
        // ������ ����
        self.send(msg_loop());
    }
    //-------------------------------------------------------------------------
    void doStop(acto::actor_t& sender, const msg_stop& msg)
    {
        m_active = false;
        // -
        msg_complete    rval;
        rval.cycles = m_counter;
        // ������� ��������� ���� ���������� ����������
        context.send(rval);
    }

private:
    bool        m_active;
    size_t      m_counter;
};


// Desc: 
//    ����������� ������. �������������� "����������"
//    � �������� ���������� �� ������.
class Analizer : public acto::implementation_t
{
public:
    Analizer()
    {
        Handler< msg_complete >(&Analizer::doComplete);
        // -
        Handler< msg_start >   (&Analizer::doStart);
        Handler< msg_stop  >   (&Analizer::doStop);
    }

    ~Analizer()
    {
        for (size_t i = 0; i < m_listeners.size(); i++)
            acto::destroy(m_listeners[i]);
    }

private:
    //-------------------------------------------------------------------------
    void doComplete(acto::actor_t& sender, const msg_complete& msg)
    {
        std::cout << msg.cycles << std::endl;
    }
    //-------------------------------------------------------------------------
    void doStart(acto::actor_t& sender, const msg_start& msg)
    {
        for (size_t i = 0; i < LISTENERS; i++) {
            // -
            acto::actor_t   actor = acto::instance_t< Listener >(self);
            // -
            actor.send(msg_start());
            // -
            m_listeners.push_back(actor);
        }
    }
    //-------------------------------------------------------------------------
    void doStop(acto::actor_t& sender, const msg_stop& msg)
    {
        for (size_t i = 0; i < m_listeners.size(); i++)
            m_listeners[i].send(msg_stop());
    }

private:
    std::vector< acto::actor_t > m_listeners;
};


//-----------------------------------------------------------------------------
int main()
{
    // ���������������� ����������.
    acto::startup();
    {
        // -
        acto::actor_t   analizer = acto::instance_t< Analizer >(acto::aoExclusive);
        // -
        std::cout << "Statistic is being collected." << std::endl;
        std::cout << "Please wait some seconds and when press a key." << std::endl;
        // ��������� ���������� �������
        analizer.send(msg_start());
        // -
        std::cout << ":key" << std::endl;
        _getch();
        // ������������ ���������� � ������� ����������
        analizer.send(msg_stop());
        // -
        system::Sleep(2 * 1000);
        // -
        acto::destroy(analizer);
    }
    // ���������� �������
    acto::shutdown();
    
    std::cout << ":end" << std::endl;
    _getch();
    // -
    return 0;
}
