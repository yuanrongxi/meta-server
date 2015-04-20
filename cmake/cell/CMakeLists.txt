CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(cell)

# definitions
#

# includes
SET(includes
    "../../cell"
    "/usr/local/server/mysql-5.6/include"
    )
INCLUDE_DIRECTORIES(${includes})

# sources
FILE(GLOB sources_c "../../cell/*.c")

# targets
ADD_EXECUTABLE(celld ${sources_c})
TARGET_LINK_LIBRARIES(celld pthread mysqlclient)
