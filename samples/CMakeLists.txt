
add_executable (ping-pong
  "ping-pong.cpp"
)
target_link_libraries (ping-pong
  acto-lib
)

add_executable (cycles
  "cycles.cpp"
)
target_link_libraries (cycles
  acto-lib
)
