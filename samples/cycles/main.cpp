///////////////////////////////////////////////////////////////////////////////
// Desc:                                                                     //
//    Данный пример предназначен для тестирования round-robin алгоритма      //
//    выделения объектам квантов времени для обработки сообщений.            //
//                                                                           //
//    Центральный объект примера - Listener. Объект Listener получает        //
//    сообщение о необходимости запустить цикл выполнения, после чего        //
//    начинает сам себе посылать сообщение msg_loop до тех пор, пока ему     //
//    не будет дана команда остановить выполнение цикла. После остановки     //
//    выполнения объект посылает кол-во итераций, которые он сделал.         //
//    Данное количество выводится на консоль как результат работы программы. //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////


#include <iostream>
#include <vector>

// Для использование библиотеки достаточно подключить
// только один этот файл
#include <acto.h>

#ifdef ACTO_WIN
#   include <conio.h>
#endif

// Кол-во игроков в игре
static const size_t LISTENERS  = 30;

///////////////////////////////////////////////////////////////////////////////
//                       ОПИСАНИЕ ТИПОВ СООБЩЕНИЙ                            //
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
//                       ОПИСАНИЕ ТИПОВ АКТЕРОВ                              //
///////////////////////////////////////////////////////////////////////////////

// Desc:
//    Актер, который эмитирует некоторого абстрактного слушателя,
//    который в цикле проверяет какие-то значения.
class Listener : public acto::actor {
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
    void doLoop(acto::actor_ref& sender, const msg_loop& msg) {
        if (m_active) {
            m_counter++;
            // Продолжить цикл
            self.send(msg_loop_class.create());
        }
    }

    void doStart(acto::actor_ref& sender, const msg_start& msg) {
        m_active  = true;
        // Начать цикл
        self.send(msg_loop_class.create());
    }

    void doStop(acto::actor_ref& sender, const msg_stop& msg) {
        m_active = false;
        // -
        msg_complete    rval;
        rval.cycles = m_counter;
        // Послать владельцу свою статистику выполнения
        context.send(rval);
        // -
        this->die();
    }

private:
    bool        m_active;
    size_t      m_counter;
};


// Desc:
//    Управляющий объект. Инициализирует "слушателей"
//    и собирает статистику их работы.
class Analizer : public acto::actor {
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
    void doComplete(acto::actor_ref& sender, const msg_complete& msg) {
        std::cout << msg.cycles << std::endl;
    }

    void doStart(acto::actor_ref& sender, const msg_start& msg) {
        for (size_t i = 0; i < LISTENERS; i++) {
            acto::actor_ref actor = acto::spawn< Listener >(self);

            actor.send(msg_start());
            m_listeners.push_back(actor);
        }
    }

    void doStop(acto::actor_ref& sender, const msg_stop& msg) {
        for (size_t i = 0; i < m_listeners.size(); i++) {
            m_listeners[i].send(msg_stop());
            // Ждать завершения работы агента
            acto::join(m_listeners[i]);
        }
        this->die();
    }

private:
    std::vector<acto::actor_ref> m_listeners;
};


//-----------------------------------------------------------------------------
int main() {
    // Инициализировать библиотеку.
    acto::startup();
    {
        // -
        acto::actor_ref analizer = acto::spawn< Analizer >(acto::aoExclusive);
        // -
        std::cout << "Statistic is being collected." << std::endl;
        std::cout << "Please wait some seconds..." << std::endl;
        // Запустить выполнение примера
        analizer.send(msg_start());

        acto::core::sleep(5 * 1000);
        // Оставноваить выполнение и собрать статистику
        analizer.send(msg_stop());

        acto::join(analizer);
    }
    // Освободить ресурсы
    acto::shutdown();

    std::cout << ":end" << std::endl;

#ifdef ACTO_WIN
    _getch();
#endif

    return 0;
}
