cmake_minimum_required(VERSION 3.1.0)

# TODO: Cambiare nome quando lo avrò deciso
project(Progetto LANGUAGES C)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
pkg_check_modules(RSVG REQUIRED librsvg-2.0)
pkg_check_modules(CAIRO REQUIRED cairo)
include_directories(${GTK3_INCLUDE_DIRS} ${RSVG_INCLUDE_DIRS} ${CAIRO_INCLUDE_DIRS})
link_directories(${GTK3_LIBRARY_DIRS} ${RSVG_LIBRARY_DIRS} ${CAIRO_LIBRARY_DIRS})
add_definitions(${GTK3_CFLAGS_OTHER})

include_directories(include)

add_executable(Progetto src/main.c src/gui.c)
target_link_libraries(Progetto ${GTK3_LIBRARIES} ${RSVG_LIBRARIES} ${CAIRO_LIBRARIES} Threads::Threads)

file(COPY resources DESTINATION .)
