CC = gcc
CXX = g++
CFLAGS = -O3 -Wall -pedantic -Wextra
LDFLAGS = -lpthread -lcurl -lrt -lssl -lcrypto -lprotobuf
COMPILE_FLAGS = -DCROW_ENABLE_SSL
#SANITIZER = -fsanitize=address -g
#SANITIZER = -fsanitize=undefined -g
#SANITIZER = -fsanitize=leak -g
LDFLAGS += $(SANITIZER)

SRC_DIR = ./src
BUILD_DIR = ./build
SOURCES = $(SRC_DIR)/cd.cpp $(SRC_DIR)/snapshot.pb.cc
HEADERS = $(wildcard $(SRC_DIR)/*.h)
EXECUTABLE = $(BUILD_DIR)/cd
OBJS = $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SOURCES))



all: prebuild $(EXECUTABLE)

# Awkward, but works, need improvements...
protoc_middleman: ./snapshot.proto
	protoc $$PROTO_PATH --cpp_out=$(SRC_DIR) ./snapshot.proto
	@touch protoc_middleman

prebuild:
	@echo ===== Variables =====
	@echo SOURCES: $(SOURCES)
	@echo HEADERS: $(HEADERS)
	@echo OBJS: $(OBJS)
	@echo CFLAGS: $(CFLAGS)
	@echo LDFLAGS: $(LDFLAGS)
	@echo SANITIZER: $(SANITIZER)
	@echo ===== Variables =====
	@echo 
	@mkdir -p $(BUILD_DIR)

$(EXECUTABLE): protoc_middleman $(OBJS)  
	$(CXX) $(OBJS) -o $@ $(LDFLAGS) $(COMPILE_FLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp $(HEADERS)
	$(CXX) $(CFLAGS) $(COMPILE_FLAGS) -c $< -o $@ $(SANITIZER)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all prebuild protoc_middleman clean 