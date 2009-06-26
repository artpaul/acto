
ADD_EXECUTABLE (sotest
    main.cpp)

TARGET_LINK_LIBRARIES (sotest
    acto-lib
    acto-ext
    acto-core
    acto-remote
    acto-system
    rt
    pthread
)
