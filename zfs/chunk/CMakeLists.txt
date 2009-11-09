
ADD_EXECUTABLE (zfs-chunk
    chunk.cpp
    client.cpp)

TARGET_LINK_LIBRARIES (zfs-chunk
    acto-lib
    acto-remote
    acto-libsocket)
