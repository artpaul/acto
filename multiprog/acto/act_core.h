///////////////////////////////////////////////////////////////////////////////////////////////////
//                                     The act_o Library                                         //
//                                                                                               //
//-----------------------------------------------------------------------------------------------//
// Copyright © 2007                                                                              //
//     Pavel A. Artemkin (acto.stan@gmail.com)                                                   //
// ----------------------------------------------------------------------------------------------//
// License:                                                                                      //
//     Code covered by the MIT License.                                                          //
//     The authors make no representations about the suitability of this software                //
//     for any purpose. It is provided "as is" without express or implied warranty.              //
//-----------------------------------------------------------------------------------------------//
// File: act_core.h                                                                              //
//     Ядро библиотеки.                                                                          //
///////////////////////////////////////////////////////////////////////////////////////////////////

#if !defined ( __multiprogs__act_core_h__ )
#define __multiprogs__act_core_h__


namespace multiprog
{

namespace acto
{

namespace core
{

// -
class worker_t;


// Desc: 
struct HandlerItem
{
	// Обработчик
	i_handler*		handler;
	// Тип обработчика
	const TYPEID	type;

public:
	HandlerItem(const TYPEID type_) : type( type_ ), handler( 0 ) { } 
};


// Desc:
struct LinkItem
{
	typedef fastdelegate::FastDelegate< void (const acto::actor_t&) >	Notifier;

	// -
	object_t*		linked;
	// -
	Notifier		entry;
};


// Desc: 
template <typename T>
class type_box_t
{
public:
	// Оборачиваемый тип
	typedef T		type_type;
	// Тип идентификатора
	typedef TYPEID	value_type;

public:
	type_box_t();
	type_box_t(const type_box_t& rhs);

public:
	bool operator == (const value_type& rhs) const;

	template <typename U>
		bool operator == (const type_box_t< U >& rhs) const;

	// Преобразование к типу идентификатора
	operator value_type () const
	{
		return m_id;
	}

private:
	// Уникальный идентификатор типа
	const value_type	m_id;
};


// Карта обработчиков сообщений
typedef std::vector< HandlerItem* >			Handlers;
// Тип очереди сообщений
typedef structs::queue_t< package_t >	    MessageQueue;
// -
typedef system::MutexLocker                 Exclusive;



// Desc: Базовый тип для сообщений
struct ACTO_API msg_t
{
public:
	virtual ~msg_t() { }
};


// Desc: Базовый класс для всех актеров, как внутренних, так и внешних (пользовательских). 
class ACTO_API base_t
{
	// -
	friend class worker_t;

public:
	struct msg_destroy : public msg_t
	{
		// Объект, который необходимо удалить
		object_t*	object;
	};

public:
	base_t();
	virtual ~base_t();

protected:
	// Установка обработчика для сообщения данного типа
	template < typename MsgT, typename ClassName >
		inline void Handler( void (ClassName::*func)(acto::actor_t& sender, const MsgT& msg) );
	// Сброс обработчика для сообщения данного типа
	template < typename MsgT >
		inline void Handler();

private:
	void set_handler(i_handler* const handler, const TYPEID type);

private:
	// Карта обработчиков сообщений для данного объекта
	Handlers			m_handlers;
};

// Desc:
struct ACTO_API i_handler
{
	// Идентификатор типа сообщения
	const TYPEID		m_type;

public:
	i_handler(const TYPEID type_);

public:
	// -
	virtual void invoke(object_t* const sender, msg_t* const msg) = 0;
};


// Desc: Объект
struct ACTO_API object_t
{
	// Критическая секция для доступа к полям
    system::section_t   cs;
    // Флаги состояния текущего объекта
    bool                deleting;
    bool                freeing;
    bool                scheduled;
    bool                unimpl;

	// Реализация объекта
	base_t*				impl;
    // -
    object_t*           next;
    // Очередь сообщений, поступивших данному объекту
    MessageQueue        queue;
	// Count of references to object
	volatile long		references;
	// Поток, в котором должен выполнятся объект
	worker_t* const     thread;

public:
	object_t(worker_t* const thread_);
};


// Desc: Транспортный пакет для сообщения
struct ACTO_API package_t
{
	// Данные сообщения
	msg_t* const		data;
    // -
    package_t*          next;
	// Отправитель сообщения
	object_t*			sender;
	// Получатель сообщения
	object_t*			target;
	// Код типа сообщения
	const TYPEID		type;

public:
	 package_t(msg_t* const data_, const TYPEID type_);
	~package_t();
};


// Desc: Обертка для вызова обработчика сообщения конкретного типа
template <typename MsgT>
class handler_t : public i_handler
{
public:
	typedef fastdelegate::FastDelegate< void (acto::actor_t&, const MsgT&) >	delegate_t;

public:
	handler_t(const delegate_t& delegate_, type_box_t< MsgT >& type_);

public:
	// Вызвать обработчик
	virtual void invoke(object_t* const sender, msg_t* const msg);

private:
	// Делегат, хранящий указатель на 
	// метод конкретного объекта.
	const delegate_t	m_delegate;
};


///////////////////////////////////////////////////////////////////////////////
// Desc: Данные среды выполнения
class runtime_t
{
	struct resources_t
	{
		// Для контейнера заголовков актеров
        system::section_t       actors;
        // Для контейнера типов
        system::section_t       types;
	};

