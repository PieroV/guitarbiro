include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../src)

find_package(Check REQUIRED)
include_directories(${CHECK_INCLUDE_DIRS})

# Link gave error about pthread, at least on Debian, so include it manually
find_package(Threads REQUIRED)

set(TEST_SOURCES
	check_period_estimator.c
	../src/period_estimator.c
)

add_executable(check_period_estimator ${TEST_SOURCES})
target_link_libraries(check_period_estimator m ${CHECK_LIBRARIES} Threads::Threads)
