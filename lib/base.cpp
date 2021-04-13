#include "base.h"
#include "module.h"
#include "runtime.h"

namespace acto {
namespace core {

object_t::object_t(base_t* const impl_)
  : impl(impl_)
  , binded(false)
  , deleting(false)
  , exclusive(false)
  , freeing(false)
  , scheduled(false)
  , unimpl(false)
{
}

void object_t::enqueue(std::unique_ptr<msg_t> msg) {
  input_stack.push(msg.release());
}

bool object_t::has_messages() const {
  return !local_stack.empty() || !input_stack.empty();
}

std::unique_ptr<msg_t> object_t::select_message() {
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
  runtime_t::instance()->release(target);
}


void base_t::die() {
  terminating_ = true;
}

void base_t::consume_package(std::unique_ptr<msg_t> p) {
  const auto hi = handlers_.find(p->type);
  if (hi != handlers_.end()) {
    hi->second->invoke(std::move(p));
  }
}

void base_t::set_handler(const std::type_index& type, std::unique_ptr<handler_t> h) {
  if (h) {
    handlers_[type] = std::move(h);
  } else {
    handlers_.erase(type);
  }
}

} // namespace core
} // namespace acto
