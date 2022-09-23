#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <unistd.h>
#include <libgen.h>
#include <iostream>
#include <semaphore.h>
#include <signal.h>
#include <getopt.h>

#include "common.h"


#define IMAGES_PATH_SIZE 5432

using namespace std;
using namespace cv;

volatile int done = 0;
void* shmptr;

void* thread_capture_live_image(const char* device_uri) {
  printf("thread_capture_live_image() started.\n");
  Mat frame;
  bool result = false;
  VideoCapture cap;

  std::vector<uint8_t> buf = {};
  std::vector<int> s = {};
  const uint16_t interval = 10;
  uint32_t iter = interval;
  sem_t* semptr = sem_open(SEM_NAME, O_CREAT, PERMS, SEM_INITIAL_VALUE);
  if (semptr == (void*) -1) done = 1;
  while (!done) {
    
    if (cap.isOpened() == false || result == false) {
      printf("cap.open(%s)'ing...\n", device_uri);
      result = cap.open(device_uri);
      cap.set(CAP_PROP_FOURCC, VideoWriter::fourcc('M', 'J', 'P', 'G')); 
      cap.set(CAP_PROP_FRAME_WIDTH, 640);
      cap.set(CAP_PROP_FRAME_HEIGHT, 360);
      if (cap.isOpened()) {
        printf("cap.open(%s)'ed\n", device_uri);
      }
    }
    if (!result) {
      fprintf(stderr, "cap.open(%s) failed\n", device_uri);
      sleep(1);
      continue;
    }    
    result = cap.read(frame);
    if (!result) {
      fprintf(stderr, "cap.read() failed\n");
      sleep(1);
      continue;
    }
    ++iter;
    if (iter < interval) { continue; }
    
    iter = 0;
    //imwrite("/tmp/test.jpg", frame, s);
    imencode(".jpg", frame, buf, s);
    size_t sz = buf.size();
    if (sem_wait(semptr) < 0) {
      perror("sem_wait()");
      break;
    }
    memcpy(shmptr, &sz, 4);
    memcpy(shmptr + 4, &(buf[0]), buf.size());
    if (sem_post(semptr) < 0) {
      perror("sem_post()");
      break;
    }
    printf("memcpy()'ed, buf.size(): %lu\n", sz);
  }
  cap.release();
  sem_close(semptr);
  unlink(SEM_NAME);
  printf("thread_capture_live_image() quits gracefully\n");
  return NULL;
}

static void signal_handler(int sig_num) {
  printf("Signal %d recieved\n", sig_num);
  done = 1;  
}

void initialize_sig_handler() {
  struct sigaction act;
  act.sa_handler = signal_handler;
  sigemptyset(&act.sa_mask);
  act.sa_flags = SA_RESETHAND;
  sigaction(SIGINT, &act, 0);
  sigaction(SIGABRT, &act, 0);
  sigaction(SIGTERM, &act, 0);
}

void report_and_exit(const char* msg) {
  perror(msg);
  signal_handler(-1);
}

void print_usage() {
    printf("Usage: cam.out --device-uri [URI]\n");
}


int main(int argc, char *argv[]) {
  static struct option long_options[] = {
    {"device-path",    required_argument, 0,  'd' },
  };
  char device_uri[PATH_MAX] = {0};
  int opt= 0;
  int long_index = 0;
  while ((opt = getopt_long(argc, argv,"d:", long_options, &long_index )) != -1) {
    switch (opt) {
      case 'd' : strncpy(device_uri, optarg, strnlen(optarg, PATH_MAX - 1));
        break;
      default: print_usage(); 
        exit(EXIT_FAILURE);
    }
  }
  if (strnlen(device_uri, PATH_MAX) == 0) {
      print_usage();
      exit(EXIT_FAILURE);
  }
  printf("[%s]\n", device_uri);
  initialize_sig_handler();
  int fd = shm_open(SHM_NAME, O_RDWR | O_CREAT, PERMS);
  if (fd < 0) report_and_exit("shm_open()");
  ftruncate(fd, SHM_SIZE);
  shmptr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if ((void*) -1  == shmptr) report_and_exit("mmap()");
  printf("shared memory created, address: %p [0..%d]\n", shmptr, SHM_SIZE - 1);
  thread_capture_live_image(device_uri);
  
  munmap(shmptr, SHM_SIZE);
  close(fd);

  return 0;
}