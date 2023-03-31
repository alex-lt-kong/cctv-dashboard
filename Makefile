SRC_DIR = ./src
OPT = -Wall -O3 -Wextra -pedantic
LIBS = -lpthread
OCV_LIB = -L/usr/local/lib -lopencv_core -lopencv_highgui -lopencv_imgproc -lopencv_video
CXX = g++
#SANITIZER = -fsanitize=address -fno-omit-frame-pointer -g

main: cd.out cam.out

cd.out: $(SRC_DIR)/cd.cpp
	$(CXX) -o cd.out $(SRC_DIR)/cd.cpp $(OPT) -DCROW_ENABLE_SSL $(LIBS) -lssl -lcrypto $(SANITIZER)

.PHONY : clean
clean:
	rm *.out
