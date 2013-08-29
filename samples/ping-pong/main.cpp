///////////////////////////////////////////////////////////////////////////////////////////////////
// Desc:                                                                                         //
//    Данный пример предназначен для тестирования производительности алгоритмов                  //
//    диспетчеризации сообщений. Он также показывает основные принципы использования библиотеки. //
//                                                                                               //
//    Модель программы: стена - мячи - игроки.                                                   //
//    Стена и игроки представлены актерами, а мячи - сообщениями, которыми обмениваются          //
//    объект-стена и объекты-актеры. Программа начинается с того, что объект-стена               //
//    получает сообщение msg_start, во время обработки которого она осуществляет начальную       //
//    рассылку сообщений msg_ball игрокам. Каждый игрок, получив это сообщение, посылает         //
//    ответное сообщение того же типа стене. Стена, получив сообщение msg_ball, увеличивает      //
//    внутренний счетчик на единицу и пересылает сообщение-мяч произвольно выбранному игроку.    //
//                                                                                               //
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <cstdlib>
#include <iostream>

// Исключить компоненты сетевого взаимодействия
#define ACTO_EXCLUDE_REMOTE

// Для использование библиотеки достаточно подключить
// только один этот файл
#include <acto.h>

#ifdef ACTO_WIN
#   include <conio.h>
#else
#   include <stdio.h>
#endif

// Кол-во мячей в игре
static const int BALLS    = 1  * 1000;
// Продолжительность игры
static const int DURATION = 2  * 1000;
// Кол-во игроков в игре
static const int PLAYERS  = 10 * 1000;


///////////////////////////////////////////////////////////////////////////////
//                       ОПИСАНИЕ ТИПОВ СООБЩЕНИЙ                            //
///////////////////////////////////////////////////////////////////////////////


// Начать игру
struct msg_start {
    // Кол-во мячей в игре
    int             balls;
    acto::actor_ref console;

public:
    msg_start(int balls_, acto::actor_ref& console_)
        : balls( balls_ )
        , console( console_ )
    { }
};


// Завершить игру
struct msg_finish
{ };


// Послать мяч
struct msg_ball
{ };


// Вывести текст на консоль
struct msg_out {
    std::string     text;

public:
    msg_out(const std::string& text_)
        : text( text_ )
    { }
};

///////////////////////////////////////////////////////////////////////////////
//                       ОПИСАНИЕ ТИПОВ АКТЕРОВ                              //
///////////////////////////////////////////////////////////////////////////////


// Desc: Консоль для вывода текстовых сообщений.
//       Инкапсулирует системную консоль для того, чтобы небыло
//       необходимости использовать примитивы синхронизации при
//       выводе из разных потоков.
class Console : public acto::actor {
public:
    Console() {
        // Метод Handler связывает конкретную процедуру с библиотекой
        // для обработки сообщения указанного типа.
        Handler< msg_out >( [] (acto::actor_ref& sender, const msg_out& msg)
            { std::cout << msg.text << std::endl; }
        );


        // Во время работы программы можно сменить обработчик просто указав другой метод.
        // Ex:
        //    Handler< msg_out >( &Console::another_proc );
        //
        //    Handler< msg_out >( [] (acto::actor_ref& sender, const msg_out& msg)
        //        { ; }
        //    );
        // Для того, чтобы отключить обработку указанного сообщения необходимо вызвать
        // метод Handler без параметров.
        // Ex:
        //    Handler< msg_out >();
        //
        // Метод Handler является защищенным методом класса act_o::actor.
        // Это означает, что его невозможно использовать во вне класса-актера.
        // Также это означает, что Handler устанавливает обработчики только
        // для объектов данного класса и никакого другого.
    }
};


// Desc: Отбивает мяч
class Player : public acto::actor {
private:
    void do_ball(acto::actor_ref& sender, const msg_ball& msg) {
        // Отправить мяч обратно
        sender.send(msg_ball());
    }

public:
    Player() {
        Handler< msg_ball >( &Player::do_ball );
    }
};


