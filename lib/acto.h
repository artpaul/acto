#pragma once

#include "event.h"
#include "generics.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>

namespace acto {

class actor;

enum class actor_thread {
  /// Use shared pool of threads for the actor.
  shared,

  /// Use dedicated thread for the actor.
  exclusive,

  /// Bind actor to the current thread.
  /// The option will be ignored if used inside the thread created by the
  /// library.
  bind,
};

namespace core {

class runtime_t;
class worker_t;
struct msg_t;

/**
 * Core object.
 */
struct object_t : public generics::intrusive_t<object_t> {
  struct waiter_t : public generics::intrusive_t<waiter_t> {
    event_t event;
  };

  using atomic_stack_t = generics::mpsc_stack_t<msg_t>;
  using intusive_stack_t = generics::stack_t<msg_t>;

  /// State mutex.
  std::mutex cs;
  /// Pointer to the object inherited from the actor class (aka actor body).
  actor* impl;
  /// Dedicated thread for the object.
  worker_t* thread{nullptr};
  /// Queue of input messages implemented with two stacks.
  atomic_stack_t input_stack;
  intusive_stack_t local_stack;
  /// Count of references to the object.
  std::atomic<unsigned long> references{0};
  /// List of events awaiting for object deconstruction.
  waiter_t* waiters{nullptr};
  /// State flags.
  const uint32_t binded : 1;
  const uint32_t exclusive : 1;
  uint32_t deleting : 1;
  uint32_t scheduled : 1;

public:
  object_t(const actor_thread thread_opt, std::unique_ptr<actor> body);

  /// Pushes a message into the mailbox.
  void enqueue(std::unique_ptr<msg_t> msg) noexcept;

  /// Whether any messages in the mailbox.
  bool has_messages() const noexcept;

  /// Selects a message from the mailbox.
  std::unique_ptr<msg_t> select_message() noexcept;
};

struct msg_t : generics::intrusive_t<msg_t> {
  /// Unique code for the message type.
  const std::type_index type;
  /// Sender of the message.
  /// Can be empty.
  object_t* sender{nullptr};

public:
  constexpr msg_t(const std::type_index& idx) noexcept
    : type(idx) {
  }

  virtual ~msg_t();
};

template <typename T, bool>
struct message_container;

/**
 * Implements empty base optimization.
 */
template <typename T>
struct message_container<T, true> : private T {
  static constexpr bool is_value_movable = false;

  constexpr message_container(T&& t)
    : T(std::move(t)) {
  }

  template <typename... Args>
  constexpr message_container(Args&&... args)
    : T(std::forward<Args>(args)...) {
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
struct message_container<T, false> {
  static constexpr bool is_value_movable = std::is_move_constructible<T>::value;

  constexpr message_container(T&& t)
    : value_(std::move(t)) {
  }

  template <typename... Args>
  constexpr message_container(Args&&... args)
    : value_(std::forward<Args>(args)...) {
  }

  constexpr const T& data() const& {
    return value_;
  }

  constexpr T data() && {
    return std::move(value_);
  }

private:
  T value_;
};

template <typename T>
using message_container_t =
  message_container<T, std::is_empty<T>::value && !std::is_final<T>::value>;

template <typename T>
struct msg_wrap_t
  : public msg_t
  , public message_container_t<T> {
  constexpr msg_wrap_t(T&& d)
    : msg_t(typeid(T))
    , message_container_t<T>(std::move(d)) {
  }

  template <typename... Args>
  constexpr msg_wrap_t(Args&&... args)
    : msg_t(typeid(T))
    , message_container_t<T>(std::forward<Args>(args)...) {
  }
};

} // namespace core

/**
 * Reference to an actor object.
 */
class actor_ref {
  friend struct std::hash<actor_ref>;
  friend void join(const actor_ref& obj);
  friend void destroy(actor_ref& object);

public:
  actor_ref() noexcept = default;

  actor_ref(core::object_t* const an_object, const bool acquire) noexcept;

