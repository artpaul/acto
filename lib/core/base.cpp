#include "base.h"
#include "module.h"
#include "runtime.h"

namespace acto {
namespace core {

///////////////////////////////////////////////////////////////////////////////

object_t::object_t(base_t* const impl_)
    : impl      (impl_)
    , waiters   (nullptr)
    , references(0)
    , binded    (false)
    , deleting  (false)
    , exclusive (false)
    , freeing   (false)
    , scheduled (false)
    , unimpl    (false)
{
    next = nullptr;
}

void object_t::enqueue(msg_t* const msg) {
    input_stack.push(msg);
}

bool object_t::has_messages() const {
    return !local_stack.empty() || !input_stack.empty();
}

msg_t* object_t::select_message() {
    if (msg_t* const p = local_stack.pop()) {
        return p;
    } else {
        local_stack.push(input_stack.extract());

        return local_stack.pop();
    }
}

///////////////////////////////////////////////////////////////////////////////

msg_t::msg_t(const std::type_index& idx)
    : type(idx)
{
}

msg_t::~msg_t() {
    // Освободить ссылки на объекты
    if (sender) {
        runtime_t::instance()->release(sender);
    }
    runtime_t::instance()->release(target);
}

///////////////////////////////////////////////////////////////////////////////

void base_t::die() {
    this->m_terminating = true;
}

void base_t::consume_package(const std::unique_ptr<msg_t>& p) {
    const auto hi = m_handlers.find(p->type);
    if (hi != m_handlers.end()) {
        hi->second->invoke(p->sender, p.get());
    }
}

void base_t::set_handler(const std::type_index& type, std::unique_ptr<handler_t> h) {
    if (h) {
        m_handlers[type] = std::move(h);
    } else {
        m_handlers.erase(type);
    }
}

///////////////////////////////////////////////////////////////////////////////

} // namespace core
} // namespace acto
