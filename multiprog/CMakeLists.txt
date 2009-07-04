
ADD_LIBRARY (acto-lib STATIC
    acto.cpp
)

TARGET_LINK_LIBRARIES (acto-lib
    acto-core
    acto-system
    rt
)

SET_TARGET_PROPERTIES(acto-lib PROPERTIES
    COMPILE_FLAGS "-pthread")

SET_TARGET_PROPERTIES(acto-lib PROPERTIES
    LINK_FLAGS "-pthread")
