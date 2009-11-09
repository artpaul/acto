
ADD_EXECUTABLE (zfs-master
    master.cpp)

TARGET_LINK_LIBRARIES (zfs-master
    acto-lib
    acto-remote
    acto-libsocket)
