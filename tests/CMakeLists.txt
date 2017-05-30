include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../src)

find_package(Check REQUIRED)

# The linker gave error about pthread, at least on Debian, so include it manually
find_package(Threads REQUIRED)

include(CheckCSourceCompiles)
include(CheckCSourceRuns)
include(CheckFunctionExists)
include(CheckIncludeFile)
include(CheckIncludeFiles)
include(CheckLibraryExists)
include(CheckSymbolExists)
include(CheckTypeSize)

include_directories(${CHECK_INCLUDE_DIRS})

# Check headers
set(INCLUDES "")
macro(ck_check_include_file header var)
  check_include_files("${INCLUDES};${header}" ${var})
  if(${var})
    set(INCLUDES ${INCLUDES} ${header})
  endif(${var})
endmacro(ck_check_include_file)

ck_check_include_file("stdlib.h" HAVE_STDLIB_H)

# (The following are used in check.h. Regardless if they are used in
# the project, they will need to be checked in order to use Check).
check_type_size(intmax_t INTMAX_T)
check_type_size(uintmax_t UINTMAX_T)

check_type_size(pid_t PID_T)
if(NOT HAVE_PID_T)
	if(WIN32)
		set(pid_t "int")
	else(WIN32)
		MESSAGE(FATAL_ERROR "pid_t doesn't exist on this platform?")
	endif(WIN32)
endif(NOT HAVE_PID_T)

###############################################################################

add_executable(check_guitar check_guitar.c ../src/guitar.c)
target_link_libraries(check_guitar m ${CHECK_LIBRARIES} Threads::Threads)

add_executable(check_period_estimator check_period_estimator.c ../src/period_estimator.c)
target_link_libraries(check_period_estimator m ${CHECK_LIBRARIES} Threads::Threads)
