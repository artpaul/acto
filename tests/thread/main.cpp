#include <util/system/atomic.h>
#include <util/system/event.h>
#include <util/system/thread.h>
#include <util/system/thread_pool.h>

#include <cstdlib>
#include <stdio.h>

acto::core::event_t ev(true);

acto::atomic_t  count = 0;

static void execute(void* param) {
    acto::atomic_increment(&count);
    {
        printf("execute: %i\n", *((int*)param));
    }
    if (acto::atomic_decrement(&count) == 0)
        ev.signaled();

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
