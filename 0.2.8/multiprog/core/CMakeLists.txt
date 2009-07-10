
ADD_LIBRARY (acto-core STATIC
    types.cpp
    message.cpp
    runtime.cpp
    ../modules/main/worker.cpp
    ../modules/main/module.cpp
)

TARGET_LINK_LIBRARIES (acto-core
    acto-system
)

SET_TARGET_PROPERTIES(acto-core PROPERTIES
    COMPILE_FLAGS "-pthread")

SET_TARGET_PROPERTIES(acto-core PROPERTIES
    LINK_FLAGS "-pthread")
