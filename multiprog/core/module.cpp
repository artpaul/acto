
#include "module.h"
#include "worker.h"
#include "runtime.h"

namespace acto {

namespace core {

///////////////////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
base_t::base_t()
    : m_terminating(false)
{
    // -
}
//-----------------------------------------------------------------------------
base_t::~base_t() {
    for (Handlers::iterator i = m_handlers.begin(); i != m_handlers.end(); i++) {
        // Удалить обработчик
        if ((*i)->handler)
            delete (*i)->handler;
        // Удалить элемент списка
        delete (*i);
    }
}
//-----------------------------------------------------------------------------
void base_t::terminate() {
    this->m_terminating = true;
}
//-----------------------------------------------------------------------------
void base_t::set_handler(i_handler* const handler, const TYPEID type) {
    for (Handlers::iterator i = m_handlers.begin(); i != m_handlers.end(); ++i) {
        if ((*i)->type == type) {
            // 1. Удалить предыдущий обработчик
            if ((*i)->handler)
                delete (*i)->handler;
            // 2. Установить новый
            (*i)->handler = handler;
            // -
            return;
        }
    }
    // Запись для данного типа сообщения еще не существует
    {
        HandlerItem* const item = new HandlerItem(type);
        // -
        item->handler = handler;
        // -
        m_handlers.push_back(item);
    }
}



///////////////////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
void main_module_t::handle_message(package_t* const package) {
    object_t* const obj = package->target;
    i_handler* handler  = 0;
    base_t* const impl  = static_cast<base_t*>(obj->impl);

    assert(obj->module == 0);

    // 1. Найти обработчик соответствующий данному сообщению
    for (base_t::Handlers::iterator i = impl->m_handlers.begin(); i != impl->m_handlers.end(); ++i) {
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
            threadCtx->sender = obj;
            // -
            handler->invoke(package->sender, package->data);
            // -
            threadCtx->sender = 0;

            if (impl->m_terminating)
                runtime_t::instance()->destroy_object(obj);
        }
    }
    catch (...) {
        // -
    }
}
//-----------------------------------------------------------------------------
void main_module_t::send_message(package_t* const package) {
    object_t* const target      = package->target;
    bool            undelivered = true;

    // Эксклюзивный доступ
    MutexLocker lock(target->cs);

    // Если объект отмечен для удалдения,
    // то ему более нельзя посылать сообщения
    if (!target->deleting) {
        // 1. Поставить сообщение в очередь объекта
        target->enqueue(package);
        // 2. Подобрать для него необходимый поток
        if (target->thread != NULL)
            target->thread->wakeup();
        else {
            if (!target->binded && !target->scheduled) {
                target->scheduled = true;
                // Добавить объект в очередь
                runtime_t::instance()->push_object(target);
            }
        }
        undelivered = false;
    }

    if (undelivered)
        delete package;
}

} // namespace core

} // namespace acto