  actor_ref(const actor_ref& rhs) noexcept;
  actor_ref(actor_ref&& rhs) noexcept;

  ~actor_ref();

public:
  /** Is the reference initialized with an object. */
  bool assigned() const noexcept;

  /** Waits until the actor stops. */
  void join() const;

  /**
   * Sends a message to the actor.
   *
   * @return true if the message has been placed into the actor's mailbox.
   */
  template <typename Msg>
  inline bool send(Msg&& msg) const {
    if (m_object) {
      return send_message(
        std::make_unique<
          core::msg_wrap_t<std::remove_cv_t<std::remove_reference_t<Msg>>>>(
          std::move(msg)));
    }
    return false;
  }

  /**
   * Sends a message to the actor.
   *
   * @return true if the message has been placed into the actor's mailbox.
   */
  template <typename Msg, typename... P>
  inline bool send(P&&... p) const {
    if (m_object) {
      return send_message(
        std::make_unique<core::msg_wrap_t<Msg>>(std::forward<P>(p)...));
    }
    return false;
  }

  /**
   * Sends a message to the actor.
   *
   * @return true if the message has been placed into the actor's mailbox.
   */
  template <typename Msg>
  inline bool send_on_behalf(const actor_ref& sender, Msg&& msg) const {
    if (m_object) {
      return send_message_on_behalf(
        sender.m_object,
        std::make_unique<
          core::msg_wrap_t<std::remove_cv_t<std::remove_reference_t<Msg>>>>(
          std::move(msg)));
    }
    return false;
  }

  template <typename Msg, typename... P>
  inline bool send_on_behalf(const actor_ref& sender, P&&... p) const {
    if (m_object) {
      return send_message_on_behalf(
        sender.m_object,
        std::make_unique<core::msg_wrap_t<Msg>>(std::forward<P>(p)...));
    }
    return false;
  }

public:
  actor_ref& operator=(const actor_ref& rhs);
  actor_ref& operator=(actor_ref&& rhs);

  bool operator==(const actor_ref& rhs) const noexcept;
  bool operator!=(const actor_ref& rhs) const noexcept;
  bool operator<(const actor_ref& rhs) const noexcept;

  explicit operator bool() const noexcept {
    return assigned();
  }

private:
  /// Dispatches a message.
  bool send_message(std::unique_ptr<core::msg_t> msg) const;

  /// Dispatches a message.
  bool send_message_on_behalf(core::object_t* sender,
                              std::unique_ptr<core::msg_t> msg) const;

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

  /** Wrapper for member function pointers. */
  template <typename M, typename C, typename P>
  class mem_handler_t : public handler_t {
    using F = std::function<void(C*, actor_ref, P)>;

  public:
    mem_handler_t(F&& func, C* ptr)
      : func_(std::move(func))
      , ptr_(ptr) {
      assert(func_);
      assert(ptr_);
    }

    void invoke(std::unique_ptr<core::msg_t> msg) const override {
      using message_reference_t =
        typename std::conditional<core::msg_wrap_t<M>::is_value_movable,
                                  core::msg_wrap_t<M>&&,
                                  const core::msg_wrap_t<M>&>::type;

      func_(ptr_, actor_ref(msg->sender, true),
            static_cast<message_reference_t>(*msg.get()).data());
    }

  private:
    const F func_;
    C* const ptr_;
  };

  /** Wrapper for functor objects. */
  template <typename M>
  class fun_handler_t : public handler_t {
    using F = std::function<void(actor_ref sender, const M& msg)>;

  public:
    fun_handler_t(F&& func)
      : func_(std::move(func)) {
      assert(func_);
    }

    void invoke(std::unique_ptr<core::msg_t> msg) const override {
      func_(actor_ref(msg->sender, true),
            static_cast<core::msg_wrap_t<M>*>(msg.get())->data());
    }

  private:
    const F func_;
  };

public:
  virtual ~actor() noexcept = default;

protected:
  inline actor_ref context() const {
    return context_;
  }

