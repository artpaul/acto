
ADD_EXECUTABLE (cycles
    main.cpp)

TARGET_LINK_LIBRARIES (cycles
    acto-lib
    acto-ext
    acto-core
    acto-system
    rt
    pthread
)
