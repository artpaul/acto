
#ifndef acto_runtime_h_9789b1fc3b144e629327ce3279b9ee89
#define acto_runtime_h_9789b1fc3b144e629327ce3279b9ee89

#include <set>

#include "struct.h"

namespace acto {

namespace core {

class  base_t;
class  worker_t;
struct object_t;
struct package_t;


/**
 * Данные среды выполнения
 */
class runtime_t {
    // Тип множества актеров
    typedef std::set< object_t* >           Actors;
    // -
    typedef structs::queue_t< object_t >    HeaderQueue;
    // Стек заголовков объектов
    typedef structs::stack_t< object_t >    HeaderStack;
    // Очередь вычислителей
    typedef structs::queue_t< worker_t >    WorkerQueue;
    // -
    typedef structs::stack_t< worker_t >    WorkerStack;

    //
    struct workers_t {
        // Текущее кол-во потоков
        atomic_t        count;
        // Текущее кол-во удаляемых потоков
        atomic_t        deleting;
        // Текущее кол-во эксклюзивных потоков
        atomic_t        reserved;
        // Удаленные потоки
        WorkerStack     deleted;
        // Свободные потоки
        WorkerStack     idle;
    };

    // Максимальное кол-во рабочих потоков в системе
    static const unsigned int MAX_WORKERS = 512;

public:
/* Общие для всех потоков данные */
   // Очередь объектов, которым пришли сообщения
    structs::queue_t< object_t > m_queue;

public:
    runtime_t();
    ~runtime_t();

    static runtime_t* instance();

public:
    // -
    void        acquire(object_t* const obj);
    // Создать экземпляр объекта, связав его с соответсвтующей реализацией
    object_t*   create_actor(base_t* const impl, const int options = 0);
    // Уничтожить объект
    void        destroy_object(object_t* const object);
    // -
    void        join(object_t* const obj);
    // -
    long        release(object_t* const obj);
    // Послать сообщение указанному объекту
    void        send(object_t* const target, msg_t* const msg, const TYPEID type);
    // Завершить выполнение
    void        shutdown();
    // Начать выполнение
    void        startup();

private:
    // Цикл выполнения планировщика
    void        cleaner(void*);
    // -
    package_t*  create_package(object_t* const target, msg_t* const data, const TYPEID type);
    // -
    worker_t*   create_worker();
    // Деструткор для пользовательских объектов (актеров)
    void        destruct_actor(object_t* const actor);
    // Определить отправителя сообщения
    object_t*   determine_sender();
    // Цикл выполнения планировщика
    void        execute(void*);
    // -
    void        pushDelete(object_t* const obj);
    // -
    void        pushIdle(worker_t* const worker);

private:
/* Внутренние данные планировщика*/
    volatile bool       m_active;
    volatile bool       m_terminating;
    // Количество физических процессоров (ядер) в системе
    long                m_processors;
    // Экземпляр GC потока
    thread_t*           m_cleaner;
    // -
    HeaderStack         m_deleted;
    // Экземпляр системного потока
    thread_t*           m_scheduler;
    // -
    event_t             m_evclean;
    event_t             m_event;
    event_t             m_evworker;
    // -
    event_t             m_evnoactors;
    event_t             m_evnoworkers;
    // Параметры потоков
    workers_t           m_workers;

/* Защищаемые блокировкой данные */
    // Критическая секция для доступа к полям
    mutex_t             m_cs;
    // Текущее множество актеров
    Actors              m_actors;
};

} // namepsace core

} // namespace acto

#endif // acto_runtime_h_9789b1fc3b144e629327ce3279b9ee89

