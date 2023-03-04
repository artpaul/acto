#include "worker.h"
#include "runtime.h"

namespace acto {
namespace core {

worker_t::worker_t(callbacks* const slots, std::function<void()> init_cb)
  : slots_(slots) {
  thread_ = std::thread([this, cb = std::move(init_cb)]() {
    // Call the initialization in thread's context.
    cb();
    // Execute the event loop.
    execute();
  });
}

worker_t::~worker_t() {
  active_ = false;
  event_.signaled();

  if (thread_.joinable()) {
    thread_.join();
  }
}

void worker_t::assign(object_t* const obj,
                      const std::chrono::steady_clock::duration slice) {
  assert(!object_ && obj);

  object_ = obj;
  start_ = std::chrono::steady_clock::now();
  time_slice_ = slice;
  // Acquire the object.
  runtime_t::instance()->acquire(obj);
  // Wakeup the thread.
  event_.signaled();
}

void worker_t::wakeup() {
  event_.signaled();
}

void worker_t::execute() {
  while (active_) {
    if (!process()) {
      slots_->push_idle(this);
    }

    // Wait for new tasks.
    event_.wait(); // Cond: (object_ != 0) || (active_ == false)
  }
}

bool worker_t::process() {
  while (object_t* const obj = object_) {
    bool need_delete = false;

    while (true) {
      // Handle a message.
      if (auto msg = obj->select_message()) {
        slots_->handle_message(obj, std::move(msg));
        // Continue processing messages if the object is bound to the thread or
        // the time slice has not been elapsed yet.
        if (obj->exclusive ||
            time_slice_ >= (std::chrono::steady_clock::now() - start_))
        {
          continue;
        }
      }

      // There are no messages in the object's mailbox or
      // the time slice was elapsed.
      std::lock_guard<std::mutex> g(obj->cs);

      if (obj->deleting) {
        // Drain the object's mailbox if it in the deleting state.
        if (obj->has_messages()) {
          time_slice_ = std::chrono::steady_clock::duration::max();
          continue;
        }

        need_delete = true;
        obj->scheduled = false;
      } else if (obj->exclusive) {
        // Just wait for new messages if the object
        // exclusively bound to the thread.
        return true;
      } else if (obj->has_messages()) {
        // Return object to the shared queue.
        slots_->push_object(obj);
      } else {
        obj->scheduled = false;
      }

      break;
    }

    if (runtime_t::instance()->release(obj)) {
      if (need_delete) {
        slots_->push_delete(obj);
      }
    }

    // Retrieve next object from the shared queue.
    if ((object_ = slots_->pop_object())) {
      start_ = std::chrono::steady_clock::now();
      runtime_t::instance()->acquire(object_);
    } else {
      // Nothing to do.
      // Put itself to the idle list.
      return false;
    }
  }

  return true;
}

} // namespace core
} // namespace acto
