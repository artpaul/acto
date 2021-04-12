#pragma once

#include <util/event.h>
#include <util/intrlist.h>
#include <util/platform.h>
#include <util/stack.h>

#include <functional>
#include <mutex>
#include <set>
#include <typeindex>
#include <typeinfo>
#include <vector>
#include <unordered_map>

namespace acto {

/// Индивидуальный поток для актера
static constexpr int aoExclusive = 0x01;

/// Привязать актера к текущему системному потоку.
/// Не имеет эффекта, если используется в контексте потока,
/// созданного самой библиотекой.
static constexpr int aoBindToThread = 0x02;

namespace core {

class base_t;
class main_module_t;
class runtime_t;
class worker_t;
struct msg_t;

/**
 * Объект
 */
struct object_t : public intrusive_t<object_t> {
  struct waiter_t : public intrusive_t<waiter_t> {
    event_t* event{nullptr};
  };

  using atomic_stack_t = generics::mpsc_stack_t<msg_t> ;
  using intusive_stack_t = generics::stack_t<msg_t>;

  // Критическая секция для доступа к полям
  std::recursive_mutex cs;

  // Реализация объекта
  base_t* impl{nullptr};
  // Список сигналов для потоков, ожидающих уничтожения объекта
  waiter_t* waiters{nullptr};
  // Очередь сообщений, поступивших данному объекту
  atomic_stack_t input_stack;
  intusive_stack_t local_stack;
  // Count of references to object
  std::atomic<unsigned long> references{0};
  // Флаги состояния текущего объекта
  ui32 binded : 1;
  ui32 deleting : 1;
  ui32 exclusive : 1;
  ui32 freeing : 1;
  ui32 scheduled : 1;
  ui32 unimpl : 1;

public:
  object_t(base_t* const impl_);

  /// Поставить сообщение в очередь
  void enqueue(std::unique_ptr<msg_t> msg);

  /// Есть ли сообщения
  bool has_messages() const;

  /// Выбрать сообщение из очереди
  std::unique_ptr<msg_t> select_message();
};


/**
 * Базовый тип для сообщений
 */
struct msg_t : intrusive_t<msg_t> {
  // Код типа сообщения
  const std::type_index type;
  // Отправитель сообщения
  object_t* sender{nullptr};
  // Получатель сообщения
  object_t* target{nullptr};

public:
  constexpr msg_t(const std::type_index& idx)
    : type(idx)
  {
  }

  virtual ~msg_t();
};

template <typename T>
struct msg_wrap_t
  : public msg_t
  , private T
{
  inline msg_wrap_t(T&& d)
    : msg_t(typeid(T))
    , T(std::move(d))
  {
  }

  template <typename ... Args>
  inline msg_wrap_t(Args&& ... args)
    : msg_t(typeid(T))
    , T(std::forward<Args>(args) ... )
  {
  }

  inline const T& data() const {
    return *static_cast<const T*>(this);
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

  explicit actor_ref(core::object_t* const an_object, const bool acquire = true);

  actor_ref(const actor_ref& rhs);
  actor_ref(actor_ref&& rhs);

  ~actor_ref();

public:
  actor_ref& operator = (const actor_ref& rhs);
  actor_ref& operator = (actor_ref&& rhs) noexcept;

  bool operator == (const actor_ref& rhs) const noexcept;
  bool operator != (const actor_ref& rhs) const noexcept;

  explicit operator bool () const noexcept {
    return assigned();
  }

  /// Инициализирован ли текущий объект
  bool assigned() const noexcept ;

  core::object_t* data() const noexcept {
    return m_object;
  }

  /// Sends a message to the actor.
  template <typename MsgT>
  inline void send(MsgT&& msg) const {
    if (m_object) {
      send_message(
        std::make_unique<core::msg_wrap_t<typename std::remove_reference<MsgT>::type>>(std::move(msg))
      );
    }
  }

  /// Sends a message to the actor.
  template <typename MsgT, typename ... P>
  inline void send(P&& ... p) const {
    if (m_object) {
      send_message(
        std::make_unique<core::msg_wrap_t<MsgT>>(std::forward<P>(p) ... ));
    }
  }

private:
  /// Dispatches a message.
  void send_message(std::unique_ptr<core::msg_t> msg) const;

private:
  core::object_t* m_object{nullptr};
};

namespace core {

/**
 * Базовый класс для локальных актеров.
 */
class base_t {
  friend class main_module_t;

  class handler_t {
  public:
    virtual ~handler_t() = default;

    virtual void invoke(object_t* const sender, std::unique_ptr<msg_t> msg) const = 0;
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
    virtual void invoke(object_t* const sender, std::unique_ptr<msg_t> msg) const {
      m_delegate(m_c, actor_ref(sender), static_cast<const msg_wrap_t<MsgT>*>(msg.get())->data());
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
    virtual void invoke(object_t* const sender, std::unique_ptr<msg_t> msg) const {
      m_delegate(actor_ref(sender), static_cast<const msg_wrap_t<MsgT>*>(msg.get())->data());
    }

  private:
    /// Делегат, хранящий указатель на
    /// метод конкретного объекта.
    const delegate_t m_delegate;
  };

public:
  virtual ~base_t() = default;

protected:
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
  void consume_package(std::unique_ptr<msg_t> p);

  void set_handler(const std::type_index& type, std::unique_ptr<handler_t> h);

private:
  /// Карта обработчиков сообщений
  using handlers = std::unordered_map<std::type_index, std::unique_ptr<handler_t>>;

  /// List of message handlers.
  handlers handlers_;
  /// Поток, в котором должен выполнятся объект
  core::worker_t* thread_{nullptr};
  ///
  bool terminating_{false};
};

} // namespace core
} // namespace acto
