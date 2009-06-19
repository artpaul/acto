
#ifndef acot_thread_pool_h_425878588d2e4b6cade43cf0383690b4
#define acot_thread_pool_h_425878588d2e4b6cade43cf0383690b4

#include <memory>

#include <generic/delegates.h>
#include <generic/intrlist.h>
#include <system/atomic.h>
#include <system/thread.h>
#include <system/event.h>

#include <core/struct.h>

namespace acto {

class thread_pool_t;

    // QoS
    // deadline, aprox-exec-time


/**
 */
class thread_worker_t : public core::intrusive_t< thread_worker_t > {
    typedef fastdelegate::FastDelegate< void (void*) >  callback_t;

public:
    thread_worker_t(thread_pool_t* const owner);
    ~thread_worker_t();

public:
    void start(callback_t cb, void* param);

private:
    void execute_loop(void* param);

private:
    callback_t                      m_callback;
    core::event_t                   m_event;
    thread_pool_t* const            m_owner;
    void*                           m_param;
    std::auto_ptr<core::thread_t>   m_thread;
    volatile bool                   m_active;
};

/**
 * Пул потоков - разделяемый ресурс между несколькими
 *               подсистемами библиотеки
 */
class thread_pool_t {
    friend class thread_worker_t;

    typedef fastdelegate::FastDelegate< void (void*) >  callback_t;
    typedef core::structs::queue_t< thread_worker_t >   idle_queue_t;

    static const unsigned int MAX_WORKERS = 512;
    static const unsigned int MIN_TIMEOUT = 2;

private:
    idle_queue_t    m_idles;
    atomic_t        m_count;
    int             m_processors;
    unsigned int    m_timeout;

private:
    inline thread_worker_t* create_worker() {
        return new thread_worker_t(this);
    }

    inline void delete_worker(thread_worker_t* const item) {
        delete item;
        // -
        atomic_decrement(&m_count);
    }

public:
    thread_pool_t()
        : m_count(0)
        , m_timeout(MIN_TIMEOUT)
    {
        m_processors = core::NumberOfProcessors();
    }

    ~thread_pool_t() {
        this->collect_all();

        assert(m_count == 0);
    }

    static thread_pool_t* instance() {
        static thread_pool_t   value;

        return &value;
    }

public:
    /// Удалить один простаивающий поток
    void collect_one() {
        // TN: Вызов данного метода может осуществляться из разных потоков
        if (thread_worker_t* const item = m_idles.pop())
            delete_worker(item);
    }
    /// Удалить все простаивающие потоки
    void collect_all() {
        // TN: Вызов данного метода может осуществляться из разных потоков
        core::structs::localstack_t< thread_worker_t > queue(m_idles.extract());
        // Удалить все потоки
        while (thread_worker_t* const item = queue.pop())
            delete_worker(item);
    }
    /// Вернуть осовободившийся ресурс обратно в пул
    void release(thread_worker_t* const worker) {
         m_idles.push(worker);
    }
    ///
    thread_worker_t* sync_allocate() {
        // TN: Вызов данного метода может осуществляться из разных потоков
        thread_worker_t* worker = m_idles.pop();

        if (worker == NULL) {
            if (m_count < (m_processors << 1))
                worker = this->create_worker();
            else {
                core::event_t   wait_event;

                wait_event.reset();
                // -
                const core::WaitResult result = wait_event.wait(1000 * m_timeout);
                // -
                worker = m_idles.pop();
                // -
                if (!worker && (result == core::WR_TIMEOUT)) {
                    // -
                    m_timeout += 2;
                    // -
                    if (m_count < MAX_WORKERS)
                        //FIXME: !!! Не атомарно, может превысить максимальное число
                        worker = create_worker();
                    else {
                        wait_event.reset();
                        wait_event.wait();
                    }
                }
                else
                    if (m_timeout > MIN_TIMEOUT)
                        m_timeout -= 2;
            }
        }
        return worker;
    }
};

} // namespace acto

#endif // acot_thread_pool_h_425878588d2e4b6cade43cf0383690b4
