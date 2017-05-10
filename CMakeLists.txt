cmake_minimum_required(VERSION 3.1.0)

# TODO: Cambiare nome quando lo avrò deciso
project(Progetto)

find_package(Threads REQUIRED)

include_directories(include)
add_subdirectory(src)

add_executable(Progetto src/main.c src/audio_init.c src/audio_record.c)
target_link_libraries(Progetto soundio Threads::Threads)
