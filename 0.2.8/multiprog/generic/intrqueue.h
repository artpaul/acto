
#ifndef intrqueue_h_b5eb85f3946b4e6ba71640998eed17b2
#define intrqueue_h_b5eb85f3946b4e6ba71640998eed17b2

#include <assert.h>

namespace acto {

namespace generics {

template <typename T>
class intrusive_queue_t {
    T*      m_head;
    T*      m_tail;
    size_t  m_size;

public:
    intrusive_queue_t()
        : m_head(NULL)
        , m_tail(NULL)
        , m_size(0)
    {
    }

public:
    /// Отцепить все элементы от очереди сохраняя порядок элементов
    T* detatch() {
        T* const result = m_head;

        m_head = m_tail = NULL;
        m_size = 0;

        return result;
    }
    ///
    bool empty() const {
        return m_head == NULL;
    }
    /// Указатель на первый элемент
    T* front() const {
        return m_head;
    }
    ///
    T* pop() {
        T* const result = this->remove(m_head, NULL);
        if (result)
            result->next = NULL;
        return result;
    }
    ///
    void push(T* item) {
        item->next = NULL;

        if (m_head == NULL) {
            m_head = m_tail = item;
        }
        else {
            m_tail->next = item;
            m_tail       = item;
        }
        ++m_size;
    }

    /// Удаляет указанный элемен из очереди
    ///
    /// \return Указатель на следующий за удаляемым элемент
    T* remove(T* const item, T* const prev) {
        if (m_head != NULL) {
            assert(m_size != 0);

            --m_size;

            if (item == m_head) {
                if (m_head == m_tail) {
                    m_head = m_tail = NULL;
                    return NULL;
                }
                else {
                    m_head = item->next;
                    return item->next;
                }
            }
            else if (item == m_tail) {
                assert(m_head != m_tail);

                m_tail = prev;
                return 0;
            }
            else {
                prev->next = item->next;
                return item->next;
            }
        }
        return NULL;
    }

    size_t size() const {
        return m_size;
    }
};

} // namespace generics

} // namespace acto

#endif // intrqueue_h_b5eb85f3946b4e6ba71640998eed17b2
