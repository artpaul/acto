
ADD_EXECUTABLE (zfs-master
    master.cpp
    filedb.cpp)

TARGET_LINK_LIBRARIES (zfs-master
    acto-lib
    acto-remote
    acto-libsocket)
