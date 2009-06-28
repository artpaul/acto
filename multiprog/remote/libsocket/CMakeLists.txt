
ADD_LIBRARY (acto-libsocket STATIC
    runtime.cpp
    libsocket.cpp
)

SET_TARGET_PROPERTIES(acto-libsocket PROPERTIES
    COMPILE_FLAGS "-pthread")
