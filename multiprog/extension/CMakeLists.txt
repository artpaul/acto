
ADD_LIBRARY (acto-ext STATIC
    services.cpp
)

SET_TARGET_PROPERTIES(acto-ext PROPERTIES
    COMPILE_FLAGS "-pthread")

SET_TARGET_PROPERTIES(acto-ext PROPERTIES
    LINK_FLAGS "-pthread")
