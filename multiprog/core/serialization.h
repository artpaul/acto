
#ifndef acto_core_serialization_h_4C3F226E1E554aedB52F0EE5648C87C2
#define acto_core_serialization_h_4C3F226E1E554aedB52F0EE5648C87C2

namespace acto {

struct msg_t;

/**
 */
class stream_t {
public:
    ///
    virtual size_t read (void* buf, size_t size) {
        return 0;
    }
    ///
    virtual void write(const void* buf, size_t size) = 0;
};

/**
 */
class serializer_t {
public:
    virtual ~serializer_t() { }

public:
    virtual void read(msg_t* const msg, stream_t* const s) = 0;
    ///
    virtual void write(const msg_t* const msg, stream_t* const s) = 0;
};

} // namepsace acto

#endif // acto_core_serialization_h_4C3F226E1E554aedB52F0EE5648C87C2
