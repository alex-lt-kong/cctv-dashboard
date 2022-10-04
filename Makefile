SRC_DIR = ./src
OPT = -Wall -O3
OCV_LIB = -L/usr/local/lib -lopencv_videoio -lopencv_imgproc -lopencv_core -lopencv_imgcodecs


main: $(SRC_DIR)/cam.cpp $(SRC_DIR)/odcs.c utils.o
	gcc -o odcs.out $(SRC_DIR)/odcs.c utils.o -lrt -lpthread -ljson-c -lonion $(OPT)
	g++ -o cam.out $(SRC_DIR)/cam.cpp utils.o -I/usr/local/include/ $(OCV_LIB) -lrt -lpthread -ljson-c $(OPT)


utils.o: $(SRC_DIR)/utils.c $(SRC_DIR)/utils.h
	gcc $(SRC_DIR)/utils.c -c $(OPT)

clean:
	rm *.out *.o
