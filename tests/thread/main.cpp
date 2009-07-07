
#include <system/thread_pool.h>
//#include <system/atomic.h>
//#include <system/event.h>
#include <system/mutex.h>

#include <string>

acto::core::mutex_t cs;
//acto::core::event_t ev(true);

struct data_t {
    std::auto_ptr<int>  value;

    data_t(int i) : value(new int(i)) {

    }
};

static void execute(void* param) {
    std::string s("execute");
    data_t* num = (data_t*)param;
    if (rand() % 2) {
        acto::core::MutexLocker lock(cs);
        printf("mutex\n");
    }

    printf("%s: %i\n", s.c_str(), *(num->value.get()));
    delete num;
}

int main() {
    for (size_t i = 1; i <= 1000000; ++i) {
        for (size_t j = 0; j < 10; ++j)
            acto::thread_pool_t::instance()->queue_task(&execute, new data_t(i));
        usleep(5000);
    }

    return 0;
}
