
ADD_LIBRARY (acto-core STATIC
    core.cpp
    message.cpp
    worker.cpp
    thread_pool.cpp
    module.cpp
    runtime.cpp
)

SET_TARGET_PROPERTIES(acto-core PROPERTIES
    COMPILE_FLAGS "-pthread")
