SRC_DIR = ./src
OPT = -Wall -O3 -Wextra -pedantic
LIBS = -lpthread -lcurl -lrt -lssl -lcrypto
CXX = g++
#SANITIZER = -fsanitize=address -fno-omit-frame-pointer -g

main: cd.out

cd.out: $(SRC_DIR)/cd.cpp
	$(CXX) -o cd.out $(SRC_DIR)/cd.cpp $(OPT) -DCROW_ENABLE_SSL $(LIBS)  $(SANITIZER)

.PHONY : clean
clean:
	rm *.out
