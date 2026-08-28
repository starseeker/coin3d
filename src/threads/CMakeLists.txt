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


# build module object library
obol_add_source_module(threads ${OBOL_THREADS_FILES})
