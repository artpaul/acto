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
//---------------------------------------------------------------------------//
// File: sys_mswin.h                                                         //
//     Обертка над API операционной системы MS Windows.                      //
///////////////////////////////////////////////////////////////////////////////

#if !defined __multiprog__sys_mswin_h__
#define __multiprog__sys_mswin_h__

#include <system/platform.h>


namespace acto {

namespace core {


///////////////////////////////////////////////////////////////////////////////////////////////////
//                                ПРИМИТИВЫ СИНХРОНИЗАЦИИ                                        //
///////////////////////////////////////////////////////////////////////////////////////////////////

/** */
enum WaitResult {
    // -
    WR_ERROR,
    // -
    WR_SIGNALED,
    // Превышен установленный период ожидания
    WR_TIMEOUT
};


///////////////////////////////////////////////////////////////////////////////
// Desc: Событие.
class event_t {
public:
    event_t() {
        m_handle = ::CreateEvent(0, TRUE, TRUE, 0);
    }

    ~event_t() {
        ::CloseHandle( m_handle );
    }

public:
    void reset() {
        ::ResetEvent(m_handle);
    }

    void signaled() {
        ::SetEvent(m_handle);
    }

    WaitResult wait() {
        switch (::WaitForSingleObject(m_handle, INFINITE)) {
        case WAIT_OBJECT_0 :
            return WR_SIGNALED;
        case WAIT_TIMEOUT :
            return WR_TIMEOUT;
        }
        return WR_ERROR;
    }

    WaitResult wait(const unsigned int msec) {
        switch (::WaitForSingleObject(m_handle, msec)) {
        case WAIT_OBJECT_0 :
            return WR_SIGNALED;
        case WAIT_TIMEOUT :
            return WR_TIMEOUT;
        }
        return WR_ERROR;
    }

private:
    HANDLE  m_handle;
};

}; // namespace core

}; // namespace acto

#endif // __multiprog__sys_mswin_h__
