
#include "act_mswin.h"

namespace acto {

namespace core {

namespace impl {

// Количество инициализация данного пространства
static long     references = 0;

}; // namespace impl



///////////////////////////////////////////////////////////////////////////////////////////////////

thread_t* thread_t::instance = 0;

//-------------------------------------------------------------------------------------------------
thread_t::thread_t(const proc_t& proc, void* const param) :
    m_handle(0),
    m_id    (0),
    m_param (param),
    m_proc  (proc)
{
    m_handle = ::CreateThread(0, 0, &thread_t::thread_proc, this, 0, &m_id);
    // -
    if ( m_handle == 0 ) {
        // Возникла ошибка и поток не создался.
    }
}
//-------------------------------------------------------------------------------------------------
thread_t::~thread_t() {
    if (m_handle != 0) {
        // Дождаться завершения потока
        ::WaitForSingleObject(m_handle, 1000);
        // Закрыть дескриптор потока
        ::CloseHandle(m_handle);
    }
}
//-------------------------------------------------------------------------------------------------
void* thread_t::param() const {
    return m_param;
}

//-------------------------------------------------------------------------------------------------
thread_t* thread_t::current() {
    return instance;
}
//-------------------------------------------------------------------------------------------------
DWORD WINAPI thread_t::thread_proc(LPVOID lpParameter) {
    if (thread_t* const p_thread = static_cast< thread_t* >(lpParameter)) {
        if (!p_thread->m_proc.empty()) {
            // Связать экземпляр с потоком
            instance = p_thread;
            // Вызвать процедуру потока
            p_thread->m_proc();
            // Обнулить связь
            instance = 0;
        }
    }
    ::ExitThread( 0 );
}


//-------------------------------------------------------------------------------------------------
void sys_initialize() {
    // -
    if (impl::references == 0) {
    }
    // -
    impl::references++;
}

//-------------------------------------------------------------------------------------------------
void sys_finalize() {
    // -
}

}; // namespace core

} // namespace acto

