
ADD_EXECUTABLE (cycles
    main.cpp)

TARGET_LINK_LIBRARIES (cycles
    acto-lib
    rt
)

SET_TARGET_PROPERTIES(cycles PROPERTIES
    COMPILE_FLAGS "-pthread")

SET_TARGET_PROPERTIES(cycles PROPERTIES
    LINK_FLAGS "-pthread")
