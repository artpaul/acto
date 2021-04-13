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
# include <conio.h>
#endif

// Кол-во игроков в игре
static const size_t LISTENERS  = 30;

///////////////////////////////////////////////////////////////////////////////
//                       ОПИСАНИЕ ТИПОВ СООБЩЕНИЙ                            //
///////////////////////////////////////////////////////////////////////////////

// -
struct msg_complete {
  size_t  cycles;
};

// -
struct msg_loop
{ };

// -
struct msg_start
{ };

// -
struct msg_stop
{ };

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
    handler< msg_loop  >(&Listener::doLoop);
    handler< msg_start >(&Listener::doStart);
    handler< msg_stop  >(&Listener::doStop);
  }

public:
  void doLoop(acto::actor_ref, const msg_loop&) {
    if (m_active) {
      m_counter++;
      // Продолжить цикл
      self().send(msg_loop());
    }
  }

  void doStart(acto::actor_ref, const msg_start&) {
    m_active  = true;
    // Начать цикл
    self().send(msg_loop());
  }

  void doStop(acto::actor_ref, const msg_stop&) {
    m_active = false;
    // -
    msg_complete    rval;
    rval.cycles = m_counter;
    // Послать владельцу свою статистику выполнения
    context().send(rval);
    // -
    this->die();
  }

private:
  bool m_active;
  size_t m_counter;
};


// Desc:
//    Управляющий объект. Инициализирует "слушателей"
//    и собирает статистику их работы.
class Analizer : public acto::actor {
public:
  Analizer() {
    handler< msg_complete >(&Analizer::doComplete);

    handler< msg_start >   (&Analizer::doStart);
    handler< msg_stop  >   (&Analizer::doStop);
  }

  ~Analizer() {
    for (size_t i = 0; i < m_listeners.size(); i++)
      acto::destroy(m_listeners[i]);
  }

private:
  void doComplete(acto::actor_ref, const msg_complete& msg) {
    std::cout << msg.cycles << std::endl;
  }

  void doStart(acto::actor_ref, const msg_start&) {
    for (size_t i = 0; i < LISTENERS; i++) {
      acto::actor_ref listener = acto::spawn<Listener>(self());

      listener.send(msg_start());
      m_listeners.push_back(listener);
    }
  }

  void doStop(acto::actor_ref, const msg_stop&) {
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
