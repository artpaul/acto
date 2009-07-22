
#include <assert.h>

// map(f, xs) :: T1 -> T2
// reduce(f, xs)
// filter(f, xs) :: T1 -> T1

// xs - список

// map, reduce (fold), filter

// (T1, T2, ...) -> R
// R - список, единичное значение, функция

class value_t {
public:
    value_t() {
    }

    value_t(void*) {
    }

public:
    bool empty() const {
        return true;
    }
};


void append(value_t& xs, const value_t& a) {
    // -
}



class func_t {
public:
    virtual bool next(value_t& result) = 0;
    ///
    virtual func_t* clone() const = 0;
};


class function {
    func_t* m_closure;

public:
    function(func_t* closure)
        : m_closure(closure)
    {
    }

    value_t call(value_t& p1) {
        //m_closure->call();
        return value_t();
    }
};

class list {
public:
    bool empty() const {
        return true;
    }

    value_t pop() {
        return value_t();
    }
};

class map {
    function    m_f;
    list        m_xs;

public:
    map(const function& f, list xs)
        : m_f(f)
        , m_xs(xs)
    {
    }

    virtual bool next(value_t& result) {
        if (m_xs.empty())
            return false;
        else 
            result = m_f.call(m_xs.pop());
        return false;
    }
};


template <typename F, typename P1, typename P2>
value_t call(P1 p1, P2 p2) {
    return value_t(new F(p1, p2));
}

template <typename F>
function functor() {
    return function(new F());
}


class increment : public func_t {
    value_t m_param;
    int     m_s;
public:
    virtual bool next(value_t& result) {
        if (m_s == 1)
            return false;
        else
            result = m_param;
        //out = in + 1;
        return true;
    }

    virtual func_t* clone() const {
        return new increment();
    }
};


int main() {
    // -> Это описание программы, которое будет выполняться на каждом хосте
    value_t result = call< map >(functor< increment >(), list());


    return 0;
}
