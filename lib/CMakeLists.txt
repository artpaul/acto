add_library (acto-lib STATIC)

target_include_directories (acto-lib PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_sources (acto-lib PRIVATE
  "acto.cpp"
  "event.cpp"
  "runtime.cpp"
  "worker.cpp"
)
