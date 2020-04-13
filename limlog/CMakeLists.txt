set(SRCS
	Timestamp.cpp
	NumToString.cpp
	Log.cpp
)

add_library(limlog ${SRCS})

set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)