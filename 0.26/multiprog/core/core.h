///////////////////////////////////////////////////////////////////////////////
//                           The act-o Library                               //
//---------------------------------------------------------------------------//
// Copyright © 2007 - 2009                                                   //
//     Pavel A. Artemkin (acto.stan@gmail.com)                               //
// ------------------------------------------------------------------ -------//
// License:                                                                  //
//     Code covered by the MIT License.                                      //
//     The authors make no representations about the suitability of this     //
//     software for any purpose. It is provided "as is" without express or   //
//     implied warranty.                                                     //
///////////////////////////////////////////////////////////////////////////////

#if !defined ( __multiprogs__act_core_h__ )
#define __multiprogs__act_core_h__

#include <typeinfo>

#include <cassert>
#include <algorithm>
#include <exception>
#include <list>
#include <map>
#include <new>
#include <vector>
#include <set>
#include <string>

#include <generic/intrlist.h>
#include <generic/delegates.h>

#include <system/thread.h>
#include <system/event.h>

#include "struct.h"
#include "message.h"

namespace acto {

// -
class actor_t;

// Индивидуальный поток для актера
const int aoExclusive    = 0x01;
// Привязать актера к текущему системному потоку.
// Не имеет эффекта, если используется в контексте потока,
// созданного самой библиотекой.
const int aoBindToThread = 0x02;


namespace core {

// -
class  worker_t;
struct i_handler;
struct object_t;
struct package_t;


// Desc:
struct HandlerItem {
    // Обработчик
    i_handler*          handler;
    // Тип обработчика
    const TYPEID        type;

public:
    HandlerItem(const TYPEID type_) : type( type_ ), handler( 0 ) { }
};


// Карта обработчиков сообщений
typedef std::vector< HandlerItem* >         Handlers;
// Тип очереди сообщений
typedef structs::queue_t< package_t >       MessageQueue;
// -
typedef MutexLocker                         Exclusive;


/**
 * Базовый класс для всех актеров, как внутренних, так и внешних (пользовательских).
 */
class ACTO_API base_t {
    friend void doHandle(package_t* const package);

    // Карта обработчиков сообщений для данного объекта
    Handlers        m_handlers;
    bool            m_terminating;

public:
    struct msg_destroy : public msg_t {
        // Объект, который необходимо удалить
        object_t*   object;
    };

public:
    base_t();
    virtual ~base_t();

protected:
    /// Завершить собственную работу
    void terminate();

    /// Установка обработчика для сообщения данного типа
    template < typename MsgT, typename ClassName >
        inline void Handler( void (ClassName::*func)(acto::actor_t& sender, const MsgT& msg) ) {
            // Тип сообщения
            type_box_t< MsgT >                       a_type     = type_box_t< MsgT >();
            // Метод, обрабатывающий сообщение
            typename handler_t< MsgT >::delegate_t   a_delegate = fastdelegate::MakeDelegate(this, func);
            // Обрабочик
            handler_t< MsgT >* const                 handler    = new handler_t< MsgT >(a_delegate, a_type);

            // Установить обработчик
            set_handler(handler, a_type);
        }
    /// Сброс обработчика для сообщения данного типа
    template < typename MsgT >
        inline void Handler() {
            // Тип сообщения
            type_box_t< MsgT >	a_type = type_box_t< MsgT >();
            // Сбросить обработчик указанного типа
            set_handler(0, a_type);
        }

private:
    void set_handler(i_handler* const handler, const TYPEID type);
};


// Desc:
struct ACTO_API i_handler {
    // Идентификатор типа сообщения
    const TYPEID    m_type;

public:
    i_handler(const TYPEID type_);

public:
    virtual void invoke(object_t* const sender, msg_t* const msg) const = 0;
};


// Desc: Объект
struct ACTO_API object_t : public intrusive_t< object_t > {
    struct waiter_t : public intrusive_t< waiter_t > {
        event_t*    event;
    };

    // Критическая секция для доступа к полям
    mutex_t             cs;

