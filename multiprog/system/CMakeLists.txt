
ADD_LIBRARY (acto-system STATIC
    thread.cpp
    mutex.cpp
    event.cpp
)

SET_TARGET_PROPERTIES(acto-system PROPERTIES
    COMPILE_FLAGS "-pthread")
