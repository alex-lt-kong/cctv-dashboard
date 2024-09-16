include(${PROJECT_SOURCE_DIR}/cmake/proto.cmake)

add_executable(cctv
  main.cpp
  ${PROTO_SNAP_SRCS} ${PROTO_SNAP_HDRS})


target_link_libraries(cctv
  spdlog zmq curl pthread rt ssl crypto
  ${Protobuf_LIBRARIES})
