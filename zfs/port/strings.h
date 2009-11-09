
#ifndef __util_cl_strings_h__
#define __util_cl_strings_h__

#include <ctype.h>
#include <memory.h>
#include <stddef.h>
#include <string.h>
#include <stdexcept>

#include "pointers.h"


namespace cl {


namespace errors {

class generic : public std::exception {
};

/** Выход за границы массивов и ограничителей */
class outofbound : public generic {
};

}; // namespace errors

}; //namespace cl


#include "chariterator.h"


namespace cl {

/**
 *  Базовый тип для строк
 */
template <typename Element>
class StringT {
    Element*    m_data;     // Данные
    size_t      m_size;     // Размер данных в символах
    size_t      m_capacity; // Объем выделенной памяти в байтах

    ///
    void allocateAndCopy(const void* data, size_t size) {
        m_capacity   = size + 1;
        m_size       = size;
        m_data       = new Element[m_capacity];
        memcpy(m_data, data, size);
        m_data[size] = 0;
    }

    void clear() {
        if (m_data)
            delete [] m_data;
        m_data     = 0;
        m_size     = 0;
        m_capacity = 0;
    }

    inline void initialize() {
        m_data     = 0;
        m_size     = 0;
        m_capacity = 0;
    }

public:
    StringT() {
        initialize();
    }

    StringT(const StringT& rhs) {
        initialize();
        if (rhs.m_data && rhs.m_size)
            allocateAndCopy(rhs.m_data, rhs.m_size);
    }

    StringT(const Element* str, const size_t len) {
        initialize();
        allocateAndCopy(str, len);
    }

    explicit StringT(const Element* str) {
        initialize();
        allocateAndCopy(str, strlen(str));
    }

    explicit StringT(const const_char_iterator& ci) {
        initialize();
        if (ci.valid())
            allocateAndCopy(~ci, ci.size());
    }

    ~StringT() {
        this->clear();
    }

    ///
    const static size_t npos = (size_t)(-1);

public:
    /// Последний символ в строке
    Element back() const {
        if (this->empty())
            return '\0';
        return m_data[m_size-1];
    }
    /// Содержит ли строка что-то
    bool empty() const {
        return !m_data || m_size == 0;
    }
    /// Первый символ в строке
    Element front() const {
        if (this->empty())
            return '\0';
        return m_data[0];
    }
    /// Размер строки в символах
    size_t size() const {
        return m_size;
    }

    /// Получить подстроку начиная с символа pos длинной len
    StringT substr(size_t pos, size_t len) const {
        if (!m_data || pos >= m_size)
            return StringT();
        // -
        if (len == npos) {
            return StringT(m_data + pos, m_size - pos);
        }
        else if ((pos + len) < m_size) {
            return StringT(m_data + pos, len);
        }
        return StringT();
    }

    /// Привести символы строки к нижнему регистру
    void tolower() {
        if (m_data) {
            for (size_t i = 0; i < m_size; ++i)
                *(m_data + i) = ::tolower(*(m_data + i));
        }
    }

    StringT& operator = (const StringT& rhs) {
        if (this == &rhs)
            return *this;
        // Если назначается пустая строка
        if (!rhs.m_data || rhs.m_size == 0) {
            this->clear();
            return *this;
        }
        // Скопировать данные из присваемоей строки
        if (m_data == 0) {
            this->allocateAndCopy(rhs.m_data, rhs.m_size);
        }
        else if (m_capacity < rhs.m_size) {
            delete [] m_data;
            this->allocateAndCopy(rhs.m_data, rhs.m_size);
        }
        else {
            // Очистить память, если новая строка меньше
            // текущей более чем на 10 %
            if ((10 * this->m_capacity) / rhs.m_size > 11) {
                delete [] m_data;
                this->allocateAndCopy(rhs.m_data, rhs.m_size);
            }
            else {
                m_size = rhs.m_size;
                // Скопировать данные вместе с завершающим нулем
                memcpy(m_data, rhs.m_data, (m_size + 1));
            }
        }
        return *this;
    }

