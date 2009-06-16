
ADD_EXECUTABLE (ping-pong
    main.cpp)

TARGET_LINK_LIBRARIES (ping-pong
    acto-lib
    acto-core
    acto-system
    rt
    pthread
)
