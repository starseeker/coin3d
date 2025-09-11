# Only storage-related threading components remain after C++17 migration
# Other threading primitives (mutex, condvar, thread, etc.) have been replaced with C++17 equivalents

# source files - includes C threading components still used by SoDB and storage
set(COIN_THREADS_FILES
	common.cpp
	storage.cpp
	recmutex.cpp
	recmutex_global_cxx17.cpp
	mutex.cpp
	condvar.cpp
	thread.cpp
)

# Files excluded from public API documentation, included in complete documentation.
set(COIN_THREADS_INTERNAL_FILES
	storagep.h
	threadsutilp.h
	recmutexp.h
	recmutex_global_cxx17.h
	mutexp.h
	condvarp.h
	threadp.h
)

# build library
if(COIN_BUILD_SINGLE_LIB)
	foreach(SRCFILE ${COIN_THREADS_FILES})
		set(COIN_SOURCE ${COIN_SOURCE} threads/${SRCFILE} CACHE INTERNAL "COIN_SOURCE")
	endforeach(SRCFILE)
else()
	add_library(threads OBJECT ${COIN_THREADS_FILES})
endif()

# define documentation
foreach(SRCFILE ${COIN_THREADS_FILES})
	set(COIN_DOCUMENTATION_FILES ${COIN_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/threads/${SRCFILE} CACHE INTERNAL "COIN_DOCUMENTATION_FILES")
endforeach(SRCFILE)
foreach(SRCFILE ${COIN_THREADS_INTERNAL_FILES})
	set(COIN_INTERNAL_DOCUMENTATION_FILES ${COIN_INTERNAL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/threads/${SRCFILE} CACHE INTERNAL "COIN_INTERNAL_DOCUMENTATION_FILES")
endforeach(SRCFILE)