    StringT& operator = (const char* rhs) {
        const size_t len = strlen(rhs);
        // Скопировать данные из присваемоей строки
        if (len == 0) {
            this->clear();
        }
        else if (m_data == 0) {
            this->allocateAndCopy(rhs, len);
        }
        else if (m_size < len) {
            delete [] m_data;
            this->allocateAndCopy(rhs, len);
        }
        else {
            // Очистить память, если новая строка меньше
            // текущей более чем на 10 %
            if ((10 * this->m_capacity) / len > 11) {
                delete [] m_data;
                this->allocateAndCopy(rhs, len);
            }
            else {
                m_size = len;
                // Скопировать данные вместе с завершающим нулем
                strcpy(m_data, rhs);
            }
        }
        return *this;
    }

    Element& operator [] (int index) {
        return m_data[index];
    }

    const Element& operator [] (int index) const {
        return m_data[index];
    }

    const Element* operator ~ () const {
        return m_data;
    }

    bool operator == (const StringT& rhs) const {
        if (this == &rhs)
            return true;
        if (this->m_size == 0 || rhs.m_size == 0)
            return false;
        return strcmp(this->m_data, rhs.m_data) == 0;
    }

    bool operator != (const StringT& rhs) const {
        return !this->operator == (rhs);
    }

    bool operator == (const char* rhs) const {
        if (this->m_size == 0 || rhs == 0)
            return false;
        return strcmp(this->m_data, rhs) == 0;
    }

    bool operator != (const char* rhs) const {
        return !this->operator == (rhs);
    }

    bool operator < (const StringT& rhs) const {
        if (this->m_size == 0 || rhs.m_size == 0)
            return false;
        return strcmp(this->m_data, rhs.m_data) < 0;
    }

    StringT operator += (const StringT& rhs) {
        if (rhs.m_data && rhs.m_size != 0) {
            if (!this->m_data || this->m_size == 0) {
                this->clear();
                this->allocateAndCopy(rhs.m_data, rhs.m_size);
            }
            else {
                const size_t combined = this->m_size + rhs.m_size;
                // -
                if (combined < m_capacity) {
                    memcpy(m_data + m_size, rhs.m_data, rhs.m_size);
                    m_size = combined;
                }
                else {
                    char* const data = new char[combined + 1];
                    // -
                    memcpy(data, m_data,  m_size);
                    memcpy(data + m_size, rhs.m_data, rhs.m_size);
                    // -
                    m_capacity = combined + 1;
                    m_size     = combined;
                    // -
                    delete [] m_data;
                    m_data = data;
                }
                m_data[m_size] = 0;
            }
        }
        return *this;
    }

    StringT operator += (const char* rhs) {
        if (rhs != 0) {
            const size_t len = strlen(rhs);
            // -
            if (!this->m_data || this->m_size == 0) {
                this->clear();
                this->allocateAndCopy(rhs, len);
            }
            else {
                const size_t combined = this->m_size + len;
                // -
                if (combined < m_capacity) {
                    memcpy(m_data + m_size, rhs, len);
                    m_size = combined;
                }
                else {
                    char* const data = new char[combined + 1];
                    // -
                    memcpy(data, m_data,  m_size);
                    memcpy(data + m_size, rhs, len);
                    // -
                    m_capacity = combined + 1;
                    m_size     = combined;
                    // -
                    delete [] m_data;
                    m_data = data;
                }
                m_data[m_size] = 0;
            }
        }
        return *this;
    }
};

/* Строка однобайтных символов - ASCII */
typedef StringT<char>  string;


//template <typename T>
//StringT<T> operator + (const StringT<T>& lhs, const StringT<T>& rhs) {
//    return StringT<T>();
//}

}; // namespace cl

#endif // __util_cl_strings_h__
