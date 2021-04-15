#pragma once

#include "event.h"
#include "generics.h"
#include "platform.h"

#include <atomic>
#include <functional>
#include <mutex>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>

namespace acto {

class actor;

/// Индивидуальный поток для актера
static constexpr int aoExclusive = 0x01;

/// Привязать актера к текущему системному потоку.
/// Не имеет эффекта, если используется в контексте потока,
/// созданного самой библиотекой.
static constexpr int aoBindToThread = 0x02;

namespace core {

class runtime_t;
class worker_t;
struct msg_t;

/**
 * Объект
 */
struct object_t : public generics::intrusive_t<object_t> {
  struct waiter_t : public generics::intrusive_t<waiter_t> {
    event_t event;
  };

  using atomic_stack_t = generics::mpsc_stack_t<msg_t> ;
  using intusive_stack_t = generics::stack_t<msg_t>;

  // Критическая секция для доступа к полям
  std::recursive_mutex cs;

  /// Pointer to the object inherited from the actor class (aka actor body).
  actor* impl{nullptr};
  /// Dedicated thread for the object.
  worker_t* thread{nullptr};
  // Список сигналов для потоков, ожидающих уничтожения объекта.
  waiter_t* waiters{nullptr};
  // Очередь сообщений, поступивших данному объекту
  atomic_stack_t input_stack;
  intusive_stack_t local_stack;
  // Count of references to the object.
  std::atomic<unsigned long> references{0};
  // Флаги состояния текущего объекта
  ui32 binded : 1;
  ui32 deleting : 1;
  ui32 exclusive : 1;
  ui32 freeing : 1;
  ui32 scheduled : 1;
  ui32 unimpl : 1;

public:
  object_t(actor* const impl_);

  /// Поставить сообщение в очередь
  void enqueue(std::unique_ptr<msg_t> msg);

  /// Есть ли сообщения
  bool has_messages() const;

  /// Выбрать сообщение из очереди
  std::unique_ptr<msg_t> select_message();
};


struct msg_t : generics::intrusive_t<msg_t> {
  /// Unique code for the message type.
  const std::type_index type;
  /// Sender of the message.
  /// Can be empty.
  object_t* sender{nullptr};
  /// Receiver of the message.
  object_t* target{nullptr};

public:
  constexpr msg_t(const std::type_index& idx)
    : type(idx)
  {
  }

  virtual ~msg_t();
};


template <typename T, bool>
struct message_container;

/**
 * Implements empty base optimization.
 */
template <typename T>
struct message_container<T, false> : private T {
  constexpr message_container(T&& t)
    : T(std::move(t))
  {
  }

  template <typename ... Args>
  constexpr message_container(Args&& ... args)
    : T(std::forward<Args>(args) ... )
  {
  }

  constexpr const T& data() const {
    return *static_cast<const T*>(this);
  }
};

/**
 * Final classes cannot be used as a base class, so
 * we need to store them as a field.
 */
template <typename T>
struct message_container<T, true> {
  constexpr message_container(T&& t)
    : value_(std::move(t))
  {
  }

  template <typename ... Args>
  constexpr message_container(Args&& ... args)
    : value_(std::forward<Args>(args) ... )
  {
  }

  constexpr const T& data() const {
    return value_;
  }

private:
  const T value_;
};

template <typename T>
using message_container_t = message_container<T, std::is_final<T>::value>;

template <typename T>
struct msg_wrap_t
  : public msg_t
  , public message_container_t<T>
{
  constexpr msg_wrap_t(T&& d)
    : msg_t(typeid(T))
    , message_container_t<T>(std::move(d))
  {
  }

  template <typename ... Args>
  constexpr msg_wrap_t(Args&& ... args)
    : msg_t(typeid(T))
    , message_container_t<T>(std::forward<Args>(args) ... )
  {
  }
};

} // namespace core


/**
 * Пользовательский объект (актер)
 */
class actor_ref {
  friend void join(actor_ref& obj);
  friend void destroy(actor_ref& object);

public:
  actor_ref() = default;

  actor_ref(core::object_t* const an_object, const bool acquire) noexcept;

  actor_ref(const actor_ref& rhs) noexcept;
  actor_ref(actor_ref&& rhs) noexcept;

  ~actor_ref();

public:
  /** Is the reference initialized with an object. */
  bool assigned() const noexcept;

  /** Waits until the actor stops. */
  void join();

  /**
   * Sends a message to the actor.
   *
   * @return true if the message has been placed in the actor's mailbox.
   */
  template <typename MsgT>
  inline bool send(MsgT&& msg) const {
    if (m_object) {
      return send_message(
        std::make_unique<core::msg_wrap_t<typename std::remove_reference<MsgT>::type>>(std::move(msg))
      );
    }
    return false;
  }