// Desc: Мяч отскакивает одному из игроков
class Wall : public acto::actor {
    // Консоль
    acto::actor_ref m_console;
    // Множество игроков
    acto::actor_ref m_players[ PLAYERS ];
    // Счетчик отскоков мячей от стены
    long long       m_counter;
    // Признак окончания игры
    bool            m_finished;

public:
    Wall()
        : m_counter ( 0 )
        , m_finished( false )
    {
        Handler< msg_ball   >( &Wall::do_ball   );
        Handler< msg_finish >( &Wall::do_finish );
        Handler< msg_start  >( &Wall::do_start  );

        // Инициализация игроков
        for (int i = 0; i < PLAYERS; i++)
            m_players[i] = acto::spawn< Player >();
    }

    ~Wall() {
        // Необходимо явно вызвать функцию удаления для каждого созданного объекта
        for (int i = 0; i < PLAYERS; i++)
            acto::destroy(m_players[i]);
    }

private:
    void do_ball(acto::actor_ref& sender, const msg_ball& msg) {
        if (!m_finished) {
            // Увеличить счетчик отскоков от стены
            m_counter++;
            // Послать случайно выбранному игроку
            m_players[ (rand() % PLAYERS) ].send(msg_ball());
        }
    }

    void do_finish(acto::actor_ref& sender, const msg_finish& msg) {
        m_finished = true;
        // -
        char    buffer[255];

        sprintf(buffer, "Counter : %d", (int)m_counter);
        // Отправить сообщение на консоль
        m_console.send< msg_out >(std::string(buffer));
        // Завершить выполнение текущего актера
        this->die();
    }

    void do_start(acto::actor_ref& sender, const msg_start& msg) {
        m_console = msg.console;

        // Послать мячи в игру
        for (int i = 0; i < msg.balls; i++)
            m_players[ (rand() % PLAYERS) ].send(msg_ball());
    }
};


//-------------------------------------------------------------------------------------------------
int main() {
    std::cout << "Balls    : " << BALLS    << std::endl;
    std::cout << "Duration : " << DURATION << std::endl;
    std::cout << "Players  : " << PLAYERS  << std::endl;
    std::cout << std::endl;

    for (unsigned int i = 1; i < 4; i++) {
        std::cout << "--- " << i << " : start acto ---" << std::endl;
        // Инициализировать библиотеку.
        // Данную функцию необходимо вызывать обязательно, так как иначе
        // поведение бибилиотеки будет непредсказуемым, но скорее всего она просто
        // не будет функционировать.
        acto::startup();
        {
            // Создать консоль.
            // Все актеры должны создаваться с использованием шаблона act_o::spawn<>.
            // Использование оператора new недопустимо.
            acto::actor_ref console = acto::spawn< Console >(acto::aoBindToThread);

            for(unsigned int j = 0; j < 3; j++) {
                // Создать стену.
                // Опция "acto::aoExclusive" создает для объекта отдельный поток,
                // в котором будут выполнятся все обработчики этого объекта
                acto::actor_ref wall = acto::spawn< Wall >();

                // -
                console.send< msg_out >("send start");

                // Начать игру: инициализировать объект, запустить мячи
                wall.send< msg_start >(BALLS, console);

                // Игра продолжается некоторое время в независимых потоках
                acto::core::sleep(DURATION);

                // Остановить игру
                wall.send(msg_finish());

                // Дождаться завершения выполнения
                acto::join(wall);

                // Обработать сообщения для консоли
                acto::process_messages();
            }
            acto::core::sleep(1000);
        }
        // По зовершении работы библиотеки необходимо вызвать эту функцию,
        // чтобы освободить все занимаемые ресурсы.
        acto::shutdown();
        // -
        std::cout << "--- end acto ---" << std::endl;
    }

    // Выполнение программы закончено
    std::cout << ": end" << std::endl;
#ifdef ACTO_WIN
    _getch();
#endif
    // -
    return 0;
}
