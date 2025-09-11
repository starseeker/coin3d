# source files
set(COIN_THREADS_FILES
	common.cpp
	thread.cpp
	mutex.cpp
	storage.cpp
	condvar.cpp
	worker.cpp
	wpool.cpp
	recmutex.cpp
	sched.cpp
)

# Files excluded from public API documentation, included in complete documentation.
set(COIN_THREADS_INTERNAL_FILES
	condvarp.h
	mutexp.h
	recmutexp.h
	schedp.h
	storagep.h
	threadp.h
	threadsutilp.h
	workerp.h
	wpoolp.h
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