  /**
   * Sends a message to the actor.
   *
   * @return true if the message has been placed in the actor's mailbox.
   */
  template <typename MsgT, typename ... P>
  inline bool send(P&& ... p) const {
    if (m_object) {
      return send_message(
        std::make_unique<core::msg_wrap_t<MsgT>>(std::forward<P>(p) ... ));
    }
    return false;
  }

public:
  actor_ref& operator = (const actor_ref& rhs);
  actor_ref& operator = (actor_ref&& rhs);

  bool operator == (const actor_ref& rhs) const noexcept;
  bool operator != (const actor_ref& rhs) const noexcept;

  explicit operator bool () const noexcept {
    return assigned();
  }

private:
  /// Dispatches a message.
  bool send_message(std::unique_ptr<core::msg_t> msg) const;

private:
  core::object_t* m_object{nullptr};
};


/**
 * Base class for all user-defined actors.
 */
class actor {
  friend class core::runtime_t;

  class handler_t {
  public:
    virtual ~handler_t() = default;

    virtual void invoke(const std::unique_ptr<core::msg_t> msg) const = 0;
  };

  /**
   * Обертка для вызова обработчика сообщения конкретного типа
   */
  template <typename MsgT, typename C>
  class mem_handler_t : public handler_t {
  public:
    using delegate_t = std::function< void (C*, actor_ref, const MsgT&) >;

  public:
    mem_handler_t(const delegate_t& delegate_, C* c)
      : m_delegate(delegate_)
      , m_c(c)
    {
    }

    // Вызвать обработчик
    void invoke(const std::unique_ptr<core::msg_t> msg) const override {
      m_delegate(
        m_c,
        actor_ref(msg->sender, true),
        static_cast<const core::msg_wrap_t<MsgT>*>(msg.get())->data());
    }

  private:
    /// Делегат, хранящий указатель на
    /// метод конкретного объекта.
    const delegate_t m_delegate;

    C* const m_c;
  };

  template <typename MsgT>
  class fun_handler_t : public handler_t {
  public:
    using delegate_t = std::function< void (actor_ref, const MsgT&) >;

  public:
    fun_handler_t(const delegate_t& delegate_)
      : m_delegate(delegate_)
    {
    }

    // Вызвать обработчик
    void invoke(const std::unique_ptr<core::msg_t> msg) const override {
      m_delegate(
        actor_ref(msg->sender, true),
        static_cast<const core::msg_wrap_t<MsgT>*>(msg.get())->data());
    }

  private:
    /// Делегат, хранящий указатель на
    /// метод конкретного объекта.
    const delegate_t m_delegate;
  };

public:
  virtual ~actor() = default;

protected:
  inline actor_ref context() const {
      return context_;
  }

  inline actor_ref self() const {
      return self_;
  }

  virtual void bootstrap() { }

  /// Stops itself.
  void die();

  /// Установка обработчика для сообщения данного типа
  template <typename M, typename ClassName>
  inline void handler(void (ClassName::*func)(actor_ref sender, const M& msg)) {
    // Установить обработчик
    set_handler(
      // Тип сообщения
      std::type_index(typeid(M)),
      // Обрабочик
      std::make_unique<mem_handler_t<M, ClassName>>(func, static_cast<ClassName*>(this)));
  }

  /// Установка обработчика для сообщения данного типа
  template <typename M>
  inline void handler(std::function< void (actor_ref sender, const M& msg) > func) {
    // Установить обработчик
    set_handler(
      // Тип сообщения
      std::type_index(typeid(M)),
      // Обрабочик
      std::make_unique<fun_handler_t<M>>(func));
  }

  /// Сброс обработчика для сообщения данного типа
  template <typename M>
  inline void handler() {
    // Сбросить обработчик указанного типа
    set_handler(std::type_index(typeid(M)), nullptr);
  }

private:
  void consume_package(std::unique_ptr<core::msg_t> p);

  void set_handler(const std::type_index& type, std::unique_ptr<handler_t> h);

private:
  /// Карта обработчиков сообщений
  using handlers = std::unordered_map<std::type_index, std::unique_ptr<handler_t>>;

  /// Reference to parent object. Can be null.
  actor_ref context_;
  /// Reference to itself.
  actor_ref self_;
  /// List of message handlers.
  handlers handlers_;
  /// Object in terminating state.
  bool terminating_{false};
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

namespace core {

object_t* make_instance(actor_ref context, const int options, actor* body);

} // namespace code

namespace detail {

template <typename Impl>
inline core::object_t* make_instance(actor_ref context, const int options, Impl* impl) {
  static_assert(std::is_base_of<::acto::actor, Impl>::value,
    "implementation should be derived from the acto::actor class");

  return core::make_instance(std::move(context), options, impl);
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
