
ADD_LIBRARY (acto-remote STATIC
    remote.cpp
)

TARGET_LINK_LIBRARIES(acto-remote
    acto-libsocket
    acto-system)
