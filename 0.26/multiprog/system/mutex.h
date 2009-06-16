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

#ifndef acto_mutex_h_2BA91EE9AA574deaBF6884ADB355901E
#define acto_mutex_h_2BA91EE9AA574deaBF6884ADB355901E

#include <memory>

namespace acto {

namespace core {

/** */
class mutex_t {
public:
    mutex_t();
    ~mutex_t();

public:
    /// Захватить мютекс
    void acquire();
    /// Освободить мютекс
    void release();

private:
    class impl;

    std::auto_ptr<impl> m_pimpl;
};


/** */
class MutexLocker {
public:
    MutexLocker(mutex_t& mutex)
        : m_mutex(mutex)
    {
        m_mutex.acquire();
    }

    ~MutexLocker() {
        m_mutex.release();
    }

private:
    mutex_t&  m_mutex;
};

} // namespace core

} // namespace acto

#endif // acto_mutex_h_2BA91EE9AA574deaBF6884ADB355901E
