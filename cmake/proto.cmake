find_package(Protobuf REQUIRED)
if ( Protobuf_FOUND )
  message( STATUS "Protobuf version: ${Protobuf_VERSION}" )
  message( STATUS "Protobuf include path: ${Protobuf_INCLUDE_DIRS}" )
  message( STATUS "Protobuf libraries: ${Protobuf_LIBRARIES}" )
  message( STATUS "Protobuf lite libraries: ${Protobuf_LITE_LIBRARIES}")
  message( STATUS "Protobuf protoc: ${Protobuf_PROTOC_EXECUTABLE}")
else()
  message( WARNING "Protobuf package not found")
endif()
include_directories(${Protobuf_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(PROTO_FILE_SNAPSHOT ${PROJECT_SOURCE_DIR}/src/proto/snapshot.proto)

protobuf_generate_cpp(PROTO_SNAP_SRCS PROTO_SNAP_HDRS ${PROTO_FILE_SNAPSHOT})