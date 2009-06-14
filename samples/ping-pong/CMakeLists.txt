
ADD_EXECUTABLE (ping-pong
    main.cpp)

TARGET_LINK_LIBRARIES (ping-pong
    acto-lib
    acto-ext
    acto-core
    acto-system
    pthread
)
