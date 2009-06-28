
ADD_EXECUTABLE (sotest
    main.cpp)

TARGET_LINK_LIBRARIES (sotest
    acto-lib
    acto-ext
    acto-core
    acto-system
    acto-remote
    rt
)

SET_TARGET_PROPERTIES(sotest PROPERTIES
    COMPILE_FLAGS "-pthread")


SET_TARGET_PROPERTIES(sotest PROPERTIES
    LINK_FLAGS "-pthread")
