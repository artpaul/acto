add_executable (acto_test
    "main.cpp"
    "tests.cpp"
)

target_link_libraries (acto_test PRIVATE
    acto-lib
)

add_test (acto-test acto_test)
