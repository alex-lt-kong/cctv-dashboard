SRC_DIR = ./src
OPT = -Wall -O2

main: $(SRC_DIR)/cam.cpp $(SRC_DIR)/odcs.c utils.o
	gcc -o odcs.out $(SRC_DIR)/odcs.c utils.o -lrt -lpthread -ljson-c -lonion $(OPT)
	g++ -o cam.out $(SRC_DIR)/cam.cpp utils.o -I/usr/include/opencv4/ -lopencv_videoio -lopencv_imgproc -lopencv_core -lopencv_imgcodecs -lrt -lpthread -ljson-c $(OPT)


utils.o: $(SRC_DIR)/utils.c $(SRC_DIR)/utils.h
	gcc $(SRC_DIR)/utils.c -c $(OPT)

clean:
	rm *.out *.o