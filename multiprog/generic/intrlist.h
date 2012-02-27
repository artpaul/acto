#ifndef intrlist_h_205186795EC84c8cBE78E2C47CF69AC1
#define intrlist_h_205186795EC84c8cBE78E2C47CF69AC1

namespace acto {

namespace core {

/** Интрузивный элемент списка */
template <typename T>
struct intrusive_t {
    T*  next;
};

} // namespace core

} // namespace acto

#endif // intrlist_h_205186795EC84c8cBE78E2C47CF69AC1
