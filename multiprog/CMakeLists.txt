
ADD_LIBRARY (acto-lib STATIC
    acto.cpp
)

SET_TARGET_PROPERTIES(acto-lib PROPERTIES
    COMPILE_FLAGS "-pthread")
