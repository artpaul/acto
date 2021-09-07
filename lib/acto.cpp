#include "acto.h"
#include "runtime.h"

namespace acto {

static std::atomic_long startup_counter(0);

actor_ref::actor_ref(core::object_t* const an_object, const bool acquire) noexcept
  : m_object(an_object)
{
  if (m_object && acquire) {
    core::runtime_t::instance()->acquire(m_object);
  }
}

actor_ref::actor_ref(const actor_ref& rhs) noexcept
  : m_object(rhs.m_object)
{
  if (m_object) {
    core::runtime_t::instance()->acquire(m_object);
  }
}

actor_ref::actor_ref(actor_ref&& rhs) noexcept
  : m_object(rhs.m_object)
{
  rhs.m_object = nullptr;
}

actor_ref::~actor_ref() {
  if (m_object) {
    core::runtime_t::instance()->release(m_object);
  }
}

bool actor_ref::assigned() const noexcept {
  return m_object != nullptr;
}

void actor_ref::join() {
  if (m_object) {
    core::runtime_t::instance()->join(m_object);
  }
}

bool actor_ref::send_message(std::unique_ptr<core::msg_t> msg) const {
  return core::runtime_t::instance()->send(m_object, std::move(msg));
}

actor_ref& actor_ref::operator = (const actor_ref& rhs) {
  if (this != &rhs) {
    if (rhs.m_object) {
      core::runtime_t::instance()->acquire(rhs.m_object);
    }
    if (m_object) {
      core::runtime_t::instance()->release(m_object);
    }
    m_object = rhs.m_object;
  }
  return *this;
}

actor_ref& actor_ref::operator = (actor_ref&& rhs) {
  if (this != &rhs) {
    if (m_object && m_object != rhs.m_object) {
        core::runtime_t::instance()->release(m_object);
    }
    m_object = rhs.m_object;
    rhs.m_object = nullptr;
  }
  return *this;
}

bool actor_ref::operator == (const actor_ref& rhs) const noexcept {
  return m_object == rhs.m_object;
}

bool actor_ref::operator != (const actor_ref& rhs) const noexcept {
  return !operator == (rhs);
}


void actor::die() {
  terminating_ = true;
}

void actor::consume_package(std::unique_ptr<core::msg_t> p) {
  const auto hi = handlers_.find(p->type);
  if (hi != handlers_.end()) {
    hi->second->invoke(std::move(p));
  }
}

void actor::set_handler(const std::type_index& type, std::unique_ptr<handler_t> h) {
  if (h) {
    handlers_[type] = std::move(h);
  } else {
    handlers_.erase(type);
  }
}


void destroy(actor_ref& object) {
  if (core::object_t* const obj = object.m_object) {
    object.m_object = nullptr;
    // Освободить ссылку на объект и удалить его
    if (core::runtime_t::instance()->release(obj) > 0) {
      core::runtime_t::instance()->deconstruct_object(obj);
    }
  }
}

void finalize_thread() {
  core::runtime_t::instance()->destroy_thread_binding();
}

void initialize_thread() {
  core::runtime_t::instance()->create_thread_binding();
}

void join(actor_ref& obj) {
  obj.join();
}

void process_messages() {
  core::runtime_t::instance()->process_binded_actors();
}

void shutdown() {
  if (startup_counter > 0 && (--startup_counter == 0)) {
    core::runtime_t::instance()->shutdown();
  }
}

void startup() {
  if (++startup_counter == 1) {
    core::runtime_t::instance()->startup();
  }
}

namespace core {

object_t::object_t(actor* const impl_)
  : impl(impl_)
  , binded(false)
  , deleting(false)
  , exclusive(false)
  , scheduled(false)
  , unimpl(false)
{
}

void object_t::enqueue(std::unique_ptr<msg_t> msg) noexcept {
  input_stack.push(msg.release());
}

bool object_t::has_messages() const noexcept {
  return !local_stack.empty() || !input_stack.empty();
}

std::unique_ptr<msg_t> object_t::select_message() noexcept {
  if (msg_t* p = local_stack.pop()) {
    return std::unique_ptr<msg_t>(p);
  } else {
    local_stack.push(input_stack.extract());
    return std::unique_ptr<msg_t>(local_stack.pop());
  }
}

msg_t::~msg_t() {
  // Release references.
  if (sender) {
    runtime_t::instance()->release(sender);
  }
  if (target) {
    runtime_t::instance()->release(target);
  }
}

} // namespace core
} // namespace acto
