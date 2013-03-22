
#include <list>
#include <acto.h>

namespace acto {

namespace services {

#ifdef ACTO_WIN

using fastdelegate::MakeDelegate;


// -
timer_t     timer;


namespace impl {

///////////////////////////////////////////////////////////////////////////////////////////////////
// Desc:
class TimerActor : public acto::implementation_t {
public:
	struct msg_cancel : public msg_t {
		actor_ref   actor;
	};

	struct msg_delete : public msg_t {
		actor_ref   actor;
	};

	struct msg_setup  : public msg_t {
		// Актер, которому будет послано уведомление
		actor_ref	actor;
		// Должно ли событие произойти только один раз или постоянно
		bool		once;
		// Интервал, через который событие должно произойти
		int			time;
	};

public:
	 TimerActor();
	~TimerActor();

private:
	/* Обработчики сообщений */

	void do_cancel(acto::actor_ref& sender, const msg_cancel& msg);
	void do_delete(acto::actor_ref& sender, const msg_delete& msg);
	void do_setup (acto::actor_ref& sender, const msg_setup&  msg);

	// Удалить событие для уазанного актера
	void delete_event(const acto::actor_ref& actor);

	// -
	static void CALLBACK TimerProc(PVOID lpParam, BOOLEAN TimerOrWaitFired);

private:
	// Зарегистрированное событие
	struct event_t {
		// Актер, которому будет послано уведомление
		actor_ref	actor;
		// Должно ли событие произойти только один раз или постоянно
		bool		once;
		// Интервал, через который событие должно произойти
		int			time;
		// -
		actor_ref	owner;
		// -
		HANDLE		timer;

	public:
		event_t(actor_ref& owner_);
		// -
		void invoke();
	};

	// -
	typedef std::list< event_t* >		Events;


	// Список зарегистрированных событий
	Events		m_events;
	// Дескриптор массива таймеров
	HANDLE		m_timers;
};


///////////////////////////////////////////////////////////////////////////////////////////////////
//-------------------------------------------------------------------------------------------------
TimerActor::TimerActor() {
	Handler< msg_cancel >( &TimerActor::do_cancel );
	Handler< msg_delete >( &TimerActor::do_delete );
	Handler< msg_setup  >( &TimerActor::do_setup  );

	// -
	m_timers = ::CreateTimerQueue();
}
//-------------------------------------------------------------------------------------------------
TimerActor::~TimerActor() {
	// -
	::DeleteTimerQueueEx( m_timers, INVALID_HANDLE_VALUE );
}

//-------------------------------------------------------------------------------------------------
void TimerActor::do_cancel(acto::actor_ref& sender, const TimerActor::msg_cancel& msg) {
	this->delete_event( msg.actor );
}
//-------------------------------------------------------------------------------------------------
void TimerActor::do_delete(acto::actor_ref& sender, const TimerActor::msg_delete& msg) {
	this->delete_event( msg.actor );
}
//-------------------------------------------------------------------------------------------------
void TimerActor::do_setup (acto::actor_ref& sender, const TimerActor::msg_setup& msg) {
	event_t* const	p_event = new event_t( actor_ref() );
	// -
	p_event->actor = msg.actor;
	p_event->once  = msg.once;
	p_event->time  = msg.time;
	p_event->owner = self;
	// -
//	core::set_notify( 0, acto::dereference(actor), MakeDelegate(this, &timer_t::do_delete) );
	// Установить системный таймер
	if ( 0 != ::CreateTimerQueueTimer( &p_event->timer,
		                               // Очередь таймеров
									   m_timers,
									   // -
									   &TimerActor::TimerProc,
									   // Параметр для процедуры
									   p_event,
									   // Период первого вызова
									   msg.time,
									   // Повторы
									   (msg.once ? 0 : msg.time),
									   // Флаги
									   0 ) )
	{
		// -
		m_events.push_back( p_event );
	}
	else {
		// Ошибка. Таймер не был созда.
		delete p_event;
	}
}

//-------------------------------------------------------------------------------------------------
void TimerActor::delete_event(const acto::actor_ref& actor) {
	for (Events::iterator i = m_events.begin(); i != m_events.end(); i++) {
		if ((*i)->actor == actor) {
			// 1. Удалить системный таймер
			//
			// NOTE: Так как последний параметр INVALID_HANDLE_VALUE, то
			//       функция возвратит управление только тогда, когда завершится
			//       выполнение соответствующей TimerProc.
			::DeleteTimerQueueTimer( m_timers, (*i)->timer, INVALID_HANDLE_VALUE );
			// 2. Удалить экземпляр события
			delete (*i);
			// 3. Удалить элемент из списка
			m_events.erase( i );
			// -
			break;
		}
	}
}

//-------------------------------------------------------------------------------------------------
void CALLBACK TimerActor::TimerProc(PVOID lpParam, BOOLEAN TimerOrWaitFired) {
	((event_t*)lpParam)->invoke();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//-------------------------------------------------------------------------------------------------
TimerActor::event_t::event_t(actor_ref& owner_) :
	owner( owner_ )
{
}
//-------------------------------------------------------------------------------------------------
void TimerActor::event_t::invoke() {
	// 1. Послать уведомление
	actor.send( msg_time() );
	// 2. Если требовалось только одно уведомление, то
	//    событие можно удалять.
	if (this->once) {
		TimerActor::msg_delete	msg;
		// -
		msg.actor = this->actor;
		// -
		owner.send( msg );
	}
}

}; // namespace impl



///////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                               //
///////////////////////////////////////////////////////////////////////////////////////////////////


//-------------------------------------------------------------------------------------------------
// Desc:
//-------------------------------------------------------------------------------------------------
void finalize() {
    acto::destroy(timer.m_actor);
}

//-------------------------------------------------------------------------------------------------
// Desc:
//-------------------------------------------------------------------------------------------------
void initialize() {
    // -
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//-------------------------------------------------------------------------------------------------
timer_t::timer_t() {
	// -
}
//-------------------------------------------------------------------------------------------------
timer_t::~timer_t() {
	// -
    assert( !m_actor.assigned() );
}


//-------------------------------------------------------------------------------------------------
void timer_t::cancel(acto::actor_ref& actor) {
	if (actor.assigned()) {
        impl::TimerActor::msg_cancel	msg;
		// -
		msg.actor = actor;
		// -
		m_actor.send( msg );
	}
}
//-------------------------------------------------------------------------------------------------
void timer_t::setup(acto::actor_ref& actor, const int time, const bool once) {
    // Инициализация актера
    if (!m_actor.assigned())
        m_actor = acto::instance< impl::TimerActor >(aoExclusive);

    // -
	if (actor.assigned()) {
		impl::TimerActor::msg_setup	msg;
		// -
		msg.actor = actor;
		msg.once  = once;
		msg.time  = time;
		// -
		m_actor.send( msg );
	}
}

#endif

}; // namespace services

}; // namespace acto
