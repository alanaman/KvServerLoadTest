# Define the name of the executable for the client
set(CLIENT_EXEC client_app)

# Create the executable target from its source file
add_executable(${CLIENT_EXEC}
    main.cpp
    utils.cpp
    )


target_include_directories(${CLIENT_EXEC} PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)