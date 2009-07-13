
ADD_LIBRARY (acto-remote STATIC
    client.cpp
    remote.cpp
)

TARGET_LINK_LIBRARIES(acto-remote
    acto-libsocket
    acto-system)

SET_TARGET_PROPERTIES(acto-remote PROPERTIES
    COMPILE_FLAGS "-pthread")

SET_TARGET_PROPERTIES(acto-remote PROPERTIES
    LINK_FLAGS "-pthread")
