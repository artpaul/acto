#pragma once

#include <core/module.h>
#include <core/services.h>
#include <util/platform.h>

namespace acto {

/**
 * Base class for all user-defined actors.
 */
class actor : public core::base_t {
    friend class core::main_module_t;

protected:
    // Ссылка на контекстный объект для данного
    actor_ref   context;
    // Ссылка на самого себя
    actor_ref   self;
};

/* Destroys the given object. */
ACTO_API void destroy(actor_ref& object);

//
ACTO_API void finalize_thread();

// Включить в текущем потоке возможность взаимодействия
// с ядром библиотеки acto
ACTO_API void initialize_thread();

/* Дождаться завершения работы указанног актера */
ACTO_API void join(actor_ref& obj);

// Обработать все сообщения для объектов,
// привязанных к текущему системному потоку (опция aoBindToThread)
ACTO_API void process_messages();

/* Library shutdown. */
ACTO_API void shutdown();

/* Library initialization. */
ACTO_API void startup();

namespace detail {

template <typename Impl>
inline core::object_t* make_instance(const actor_ref& context, const int options) {
    return core::main_module_t::instance()->make_instance< Impl >(context, options);
}

} // namespace detail

template <typename T>
inline actor_ref spawn() {
    actor_ref a;
    return actor_ref(detail::make_instance<T>(a, 0), false);
}

template <typename T>
inline actor_ref spawn(actor_ref& context) {
    return actor_ref(detail::make_instance<T>(context, 0), false);
}

template <typename T>
inline actor_ref spawn(const int options) {
    actor_ref a;
    return actor_ref(detail::make_instance<T>(a, options), false);
}

template <typename T>
inline actor_ref spawn(actor_ref& context, const int options) {
    return actor_ref(detail::make_instance<T>(context, options), false);
}

} // namespace acto
