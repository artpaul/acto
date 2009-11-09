
ADD_EXECUTABLE (test-zfs
    main.cpp)

TARGET_LINK_LIBRARIES (test-zfs
    zfs-client
    acto-lib
    acto-remote
    acto-libsocket)