    // Реализация объекта
    base_t*             impl;
    // Поток, в котором должен выполнятся объект
    worker_t* const     thread;
    // Список сигналов для потоков, ожидающих уничтожения объекта
    waiter_t*           waiters;
    // Очередь сообщений, поступивших данному объекту
    MessageQueue        queue;
    // Count of references to object
    volatile long       references;

    // Флаги состояния текущего объекта
    unsigned int        binded    : 1;
    unsigned int        deleting  : 1;
    unsigned int        freeing   : 1;
    unsigned int        scheduled : 1;
    unsigned int        unimpl    : 1;

public:
    object_t(worker_t* const thread_);
};


// Desc: Транспортный пакет для сообщения
struct ACTO_API package_t : public intrusive_t< package_t > {
    // Данные сообщения
    msg_t* const        data;
    // Отправитель сообщения
    object_t*           sender;
    // Получатель сообщения
    object_t*           target;
    // Код типа сообщения
    const TYPEID        type;

public:
    package_t(msg_t* const data_, const TYPEID type_);
    ~package_t();
};


// Desc: Обертка для вызова обработчика сообщения конкретного типа
template <typename MsgT>
class handler_t : public i_handler {
public:
    typedef fastdelegate::FastDelegate< void (acto::actor_t&, const MsgT&) >    delegate_t;

public:
    handler_t(const delegate_t& delegate_, type_box_t< MsgT >& type_)
        : i_handler ( type_ )
        , m_delegate( delegate_ )
    {
    }

    // Вызвать обработчик
    virtual void invoke(object_t* const sender, msg_t* const msg) const {
        acto::actor_t   actor(sender);

        m_delegate(actor, *static_cast< const MsgT* const >(msg));
    }

private:
    // Делегат, хранящий указатель на
    // метод конкретного объекта.
    const delegate_t    m_delegate;
};


///////////////////////////////////////////////////////////////////////////////
// Desc: Данные среды выполнения
class runtime_t {
    struct resources_t {
        // Для контейнера заголовков актеров
        mutex_t     actors;
    };

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
    // Параметры потоков
    workers_t   m_workers;
    event_t     m_evclean;

public:
    runtime_t();
    ~runtime_t();

    static runtime_t* instance();

public:
    // -
    void        acquire(object_t* const obj);
    // Создать экземпляр объекта, связав его с соответсвтующей реализацией
    object_t*   createActor(base_t* const impl, const int options = 0);
    // Уничтожить объект
    void        destroyObject(object_t* const object);
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
    void        cleaner();
    // -
    package_t*  createPackage(object_t* const target, msg_t* const data, const TYPEID type);
    // -
    worker_t*   createWorker();
    // Деструткор для пользовательских объектов (актеров)
    void        destruct_actor(object_t* const actor);
    // Определить отправителя сообщения
    object_t*   determineSender();
    // Цикл выполнения планировщика
    void        execute();
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
    // Экземпляр системного потока
    thread_t*           m_scheduler;
    // -
    event_t             m_event;
    event_t             m_evworker;

    // -
    event_t             m_evnoactors;
    event_t             m_evnoworkers;

    HeaderStack         m_deleted;

/* Защищаемые блокировкой данные */
    // Критическая секция для доступа к полям
    resources_t         m_cs;
    // Текущее множество актеров
    Actors              m_actors;
};


/* Контекс потока */
struct thread_context_t {
    // Список актеров ассоциированных
    // только с текущим потоком
    std::set< object_t* >   actors;
    // Счетчик инициализаций
    int                     counter;
    // -
    bool                    is_core;
    // Текущий активный объект в данном потоке
    object_t*               sender;
};


///////////////////////////////////////////////////////////////////////////////
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

// Инициализация
void initialize();
void initializeThread(const bool isInternal);

// -
void finalize();

void finalizeThread();

void processBindedActors();

}; // namespace core

}; // namespace acto

#endif // __multiprogs__act_core_h__
