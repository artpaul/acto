#pragma once

#include "module.h"
#include "platform.h"

namespace acto {

/**
 * Base class for all user-defined actors.
 */
class actor : public core::base_t {
  friend class core::main_module_t;

protected:
  inline actor_ref context() const {
      return context_;
  }

  inline actor_ref self() const {
      return self_;
  }

private:
  // Ссылка на контекстный объект для данного
  actor_ref context_;
  /// Reference to itself.
  actor_ref self_;
};

/** Destroys the given object. */
ACTO_API void destroy(actor_ref& object);

//
ACTO_API void finalize_thread();

// Включить в текущем потоке возможность взаимодействия
// с ядром библиотеки acto
ACTO_API void initialize_thread();

/** Waits until the actor will finish. */
ACTO_API void join(actor_ref& obj);

/**
 * Processes all messages for objects
 * binded to the current thread (with aoBindToThread option).
 */
ACTO_API void process_messages();

/** Library shutdown. */
ACTO_API void shutdown();

/** Library initialization. */
ACTO_API void startup();

namespace detail {

template <typename Impl>
inline core::object_t* make_instance(actor_ref context, const int options, Impl* impl) {
  static_assert(std::is_base_of<::acto::actor, Impl>::value,
    "implementation should be derived from the acto::actor class");

  return core::main_module_t::instance()->make_instance<Impl>(
    std::move(context), options, impl);
}

template <typename Impl>
inline core::object_t* make_instance(actor_ref context, const int options, std::unique_ptr<Impl> impl) {
  return make_instance(std::move(context), options, impl.release());
}

} // namespace detail

template <typename T, typename ... P>
inline actor_ref spawn(P&& ... p) {
  return actor_ref(detail::make_instance<T>(actor_ref(), 0, std::make_unique<T>(std::forward<P>(p) ...)), false);
}

template <typename T>
inline actor_ref spawn(actor_ref context) {
  return actor_ref(detail::make_instance<T>(std::move(context), 0, std::make_unique<T>()), false);
}

template <typename T>
inline actor_ref spawn(const int options) {;
  return actor_ref(detail::make_instance<T>(actor_ref(), options, std::make_unique<T>()), false);
}

template <typename T>
inline actor_ref spawn(actor_ref context, const int options) {
  return actor_ref(detail::make_instance<T>(std::move(context), options, std::make_unique<T>()), false);
}

} // namespace acto
