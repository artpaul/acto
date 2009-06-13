
#ifndef alloc_h_A69B29FCE00541e5A9261BF093860CEF
#define alloc_h_A69B29FCE00541e5A9261BF093860CEF

namespace acto {

namespace core {

/** */
class allocator_t {
public:
    template <typename T>
    static inline T* allocate() {
        return new T();
    }

    template <typename T, typename P1>
    static inline T* allocate(P1 p1) {
        return new T(p1);
    }

    template <typename T, typename P1, typename P2>
    static inline T* allocate(P1 p1, P2 p2) {
        return new T(p1, p2);
    }
};

} // namespace core 

} // namepsace acto

#endif // alloc_h_A69B29FCE00541e5A9261BF093860CEF
