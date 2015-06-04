CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(all)

#### library paths
SET(LIBRARY_PATH "${CMAKE_SOURCE_DIR}/lib")

#### output paths
SET(LIBRARY_OUTPUT_PATH "${CMAKE_SOURCE_DIR}/lib")
SET(EXECUTABLE_OUTPUT_PATH "${CMAKE_SOURCE_DIR}/bin")

#### settings
SET(CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} -g -Wno-deprecated -I/usr/local/include")
# SET(CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} -pg")
SET(CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} -g -O3")
SET(CMAKE_C_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} -L${LIBRARY_PATH} -L/usr/local/server/mysql-5.6/lib -L/usr/local/lib -lpthread -lmysqlclient -ltcmalloc -lzookeeper_mt")

#### projects
ADD_SUBDIRECTORY(cell)

