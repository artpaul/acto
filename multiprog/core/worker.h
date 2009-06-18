///////////////////////////////////////////////////////////////////////////////
//                           The act-o Library                               //
//---------------------------------------------------------------------------//
// Copyright © 2007 - 2009                                                   //
//     Pavel A. Artemkin (acto.stan@gmail.com)                               //
// ------------------------------------------------------------------ -------//
// License:                                                                  //
//     Code covered by the MIT License.                                      //
//     The authors make no representations about the suitability of this     //
//     software for any purpose. It is provided "as is" without express or   //
//     implied warranty.                                                     //
///////////////////////////////////////////////////////////////////////////////

#ifndef actocore_worker_h
#define actocore_worker_h

#include <ctime>

#include <generic/intrlist.h>
#include <generic/delegates.h>

#include "struct.h"

namespace acto {

namespace core {

struct object_t;
struct package_t;


/**
 * Системный поток
 */
class worker_t : public intrusive_t< worker_t > {
public:
    typedef fastdelegate::FastDelegate< void (object_t* const) >    PushDelete;
    typedef fastdelegate::FastDelegate< void (worker_t* const) >    PushIdle;
    typedef fastdelegate::FastDelegate< void (package_t *const package) >    HandlePackage;

public:
    struct Slots {
        PushDelete      deleted;
        HandlePackage   handle;
        PushIdle        idle;
    };

public:
    worker_t(const Slots slots);
    ~worker_t();

public:
    // Поместить сообщение в очередь
    void assign(object_t* const obj, const clock_t slice);
    // -
    void wakeup();

private:
    void execute(void* param);

private:
    // Флаг активности потока
    volatile bool       m_active;
    // -
    event_t             m_event;
    // -
    object_t* volatile  m_object;
    // -
    clock_t             m_start;
    clock_t             m_time;
    // -
    const Slots         m_slots;
    // Экземпляр системного потока
    thread_t*           m_system;
};

} // namespace core

} // namespace acto

#endif // actocore_worker_h
