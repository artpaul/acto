
ADD_EXECUTABLE (ping-pong
    main.cpp)

TARGET_LINK_LIBRARIES (ping-pong
    acto-system
    acto-core
    acto-ext
    acto-lib
    pthread
)
