add_executable(minst_test
    main.cpp
)

target_include_directories(minst_test PUBLIC
    ${CMAKE_SOURCE_DIR}/isa/
)

target_link_libraries(minst_test PRIVATE
    isa_blockisa
    GTest::gtest_main
)