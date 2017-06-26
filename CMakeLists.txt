cmake_minimum_required(VERSION 3.1.0)

project(GuitarBiro VERSION 1.0 LANGUAGES C)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/CMakeModules/")

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
pkg_check_modules(RSVG REQUIRED librsvg-2.0)
pkg_check_modules(CAIRO REQUIRED cairo)
include_directories(${GTK3_INCLUDE_DIRS} ${RSVG_INCLUDE_DIRS} ${CAIRO_INCLUDE_DIRS})
link_directories(${GTK3_LIBRARY_DIRS} ${RSVG_LIBRARY_DIRS} ${CAIRO_LIBRARY_DIRS})
add_definitions(${GTK3_CFLAGS_OTHER})

include_directories(include)
add_subdirectory(tests)

add_executable(guitarbiro src/main.c src/audio_init.c src/audio_record.c
		src/detect.c src/gui.c src/guitar.c src/period_estimator.c)
target_link_libraries(guitarbiro m soundio Threads::Threads ${GTK3_LIBRARIES}
		${RSVG_LIBRARIES} ${CAIRO_LIBRARIES})

enable_testing()
add_test(NAME check_period_estimator COMMAND check_period_estimator resources)
add_test(NAME check_guitar COMMAND check_guitar)

file(COPY resources DESTINATION .)
