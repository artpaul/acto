
ADD_LIBRARY (acto-lib STATIC
    acto.cpp
)

TARGET_LINK_LIBRARIES (acto-lib
    acto-core
    acto-system
)