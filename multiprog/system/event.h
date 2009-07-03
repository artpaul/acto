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

#ifndef acto_event_h_F3FC653C947A45ab8159F41C677233FD
#define acto_event_h_F3FC653C947A45ab8159F41C677233FD

#include <memory>

namespace acto {

namespace core {

/** */
enum WaitResult {
    // -
    WR_ERROR,
    // -
    WR_SIGNALED,
    // Превышен установленный период ожидания
    WR_TIMEOUT
};


/** Событие */
class event_t {
public:
    event_t(const bool auto_reset = false);
    ~event_t();

public:
    ///
    void reset();
    ///
    void signaled();
    ///
    WaitResult wait();
    WaitResult wait(const unsigned int msec);

private:
    class impl;

    std::auto_ptr<impl>   m_pimpl;
};

} // namespace core

} // namespace acto

#endif // acto_event_h_F3FC653C947A45ab8159F41C677233FD