  inline actor_ref self() const {
    return self_;
  }

  virtual void bootstrap() {
  }

  /// Stops itself.
  void die();

  /// Sets handler as member function pointer.
  template <typename M, typename ClassName, typename P>
  inline void handler(void (ClassName::*func)(actor_ref, P)) {
    set_handler(
      // Type of the handler.
      std::type_index(typeid(M)),
      // Callback.
      std::make_unique<mem_handler_t<M, ClassName, P>>(
        func, static_cast<ClassName*>(this)));
  }

  /// Sets handler as functor object.
  template <typename M>
  inline void handler(std::function<void(actor_ref, const M&)> func) {
    set_handler(
      // Type of the handler.
      std::type_index(typeid(M)),
      // Callback.
      std::make_unique<fun_handler_t<M>>(std::move(func)));
  }

  /// Removes handler for the given type.
  template <typename M>
  inline void handler() {
    set_handler(std::type_index(typeid(M)), nullptr);
  }

  /// Removes handler for the given type.
  template <typename M>
  inline void handler(std::nullptr_t) {
    set_handler(std::type_index(typeid(M)), nullptr);
  }

private:
  void consume_package(std::unique_ptr<core::msg_t> p);

  void set_handler(const std::type_index& type, std::unique_ptr<handler_t> h);

private:
  using handlers =
    std::unordered_map<std::type_index, std::unique_ptr<handler_t>>;

  /// Reference to parent object. Can be null.
  actor_ref context_;
  /// Reference to itself.
  actor_ref self_;
  /// List of message handlers.
  handlers handlers_;
  /// Object in terminating state.
  bool terminating_{false};
};

/**
 * Destroys the given object.
 */
void destroy(actor_ref& object);

/**
 * Waits until the actor will finish.
 */
void join(const actor_ref& obj);

/**
 * Stops all actors.
 */
void shutdown();

namespace core {

object_t* make_instance(actor_ref context,
                        const actor_thread thread_opt,
                        std::unique_ptr<actor> body);

} // namespace core

template <typename T, typename... P>
inline std::enable_if_t<std::is_base_of<::acto::actor, T>::value, actor_ref>
spawn(P&&... p) {
  return actor_ref(
    core::make_instance(actor_ref(), actor_thread::shared,
                        std::make_unique<T>(std::forward<P>(p)...)),
    false);
}

template <typename T, typename... P>
inline std::enable_if_t<std::is_base_of<::acto::actor, T>::value, actor_ref>
spawn(actor_ref context, P&&... p) {
  return actor_ref(
    core::make_instance(std::move(context), actor_thread::shared,
                        std::make_unique<T>(std::forward<P>(p)...)),
    false);
}

template <typename T, typename... P>
inline std::enable_if_t<std::is_base_of<::acto::actor, T>::value, actor_ref>
spawn(const actor_thread thread_opt, P&&... p) {
  return actor_ref(
    core::make_instance(actor_ref(), thread_opt,
                        std::make_unique<T>(std::forward<P>(p)...)),
    false);
}

template <typename T, typename... P>
inline std::enable_if_t<std::is_base_of<::acto::actor, T>::value, actor_ref>
spawn(actor_ref context, const actor_thread thread_opt, P&&... p) {
  return actor_ref(
    core::make_instance(std::move(context), thread_opt,
                        std::make_unique<T>(std::forward<P>(p)...)),
    false);
}

namespace this_thread {

/**
 * Processes all messages for objects
 * binded to the current thread (with aoBindToThread option).
 */
void process_messages();

template <typename D>
inline void sleep_for(const D duration) {
  std::this_thread::sleep_for(duration);
}

} // namespace this_thread
} // namespace acto

template <>
struct std::hash<acto::actor_ref> {
  size_t operator()(const acto::actor_ref& ref) const noexcept {
    return reinterpret_cast<size_t>(ref.m_object);
  }
};
