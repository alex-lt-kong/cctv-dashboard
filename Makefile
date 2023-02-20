SRC_DIR = ./src
OPT = -Wall -O3 -Wextra -pedantic
LIBS = -lpthread 
OCV_LIB = -L/usr/local/lib -lopencv_core -lopencv_highgui -lopencv_imgproc -lopencv_video
CXX = g++
#SANITIZER = -fsanitize=address -fno-omit-frame-pointer -g

main: odcs.out cam.out

cam.out: $(SRC_DIR)/cam.cpp
	$(CXX) -o cam.out $(SRC_DIR)/cam.cpp -I/usr/local/include/ $(OCV_LIB) $(LIBS) -lrt $(OPT) $(SANITIZER)

odcs.out: $(SRC_DIR)/odcs.cpp
	$(CXX) -o odcs.out $(SRC_DIR)/odcs.cpp $(OPT) -DCROW_ENABLE_SSL $(LIBS) -lssl -lcrypto $(SANITIZER)

.PHONY : clean
clean:
	rm *.out
