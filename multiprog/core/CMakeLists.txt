ADD_LIBRARY (acto-core STATIC
    base.cpp
    message.cpp
    runtime.cpp
    worker.cpp
    module.cpp
    services.cpp
)

TARGET_LINK_LIBRARIES (acto-core
    acto-system
)

SET_TARGET_PROPERTIES(acto-core PROPERTIES
    COMPILE_FLAGS "-pthread")

SET_TARGET_PROPERTIES(acto-core PROPERTIES
    LINK_FLAGS "-pthread")