    // Тип множества актеров
    typedef std::set< object_t* >			Actors;
    // -
    typedef structs::queue_t< object_t >    HeaderQueue;
    // Стек заголовков объектов
    typedef structs::stack_t< object_t >    HeaderStack;
    // Тип множества зарегистрированных типов сообщений
    typedef std::map< std::string, TYPEID >	Types;
    // Очередь вычислителей
    typedef structs::queue_t< worker_t >	WorkerQueue;
	// -
    typedef structs::stack_t< worker_t >	WorkerStack;

    // 
    struct workers_t
    {
        // Текущее кол-во потоков
        volatile long   count;
        // Текущее кол-во удаляемых потоков
        volatile long   deleting;
        // Текущее кол-во эксклюзивных потоков
        volatile long   reserved;
        // Удаленные потоки
        WorkerStack		deleted;
        // Свободные потоки
	    WorkerStack     idle;

    };

    // Максимальное кол-во рабочих потоков в системе 
	static const unsigned int MAX_WORKERS = 512;

public:
	 runtime_t();
	~runtime_t();

public:
    // -
    void        acquire(object_t* const obj);
    // Создать экземпляр объекта, связав его с соответсвтующей реализацией
    object_t*	createActor(base_t* const impl, const int options = 0);
    // Уничтожить объект
    void        destroyObject(object_t* const object);
    // -
    long        release(object_t* const obj);
    // Послать сообщение указанному объекту 
    void		send(object_t* const target, msg_t* const msg, const TYPEID type);
    // Завершить выполнение
    void        shutdown();
    // Начать выполнение
    void        startup();    
    // -
    TYPEID      typeIdentifier(const char* const type_name);

private:
    // Цикл выполнения планировщика
	void		cleaner();
    // -
    package_t*	createPackage(object_t* const target, msg_t* const data, const TYPEID type);
    // -
    worker_t*   createWorker();
    // Деструткор для пользовательских объектов (актеров)
    void        destruct_actor(object_t* const actor);
    // Определить отправителя сообщения
    object_t*   determineSender(system::thread_t* const current);
    // Цикл выполнения планировщика
	void		execute();
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
    system::thread_t*   m_cleaner;
    // Экземпляр системного потока
	system::thread_t*	m_scheduler;
    // -
    system::event_t     m_event;
    system::event_t     m_evworker;
    
    // -
    system::event_t     m_evnoactors;
    system::event_t     m_evnoworkers;

    HeaderStack         m_deleted;

/* Защищаемые блокировкой данные */
	// Критическая секция для доступа к полям
	resources_t			m_cs;
    // Текущее множество актеров
	Actors				m_actors;
    // Генератор идентификаторов
	volatile TYPEID     m_counter;
    // Типы сообщений
	Types				m_types;


public:
/* Общие для всех потоков данные */
   // Очередь объектов, которым пришли сообщения
    structs::queue_t< object_t > m_queue;
    // Параметры потоков
    workers_t           m_workers;
    system::event_t     m_evclean;
};


///////////////////////////////////////////////////////////////////////////////
// Desc: Системный поток
class worker_t : public structs::item_t< worker_t >
{
    typedef fastdelegate::FastDelegate< void (object_t* const) >    PushDelete;
    typedef fastdelegate::FastDelegate< void (worker_t* const) >    PushIdle;

public:
    struct Slots {
        PushDelete  deleted;
        PushIdle    idle;
    };

public:
	 worker_t(const Slots slots);
	~worker_t();

public:
	// Поместить сообщение в очередь
	void        assign(object_t* const obj, const clock_t slice);
    // Текущий объект, обрабатывающий сообщение
    object_t*   invoking() const;
    // -
    void        wakeup();

private:
    // -
    void doHandle(package_t* const);
	// -
	void execute();

private:
    // Флаг активности потока
	volatile bool       m_active;
	// -
    system::event_t     m_event;
    // -
    object_t* volatile  m_object;
    // Текущий объект, обрабатывающий сообщение
	object_t*           m_invoking;
    // -
    clock_t             m_start;
    clock_t             m_time;
	// -
    const Slots         m_slots;
    // Экземпляр системного потока
	system::thread_t*   m_system;
};

extern runtime_t	runtime;


///////////////////////////////////////////////////////////////////////////////
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

// Инициализация
void initialize();

// -
void finalize();


}; // namespace core

}; // namespace acto

}; // namespace multiprog

#endif // __multiprogs__act_core_h__
