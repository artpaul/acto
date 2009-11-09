
ADD_LIBRARY (zfs-client
    zfslib.cpp)

TARGET_LINK_LIBRARIES (zfs-client
    acto-lib
    acto-remote
    acto-libsocket)
