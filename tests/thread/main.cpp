#include <util/event.h>
#include <util/thread_pool.h>

#include <atomic>
#include <cstdlib>
#include <stdio.h>
#include <thread>

acto::core::event_t ev(true);

std::atomic<long> count(0);

static void execute(void* param) {
    ++count;
    {
        printf("execute: %i\n", *((int*)param));
    }
    if (--count == 0) {
        ev.signaled();
    }

    delete (int*)param;
}

int main() {
    //acto::core::thread_t* th[200];
    for (size_t i = 1; i <= 5; ++i) {
        for (size_t j = 0; j < 1500; ++j)
            //th[j] = new acto::core::thread_t(&execute, new int(i));
            acto::thread_pool_t::instance()->queue_task(&execute, new int(i));
        //printf("loop\n");
        if (i != 5)
            ev.wait();
        //for (size_t j = 0; j < 200; ++j)
        //    delete th[j];
    }
    //printf(":end\n");
    return 0;
}
