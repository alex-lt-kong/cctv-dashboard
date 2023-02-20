SRC_DIR = ./src
OPT = -Wall -O3 -Wextra -pedantic 
OCV_LIB = -L/usr/local/lib -lopencv_calib3d -lopencv_contrib -lopencv_core -lopencv_features2d -lopencv_flann -lopencv_gpu -lopencv_highgui -lopencv_imgproc -lopencv_legacy -lopencv_ml -lopencv_nonfree -lopencv_objdetect -lopencv_ocl -lopencv_photo -lopencv_stitching -lopencv_superres -lopencv_ts -lopencv_video -lopencv_videostab -lrt -lpthread -lm -ldl

main: $(SRC_DIR)/cam.cpp $(SRC_DIR)/odcs.cpp utils.o
	g++ -o odcs.out $(SRC_DIR)/odcs.cpp $(OPT) -DCROW_ENABLE_SSL -lssl -lcrypto
	g++ -o cam.out $(SRC_DIR)/cam.cpp utils.o -I/usr/local/include/ $(OCV_LIB) -lrt -lpthread -ljson-c $(OPT)
	
utils.o: $(SRC_DIR)/utils.c $(SRC_DIR)/utils.h
	gcc $(SRC_DIR)/utils.c -c $(OPT)

clean:
	rm *.out *.o
