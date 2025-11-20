# sqlpp11 requires C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include FetchContent module to download dependencies
include(FetchContent)

# --- 1. Find system-installed PostgreSQL (libpq) ---
# This finds libpq at the top level and, crucially,
# populates the CMake cache with PostgreSQL_LIBRARIES,
# PostgreSQL_INCLUDE_DIRS, and the PostgreSQL::PQ target.
find_package(PostgreSQL REQUIRED)

# --- 2. Set sqlpp11 options as CACHE variables ---
# Instead of using CMAKE_ARGS, set these options in the cache.
# The sqlpp11 sub-project will read these cache variables
# when it is configured by FetchContent_MakeAvailable.
set(BUILD_POSTGRESQL_CONNECTOR ON CACHE BOOL "Enable sqlpp11 PostgreSQL connector")
set(BUILD_TESTING OFF CACHE BOOL "Disable sqlpp11 testing")
set(ENABLE_DATE_TIME_NETWORKING OFF CACHE BOOL "Disable sqlpp11 networking")

# --- 3. Fetch sqlpp11 ---
FetchContent_Declare(
    sqlpp11
    GIT_REPOSITORY https://github.com/rbock/sqlpp11.git
    GIT_TAG main    # Use a stable tag
    # CMAKE_ARGS block is removed
)

# This will download, configure, and build sqlpp11.
# - It will see BUILD_POSTGRESQL_CONNECTOR=ON in the cache.
# - It will run its own find_package(PostgreSQL), which will
#   succeed because it will find the results from step 1 in the cache.
# - It will then build the connector and create the sqlpp11::postgresql target.
FetchContent_MakeAvailable(sqlpp11)

# --- 4. Define our application ---
add_executable(my_app 
    main.cpp
    kv_database.cpp
    kv_server.cpp
    kv_server_cw.cpp
    )

# --- 5. Link libraries and includes ---

# Add our local "include" directory
target_include_directories(my_app PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${SOURCE_DIR}/civetweb/include
)

set(CIVETWEB_ENABLE_SSL OFF CACHE BOOL "" FORCE)
set(CIVETWEB_ENABLE_CXX ON  CACHE BOOL "" FORCE)
set(CIVETWEB_ENABLE_WEBSOCKETS ON CACHE BOOL "" FORCE)
add_subdirectory(civetweb)


# Link our app against the libraries
target_link_libraries(my_app PRIVATE
    civetweb-cpp
    sqlpp11::postgresql
    pthread
)