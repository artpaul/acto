
#include "../multiprog.h"


namespace multiprog {

namespace acto {

namespace core {

using fastdelegate::FastDelegate;
using fastdelegate::MakeDelegate;

///////////////////////////////////////////////////////////////////////////////////////////////////
//-------------------------------------------------------------------------------------------------
worker_t::worker_t(const Slots slots) :
	m_active  ( true ),
    m_invoking( 0 ),
    m_object  ( 0 ),
    m_slots   ( slots ),
    m_start   ( 0 ),
	m_system  ( 0 )
{
    // -
	m_system = new multiprog::system::thread_t(MakeDelegate(this, &worker_t::execute), this);
}
//-------------------------------------------------------------------------------------------------
worker_t::~worker_t() {   
    // 1.
    m_active = false;
    // 2.
    m_event.signaled();
	// 3. Дождаться завершения системного потока
    m_system->join();
    // 4. Удалить системный поток
	delete m_system;
}

//-------------------------------------------------------------------------------------------------
// Desc: Поместить сообщение в очередь
//-------------------------------------------------------------------------------------------------
void worker_t::assign(object_t* const obj, const clock_t slice) {
    // 1. Назнгачить новый объект потоку
    m_object = obj;
    m_start  = clock();
    m_time   = slice;
    // 2. Активировать поток, если необходимо
    if (m_object)
        m_event.signaled();
}
//-------------------------------------------------------------------------------------------------
object_t* worker_t::invoking() const {
    return m_invoking;
}
//-------------------------------------------------------------------------------------------------
void worker_t::wakeup() {
    m_event.signaled();
}

//-------------------------------------------------------------------------------------------------
void worker_t::doHandle(package_t *const package) {
    object_t* const obj  = package->target;
    i_handler* handler   = 0;
    base_t*	  const impl = obj->impl;

    // 1. Найти обработчик соответствующий данному сообщению
    for (Handlers::iterator i = impl->m_handlers.begin(); i != impl->m_handlers.end(); i++) {
        if (package->type == (*i)->type) {
            handler = (*i)->handler;
	        break;
        }
    }
    // 2. Если соответствующий обработчик найден, то вызвать его
    try {
        if (handler) {
            // TN: Данный параметр читает только функция determine_sender,
            //     которая всегда выполняется в контексте этого потока.
            m_invoking = obj;
            // -
	        handler->invoke(package->sender, package->data);
            // -
            m_invoking = 0;
        }
    }
    catch (...) {
        // -
    }
}
//-------------------------------------------------------------------------------------------------
// Desc:
//-------------------------------------------------------------------------------------------------
void worker_t::execute() {
	while (m_active) {
        //
        // 1. Если данному потоку назначен объект, то необходимо 
        //    обрабатывать сообщения, ему поступившие
        //
        while (object_t* const obj = m_object) {
            // -
            if (package_t* const package = obj->queue.pop()) {
                // Обработать сообщение
			    doHandle(package);
                // -
                delete package;

                // Проверить истечение лимита времени 
                // обработки для данного объекта
                if (!m_object->thread) {
                    if ((clock() - m_start) > m_time) {
                        // -
                        runtime.m_queue.push(obj);
                        m_object = 0;
                    }
                }
            }
            else {
                bool deleting = false;
                // -
                {
                    Exclusive	lock(obj->cs);
                    // -
                    if (obj->queue.empty()) {
                        // Если это динамический объект 
                        if (obj->thread == 0) {
                            // -                         
                            obj->scheduled = false;
                            m_object = 0;
                        }
                        else { // Если это эксклюзивный объект
                            assert(obj->thread == this);
                            // -
                            if (obj->deleting) { 
                                // -
                                obj->scheduled = false;
                                m_object = 0;
                            }
                        }
                        //
                        // 2. Если текущий объект необходимо удалить
                        //
                        if (obj->deleting) {
                            assert(!obj->scheduled);
                            // -
                            obj->unimpl = true;
                            delete obj->impl, obj->impl = 0;
                            obj->unimpl = false;
                            // -
                            if (!obj->freeing && (obj->references == 0)) {
                                obj->freeing = true;
                                deleting     = true;
                            }
                        }
                    }
                }
                // -
                if (deleting)
                    m_slots.deleted(obj);
            }

            // Получить новый объект для обработки,
            // если он есть в очереди
             if (m_object == 0) {
                m_object = runtime.m_queue.pop();
                // -
                if (m_object)
                    m_start = clock();
                else
                    // Поместить текущий поток в список свободных
                    m_slots.idle(this);
            }
        }
        // 
        // 2. Ждать, пока не появится новое задание для данного потока
        // 
        m_event.wait();  // Cond: (m_object != 0) || (m_active == false)
        m_event.reset();
	}
}

}; // namespace core

}; // namespace acto

}; // namespace multiprog
