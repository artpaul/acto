
ADD_EXECUTABLE (zfs-master
    master.cpp
    filedb.cpp
    chunk_handler.cpp
    client_handler.cpp)

TARGET_LINK_LIBRARIES (zfs-master
    acto-lib
    acto-remote
    acto-libsocket)

SET_TARGET_PROPERTIES(zfs-master PROPERTIES
    COMPILE_FLAGS "-pthread")

SET_TARGET_PROPERTIES(zfs-master PROPERTIES
    LINK_FLAGS "-pthread")
