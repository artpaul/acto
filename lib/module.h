#pragma once

#include "base.h"
#include "event.h"
#include "queue.h"

namespace acto {
namespace core {

class runtime_t;

/**
 * Модуль, обеспечивающий обработку локальных актёров
 */
class main_module_t {
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
  void handle_message(std::unique_ptr<msg_t> msg);
  /// Отправить сообщение соответствующему объекту
  void send_message(std::unique_ptr<msg_t> msg);
  /// -
  void shutdown(event_t& event);
  /// -
  void startup(runtime_t*);

  /** Makes actor instance. */
  template <typename Impl>
  object_t* make_instance(actor_ref context, const int options, Impl* value) {
    assert(value);
    // Создать объект ядра (счетчик ссылок увеличивается автоматически)
    core::object_t* const result = create_actor(value, options);

    if (result) {
      value->context_ = std::move(context);
      value->self_ = actor_ref(result);
      value->bootstrap();
    } else {
        delete value;
    }

    return result;
  }

private:
  core::object_t* create_actor(base_t* body, const int options);

private:
  class impl;
  std::unique_ptr<impl> m_pimpl{nullptr};
};

} // namespace core
} // namespace acto
