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
// File: act_struct.h                                                        //
//     Специальные классы структур, специально адаптированные                //
//     под задачи библиотеки.                                                //
///////////////////////////////////////////////////////////////////////////////

#if !defined ( __multiprog__act_struct_h__ )
#define __multiprog__act_struct_h__

#include <assert.h>

#include <generic/sequence.h>

#include <system/platform.h>
#include <system/atomic.h>
#include <system/mutex.h>


namespace acto {

namespace core {

namespace structs {

///////////////////////////////////////////////////////////////////////////////
// Desc:
template <typename T, typename Guard = mutex_t>
class queue_t {
    T*      m_tail;
    Guard   m_cs;

public:
    queue_t() {
        m_tail = NULL;
    }

    sequence_t<T> extract() {
        MutexLocker lock(m_cs);

        if (m_tail) {
           T* const head = m_tail->next;

           m_tail->next = NULL;
           m_tail       = NULL;
           return head;
        }
        return NULL;
    }

    void push(T* const node) {
        MutexLocker lock(m_cs);

        if (m_tail) {
            node->next   = m_tail->next;
            m_tail->next = node;
        }
        else
            node->next = node;
        m_tail = node;
    }

    T* pop() {
        MutexLocker lock(m_cs);

        if (m_tail) {
            T* const result = m_tail->next;

            if (result == m_tail)
                m_tail = NULL;
            else
                m_tail->next = m_tail->next->next;
            return result;
        }
        return NULL;
    }

    bool empty() const {
        return (m_tail == NULL);
    }
};


///////////////////////////////////////////////////////////////////////////////
// Desc:
//    Используемый алгоритм будет корректно работать, если только
//    единственный поток извлекает объекты из стека и удаляет их.
template <typename T>
class stack_t {
public:
    typedef T       node_t;

public:
    stack_t() :
        m_head( 0 )
    {
    }

    bool empty() const {
        return (m_head == 0);
    }

    sequence_t<T> extract() {
        node_t*     top;

        while (true) {
            top = m_head;
            if (top == 0)
                return 0;
            if (atomic_compare_and_swap((atomic_t*)&m_head, (long)top, 0))
                return top;
        }
    }

    void push(node_t* const node) {
        while (true) {
            node_t* const top = m_head;

            node->next = top;
            if (atomic_compare_and_swap((atomic_t*)&m_head, (long)top, (long)node))
                return;
        }
    }

    void push(sequence_t<T> seq) {
        while (T* const item = seq.pop())
            this->push(item);
    }

    node_t* pop() {
        while (true) {
            node_t* const top = m_head;

            if (top == NULL)
                return NULL;
            else {
                node_t* const next = top->next;

                if (atomic_compare_and_swap((atomic_t*)&m_head, (long)top, (long)next))
                    return top;
            }
        }
    }

private:
    node_t* volatile m_head;
};

///////////////////////////////////////////////////////////////////////////////
// Desc:
template <typename T>
class localstack_t {
public:
    typedef T       node_t;

public:
    localstack_t() :
        m_head( 0 )
    {
    }

    ~localstack_t() {
        assert(m_head == 0);
    }

    localstack_t(sequence_t<T> seq) :
        m_head( seq.extract() )
    {
    }

    bool empty() const {
        return (m_head == 0);
    }

    sequence_t<T> extract() {
        sequence_t<T> top = sequence_t<T>(m_head);
        m_head = 0;
        return top;
    }

    void push(node_t* const node) {
        node->next = m_head;
        m_head     = node;
    }

    void push(sequence_t<T> seq) {
        while (T* const item = seq.pop())
            this->push(item);
    }

    node_t* pop() {
        node_t* const result = m_head;
        // -
        if (result)
            m_head = result->next;
        return result;
    }

private:
    node_t*     m_head;
};

}; // namespace structs

}; // namespace core

}; // namespace acto


#endif // __multiprog__act_struct_h__
