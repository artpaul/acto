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

#ifndef acto_thread_h_274B1DD8BD844a7dAABB1218F394DC83
#define acto_thread_h_274B1DD8BD844a7dAABB1218F394DC83

#include <memory>
#include <exception>

namespace acto {

namespace core {

/** */
class thread_exception : public std::exception {
};

/**
 */
class thread_t {
public:
    typedef void (*callback_t)(void*);

    thread_t(const callback_t proc, void* const param = 0);
    ~thread_t();

public:
    /// Ждать завершения текущего потока
    void join();
    /// Пользовательские данные, связанные с текущим потоком
    void* param() const;

private:
    class impl;

    std::auto_ptr<impl> m_pimpl;
};

} // namespace core

} // namespace acto

#endif // acto_thread_h_274B1DD8BD844a7dAABB1218F394DC83