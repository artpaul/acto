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

#include <system/act_mswin.h>
#include <generic/ptr.h>

// Структуры, специально адаптированные под задачи библиотеки

namespace acto {

namespace core {

namespace structs {

// Desc: Элемент списка
template <typename T>
struct item_t {
    T*      next;
};

template <typename T>
struct sequence_t {
    T*      head;

public:
    sequence_t(T* const item) : head(item) { }

    T* extract() {
        T* const result = head;
        head = 0;
        return result;
    }

    T* pop() {
        T* const result = head;
        // -
        if (result) {
            head         = result->next;
            result->next = 0;
        }
        // -
        return result;
    }
};


///////////////////////////////////////////////////////////////////////////////
// Desc:
template <typename T, typename Guard = system::section_t>
class queue_t {
public:
    typedef T       node_t;

public:
    queue_t() {
        head = 0;
        tail = 0;
    }

    void push(node_t* const node) {
        system::MutexLocker lock(m_cs);
        // -
        if (tail) {
            tail->next = node;
            node->next = 0;
            tail = node;
        }
        else {
            node->next = 0;
            head = node;
            tail = node;
        }
    }

    node_t* pop() {
        system::MutexLocker lock(m_cs);
        // -
        node_t* const result = head;
        // -
        if (head) {
            head = head->next;
            // -
            if (head == 0)
                tail = 0;
        }
        // -
        if (result)
            result->next = 0;
        return result;
    }

    bool empty() const {
        return (head == 0);
    }

private:
    node_t* volatile    head;
    node_t* volatile    tail;
    // -
    mutable Guard   m_cs;
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

        while(true) {
            top = m_head;
            if (top == 0)
                return 0;
            if (system::AtomicCompareExchangePointer((volatile PVOID*)&m_head, 0, top) == top)
                return top;
        }
    }

    void push(node_t* const node) {
        node_t*     top;

        while (true) {
            top = m_head;
            node->next = top;
            if (system::AtomicCompareExchangePointer((volatile PVOID*)&m_head, node, top) == top)
                return;
        }
    }

    void push(sequence_t<T> seq) {
        while (T* const item = seq.pop())
            this->push(item);
    }

    node_t* pop() {
        node_t*     top;
        node_t*     next;

        while (true) {
            top = m_head;
            if (top == 0)
                return 0;
            next = top->next;
            if (system::AtomicCompareExchangePointer((volatile PVOID*)&m_head, next, top) == top)
                return top;
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
