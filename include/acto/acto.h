#pragma once

#include "event.h"
#include "intrusive.h"

#include <atomic>
#include <bit>
#include <cassert>
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
struct object_t : public intrusive::node<object_t> {
  struct waiter_t : public intrusive::node<waiter_t> {
    event on_deleted;
  };

  using atomic_stack = intrusive::mpsc_stack<msg_t>;
  using intusive_stack = intrusive::stack<msg_t>;

  /// State mutex.
  std::mutex cs;
  /// Pointer to the object inherited from the actor class (aka actor body).
  actor* impl;
  /// Dedicated thread for the object.
  worker_t* thread{nullptr};
  /// Queue of input messages implemented with two stacks.
  atomic_stack input_stack;
  intusive_stack local_stack;
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

struct msg_t : intrusive::node<msg_t> {
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

  constexpr message_container(T&& t) noexcept(
    std::is_nothrow_move_constructible_v<T>)
    : T(std::move(t)) {
  }

  template <typename... Args>
  constexpr message_container(Args&&... args) noexcept(
    std::is_nothrow_constructible_v<T, Args...>)
    : T(std::forward<Args>(args)...) {
  }

  constexpr const T& data() const noexcept {
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

  constexpr message_container(T&& t) noexcept(
    std::is_nothrow_move_constructible_v<T>)
    : value_(std::move(t)) {
  }

  template <typename... Args>
  constexpr message_container(Args&&... args) noexcept(
    std::is_nothrow_constructible_v<T, Args...>)
    : value_(std::forward<Args>(args)...) {
  }

  constexpr const T& data() const& noexcept {
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
  constexpr msg_wrap_t(T&& d) noexcept(
    std::is_nothrow_constructible_v<message_container_t<T>, T&&>)
    : msg_t(typeid(T))
    , message_container_t<T>(std::move(d)) {
  }

  template <typename... Args>
  constexpr msg_wrap_t(Args&&... args) noexcept(
    std::is_nothrow_constructible_v<message_container_t<T>, Args...>)
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
  friend void destroy(const actor_ref& object);

public:
  constexpr actor_ref() noexcept = default;

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
    if (!object_) {
      return false;
    }

    return send_message(
      std::make_unique<core::msg_wrap_t<std::remove_cvref_t<Msg>>>(
        std::forward<Msg>(msg)));
  }

  /**
   * Sends a message to the actor.
   *
   * @return true if the message has been placed into the actor's mailbox.
   */
  template <typename Msg, typename... P>
  inline bool send(P&&... p) const {
    if (!object_) {
      return false;
    }

    return send_message(
      std::make_unique<core::msg_wrap_t<std::remove_cvref_t<Msg>>>(
        std::forward<P>(p)...));
  }

  /**
   * Sends a message to the actor.
   *
   * @return true if the message has been placed into the actor's mailbox.
   */
  template <typename Msg>
  inline bool send_on_behalf(const actor_ref& sender, Msg&& msg) const {
    if (!object_) {
      return false;
    }

    return send_message_on_behalf(
      sender.object_,
      std::make_unique<core::msg_wrap_t<std::remove_cvref_t<Msg>>>(
        std::forward<Msg>(msg)));
  }

  template <typename Msg, typename... P>
  inline bool send_on_behalf(const actor_ref& sender, P&&... p) const {
    if (!object_) {
      return false;
    }

    return send_message_on_behalf(
      sender.object_,
      std::make_unique<core::msg_wrap_t<std::remove_cvref_t<Msg>>>(
        std::forward<P>(p)...));
  }

public:
  actor_ref& operator=(const actor_ref& rhs);
  actor_ref& operator=(actor_ref&& rhs);

  bool operator==(const actor_ref& rhs) const noexcept {
    return object_ == rhs.object_;
  }

  bool operator!=(const actor_ref& rhs) const noexcept {
    return object_ != rhs.object_;
  }

  bool operator<(const actor_ref& rhs) const noexcept {
    return std::less<const core::object_t*>()(object_, rhs.object_);
  }

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
  core::object_t* object_{nullptr};
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

  template <typename F, typename M>
  struct is_handler_signature {
    static constexpr bool value =
      std::is_invocable_v<F> || std::is_invocable_v<F, const M&> ||
      std::is_invocable_v<F, M&&> || std::is_invocable_v<F, actor_ref> ||
      std::is_invocable_v<F, actor_ref, const M&> ||
      std::is_invocable_v<F, actor_ref, M&&>;
  };

  template <typename M, typename P>
  using message_reference_t =
    std::conditional_t<core::msg_wrap_t<M>::is_value_movable &&
                         std::is_rvalue_reference_v<P>,
                       core::msg_wrap_t<M>&&,
                       const core::msg_wrap_t<M>&>;

  /// Wrapper for member function pointers.
  template <typename M, typename C, typename P>
  class mem_handler_t final : public handler_t {
    using F = std::function<void(C*, actor_ref, P)>;

  public:
    mem_handler_t(F&& func, C* ptr) noexcept
      : func_(std::move(func))
      , ptr_(ptr) {
      assert(func_);
      assert(ptr_);
    }

    void invoke(std::unique_ptr<core::msg_t> msg) const final {
      func_(ptr_, actor_ref(msg->sender, true),
            static_cast<message_reference_t<M, P>>(*msg.get()).data());
    }

  private:
    const F func_;
    C* const ptr_;
  };

  /// Wrapper for functor objects.
  template <typename M, typename... Args>
  class fun_handler_t final : public handler_t {
    using F = std::function<void(Args...)>;

  public:
    fun_handler_t(F&& func) noexcept
      : func_(std::move(func)) {
      assert(func_);
    }

    void invoke(std::unique_ptr<core::msg_t> msg) const final {
      if constexpr (sizeof...(Args) == 0) {
        func_();
      } else if constexpr (sizeof...(Args) == 1) {
        using P0 = std::tuple_element_t<0, std::tuple<Args...>>;

        if constexpr (std::is_same_v<actor_ref, std::decay_t<P0>>) {
          func_(actor_ref(msg->sender, true));
        } else {
          func_(static_cast<message_reference_t<M, P0>>(*msg.get()).data());
        }
      } else if constexpr (sizeof...(Args) == 2) {
        using P1 = std::tuple_element_t<1, std::tuple<Args...>>;

        func_(actor_ref(msg->sender, true),
              static_cast<message_reference_t<M, P1>>(*msg.get()).data());
      }
    }

  private:
    const F func_;
  };

public:
  virtual ~actor() noexcept = default;

protected:
  actor_ref context() const {
    return context_;
  }

  actor_ref self() const {
    return self_;
  }

  virtual void bootstrap() {
  }

  /// Stops itself.
  void die() noexcept;

public:
  /// Sets handler as member function pointer.
  template <typename M, typename ClassName, typename P>
  void handler(void (ClassName::*func)(actor_ref, P)) {
    set_handler(
      // Type of the handler.
      std::type_index(typeid(M)),
      // Callback.
      std::make_unique<mem_handler_t<M, ClassName, P>>(
        func, static_cast<ClassName*>(this)));
  }

  /// Sets handler as functor object.
  template <typename M,
            typename F,
            typename = std::enable_if_t<is_handler_signature<F, M>::value>>
  void handler(F&& func) {
    if constexpr (std::is_invocable_v<F>) {
      set_handler(
        // Type of the handler.
        std::type_index(typeid(M)),
        // Callback.
        std::make_unique<fun_handler_t<M>>(std::move(func)));
    } else if constexpr (std::is_invocable_v<F, const M&>) {
      set_handler(
        // Type of the handler.
        std::type_index(typeid(M)),
        // Callback.
        std::make_unique<fun_handler_t<M, const M&>>(std::move(func)));
    } else if constexpr (std::is_invocable_v<F, M&&>) {
      set_handler(
        // Type of the handler.
        std::type_index(typeid(M)),
        // Callback.
        std::make_unique<fun_handler_t<M, M&&>>(std::move(func)));
    } else if constexpr (std::is_invocable_v<F, actor_ref>) {
      set_handler(
        // Type of the handler.
        std::type_index(typeid(M)),
        // Callback.
        std::make_unique<fun_handler_t<M, actor_ref>>(std::move(func)));
    } else if constexpr (std::is_invocable_v<F, actor_ref, const M&>) {
      set_handler(
        // Type of the handler.
        std::type_index(typeid(M)),
        // Callback.
        std::make_unique<fun_handler_t<M, actor_ref, const M&>>(
          std::move(func)));
    } else if constexpr (std::is_invocable_v<F, actor_ref, M&&>) {
      set_handler(
        // Type of the handler.
        std::type_index(typeid(M)),
        // Callback.
        std::make_unique<fun_handler_t<M, actor_ref, M&&>>(std::move(func)));
    }
  }

  /// Removes handler for the given type.
  template <typename M>
  void handler() {
    set_handler(std::type_index(typeid(M)), nullptr);
  }

  /// Removes handler for the given type.
  template <typename M>
  void handler(std::nullptr_t) {
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
 * Sets the actor to destroying state.
 *
 * Actor's body will be delete when all messages sent prior state change
 * will be processed.
 */
void destroy(const actor_ref& object);

/**
 * Sets the actor to destroying state and waits until actor's body will be
 * deleted.
 */
void destroy_and_wait(const actor_ref& object);

/**
 * Waits until actor's body will be deleted.
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
  constexpr size_t operator()(const acto::actor_ref& ref) const noexcept {
    return std::bit_cast<size_t>(ref.object_);
  }
};
