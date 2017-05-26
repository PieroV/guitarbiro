include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../src)

find_package(Check REQUIRED)
include_directories(${CHECK_INCLUDE_DIRS})

# Link gave error about pthread, at least on Debian, so include it manually
find_package(Threads REQUIRED)

add_executable(check_period_estimator check_period_estimator.c ../src/period_estimator.c)
target_link_libraries(check_period_estimator m ${CHECK_LIBRARIES} Threads::Threads)

add_executable(check_guitar check_guitar.c ../src/guitar.c)
target_link_libraries(check_guitar m ${CHECK_LIBRARIES} Threads::Threads)
