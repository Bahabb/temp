## src/CMakeLists.txt
#add_executable(server server.c common.h)
#add_executable(client client.c common.h)

## Set compiler flags (can also be done in the top-level CMakeLists.txt)
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_POSIX_C_SOURCE=2")



add_executable(server
    server.c
    client_mgmt.c
)

# Optional: Uncomment if you have client.c and want to build it
add_executable(client
     client.c
 )

target_include_directories(server PRIVATE
    ${LIBWEBSOCKETS_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
)

target_link_libraries(server PRIVATE
    ${LIBWEBSOCKETS_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    pthread
)

target_compile_options(server PRIVATE -Wall -Wextra)
