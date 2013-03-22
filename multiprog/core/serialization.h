#pragma once

#include <cstddef>

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
