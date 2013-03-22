#include "types.h"
#include "runtime.h"
#include "module.h"
#include <util/system/platform.h>
#include <util/system/thread.h>

namespace acto {
namespace core {

///////////////////////////////////////////////////////////////////////////////

object_t::object_t(actor_body_t* const impl_, const ui8 module_)
    : impl      (impl_)
    , waiters   (NULL)
    , references(0)
    , module    (module_)
    , binded    (false)
    , deleting  (false)
    , exclusive (false)
    , freeing   (false)
    , scheduled (false)
    , unimpl    (false)
{
    next = NULL;
}

void object_t::enqueue(package_t* const msg) {
    input_stack.push(msg);
}

bool object_t::has_messages() const {
    return !local_stack.empty() || !input_stack.empty();
}

package_t* object_t::select_message() {
    if (package_t* const p = local_stack.pop()) {
        return p;
    } else {
        local_stack.push(input_stack.extract());

        return local_stack.pop();
    }
}

///////////////////////////////////////////////////////////////////////////////

package_t::package_t(msg_t* const data_, const TYPEID type_)
    : data  (data_)
    , sender(NULL)
    , type  (type_)
{
}

package_t::~package_t() {
    // Освободить ссылки на объекты
    if (sender) {
        runtime_t::instance()->release(sender);
    }
    runtime_t::instance()->release(target);
}

} // namespace core
} // namespace acto
