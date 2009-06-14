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
// File: linux.h                                                             //
//     Обертка над API операционной системы Linux.                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef actosys_linux_h
#define actosys_linux_h



namespace acto {

namespace core {

class event_t;


/** */
enum WaitResult {
    // -
    WR_ERROR,
    // -
    WR_SIGNALED,
    // Превышен установленный период ожидания
    WR_TIMEOUT
};

/** */
class event_t {
public:
    void reset() {
        // -
    }

    void signaled() {
        // -
    }

    WaitResult wait() {
        return WR_ERROR;
    }

    WaitResult wait(const unsigned int msec) {
        return WR_ERROR;
    }
};

} // namespace core

} // namespace acto

#endif // actosys_linux_h
