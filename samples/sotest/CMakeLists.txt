
ADD_EXECUTABLE (sotest
    main.cpp)

TARGET_LINK_LIBRARIES (sotest
    acto-lib
    acto-ext
    acto-remote
)

SET_TARGET_PROPERTIES(sotest PROPERTIES
    COMPILE_FLAGS "-pthread")


SET_TARGET_PROPERTIES(sotest PROPERTIES
    LINK_FLAGS "-pthread")
