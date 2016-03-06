#pragma once

#include "base.h"

#include <util/event.h>
#include <util/queue.h>

namespace acto {
namespace core {

class runtime_t;

/**
 * Модуль, обеспечивающий обработку локальных актёров
 */
class main_module_t {
    /// -
    core::object_t* create_actor(base_t* const body, const int options);

public:
     main_module_t();
    ~main_module_t();

    static main_module_t* instance() {
        static main_module_t value;

        return &value;
    }

    /// Определить отправителя сообщения
    static object_t* determine_sender();

public:
    /// -
    void destroy_object_body(base_t* const body);
    /// -
    void handle_message(package_t* const package);
    /// Отправить сообщение соответствующему объекту
    void send_message(package_t* const package);
    /// -
    void shutdown(event_t& event);
    /// -
    void startup(runtime_t*);

    /// Создать экземпляр актёра
    template <typename Impl>
    object_t* make_instance(const actor_ref& context, const int options) {
        Impl* const value = new Impl();
        // Создать объект ядра (счетчик ссылок увеличивается автоматически)
        core::object_t* const result = this->create_actor(value, options);

        if (result) {
            value->context = context;
            value->self    = actor_ref(result);
        }

        return result;
    }

private:
    class impl;

    std::unique_ptr< impl > m_pimpl;
};

} // namespace core
} // namespace acto
