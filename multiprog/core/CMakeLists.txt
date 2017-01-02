ADD_LIBRARY (acto-core STATIC
    base.cpp
    runtime.cpp
    worker.cpp
    module.cpp
    services.cpp
)

TARGET_LINK_LIBRARIES (acto-core
    acto-system
)