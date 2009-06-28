
ADD_EXECUTABLE (ping-pong
    main.cpp)

TARGET_LINK_LIBRARIES (ping-pong
    acto-lib
    acto-ext
    acto-core
    acto-system
    rt
)

SET_TARGET_PROPERTIES(ping-pong PROPERTIES
    COMPILE_FLAGS "-pthread")


SET_TARGET_PROPERTIES(ping-pong PROPERTIES
    LINK_FLAGS "-pthread")
