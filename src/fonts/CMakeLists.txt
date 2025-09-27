# source files
set(COIN_FONTS_FILES
	freetype.cpp
	profont_data.cpp
	fontspec.cpp
	common.cpp
)

# Files excluded from public API documentation, included in complete documentation.
set(COIN_FONTS_INTERNAL_FILES
	freetype.h
	freetype.cpp
	profont_data.h
	profont_data.cpp
	fontspec.h
	fontspec.cpp
	common.h
	common.cpp
)

# build library
if(COIN_BUILD_SINGLE_LIB)
	foreach(SRCFILE ${COIN_FONTS_FILES})
		set(COIN_SOURCE ${COIN_SOURCE} fonts/${SRCFILE} CACHE INTERNAL "COIN_SOURCE")
	endforeach(SRCFILE)
else()
	add_library(fonts OBJECT ${COIN_FONTS_FILES})
endif()

# define documentation
foreach(SRCFILE ${COIN_FONTS_FILES})
	set(COIN_DOCUMENTATION_FILES ${COIN_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/fonts/${SRCFILE} CACHE INTERNAL "COIN_DOCUMENTATION_FILES")
endforeach(SRCFILE)
foreach(SRCFILE ${COIN_FONTS_INTERNAL_FILES})
	set(COIN_INTERNAL_DOCUMENTATION_FILES ${COIN_INTERNAL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/fonts/${SRCFILE} CACHE INTERNAL "COIN_INTERNAL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
