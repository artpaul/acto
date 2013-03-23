
ADD_LIBRARY (acto-system STATIC
    thread.cpp
    mutex.cpp
    event.cpp
    thread_pool.cpp
)

TARGET_LINK_LIBRARIES (acto-system
    rt
)

SET_TARGET_PROPERTIES(acto-system PROPERTIES
    COMPILE_FLAGS "-pthread")

SET_TARGET_PROPERTIES(acto-system PROPERTIES
    LINK_FLAGS "-pthread")
