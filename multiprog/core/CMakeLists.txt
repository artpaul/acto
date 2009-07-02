
ADD_LIBRARY (acto-core STATIC
    types.cpp
    message.cpp
    runtime.cpp
    ../modules/main/worker.cpp
    ../modules/main/module.cpp
)

SET_TARGET_PROPERTIES(acto-core PROPERTIES
    COMPILE_FLAGS "-pthread")
