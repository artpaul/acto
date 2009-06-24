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

#ifndef acto_queue_h_5BC02FC754954131B69D316CACBCFE1A
#define acto_queue_h_5BC02FC754954131B69D316CACBCFE1A

#include <assert.h>

#include <system/mutex.h>

#include "sequence.h"

namespace acto {

namespace generics { 

/** 
 */
template <typename T, typename Guard = core::mutex_t>
class queue_t {
    T*      m_tail;
    Guard   m_cs;

public:
    queue_t() {
        m_tail = NULL;
    }

    sequence_t<T> extract() {
        core::MutexLocker lock(m_cs);

        if (m_tail) {
           T* const head = m_tail->next;

           m_tail->next = NULL;
           m_tail       = NULL;
           return head;
        }
        return NULL;
    }

    void push(T* const node) {
        core::MutexLocker lock(m_cs);

        if (m_tail) {
            node->next   = m_tail->next;
            m_tail->next = node;
        }
        else
            node->next = node;
        m_tail = node;
    }

    T* pop() {
        core::MutexLocker lock(m_cs);

        if (m_tail) {
            T* const result = m_tail->next;

            if (result == m_tail)
                m_tail = NULL;
            else
                m_tail->next = m_tail->next->next;
            // -
            result->next = NULL;
            return result;
        }
        return NULL;
    }

    bool empty() const {
        return (m_tail == NULL);
    }
};

} // namepsace generics

} // namespace acto

#endif // acto_queue_h_5BC02FC754954131B69D316CACBCFE1A
