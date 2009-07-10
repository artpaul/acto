
ADD_EXECUTABLE (test-thread
    main.cpp)

TARGET_LINK_LIBRARIES (test-thread
    acto-system
    rt
)

SET_TARGET_PROPERTIES(test-thread PROPERTIES
    COMPILE_FLAGS "-pthread")


SET_TARGET_PROPERTIES(test-thread PROPERTIES
    LINK_FLAGS "-pthread")
