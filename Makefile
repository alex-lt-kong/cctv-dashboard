SRC_DIR = ./src
OPT = -Wall -O3

main: $(SRC_DIR)/cam.cpp
	gcc -o relay.out $(SRC_DIR)/relay.c -lrt -lpthread $(OPT)
	g++ -o cam.out $(SRC_DIR)/cam.cpp -I/usr/include/opencv4/ -lopencv_videoio -lopencv_imgproc -lopencv_core -lopencv_imgcodecs -lrt -lpthread $(OPT)


clean:
	rm *.out $(SRC_DIR)/*.out