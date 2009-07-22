
ADD_EXECUTABLE (mr
    main.cpp)

TARGET_LINK_LIBRARIES (mr
    acto-lib
    acto-remote
)

SET_TARGET_PROPERTIES(mr PROPERTIES
    COMPILE_FLAGS "-pthread")


SET_TARGET_PROPERTIES(mr PROPERTIES
    LINK_FLAGS "-pthread")
