cmake_minimum_required(VERSION 3.0)

# TODO: Cambiare nome quando lo avrò deciso
project (Progetto)

include_directories(include)
add_subdirectory(src)

add_executable(Progetto src/main.c src/audio_init.c src/audio_record.c)
target_link_libraries(Progetto soundio)
