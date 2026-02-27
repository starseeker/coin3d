# Only storage-related threading components remain after C++17 migration
# Other threading primitives (mutex, condvar, thread, etc.) have been replaced with C++17 equivalents

# source files - includes C threading components still used by SoDB and storage
set(OBOL_THREADS_FILES
	common.cpp
	storage.cpp
	storage_cxx17.cpp
	SbStorage.cpp
	recmutex.cpp
	recmutex_global_cxx17.cpp
	mutex.cpp
	condvar.cpp
	thread.cpp
)

# Files excluded from public API documentation, included in complete documentation.
set(OBOL_THREADS_INTERNAL_FILES
	storagep.h
	storage_cxx17.h
	recmutexp.h
	recmutex_global_cxx17.h
	mutexp.h
	condvarp.h
	threadp.h
)

# build library
if(OBOL_BUILD_SINGLE_LIB)
	foreach(SRCFILE ${OBOL_THREADS_FILES})
		set(OBOL_SOURCE ${OBOL_SOURCE} threads/${SRCFILE} CACHE INTERNAL "OBOL_SOURCE")
	endforeach(SRCFILE)
else()
	add_library(threads OBJECT ${OBOL_THREADS_FILES})
endif()

# define documentation
foreach(SRCFILE ${OBOL_THREADS_FILES})
	set(OBOL_DOCUMENTATION_FILES ${OBOL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/threads/${SRCFILE} CACHE INTERNAL "OBOL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
foreach(SRCFILE ${OBOL_THREADS_INTERNAL_FILES})
	set(OBOL_INTERNAL_DOCUMENTATION_FILES ${OBOL_INTERNAL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/threads/${SRCFILE} CACHE INTERNAL "OBOL_INTERNAL_DOCUMENTATION_FILES")
endforeach(SRCFILE)