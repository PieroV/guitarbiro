cmake_minimum_required(VERSION 3.1.0)

# TODO: Cambiare nome quando lo avrò deciso
project(Progetto VERSION 1.0 LANGUAGES C)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/CMakeModules/")

find_package(Threads REQUIRED)

include_directories(include)
add_subdirectory(src)
add_subdirectory(tests)

add_executable(Progetto src/main.c src/audio_init.c src/audio_record.c
		src/guitar.c src/period_estimator.c)
target_link_libraries(Progetto m soundio Threads::Threads)

enable_testing()
add_test(NAME check_period_estimator COMMAND check_period_estimator)
add_test(NAME check_guitar COMMAND check_guitar)

file(COPY resources DESTINATION .)
