#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <string.h>
#include <time.h>

#include "common.h"

volatile int done = 0;

void report_and_exit(const char* msg) {
  perror(msg);
  exit(-1);
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


int main() {

  struct timespec ts;
  int fd = shm_open(SHM_NAME, O_RDWR, PERMS);  /* empty to begin */
  if (fd < 0) report_and_exit("shm_open()");

  /* get a pointer to memory */
  void* shmptr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if ((void*) -1 == shmptr) report_and_exit("mmap()");
  FILE *fp;
  sem_t* semptr = sem_open(SEM_NAME, O_RDWR);
  if (semptr == (void*) -1) report_and_exit("sem_open()");
  initialize_sig_handler();
  while (!done) {
    if (sem_wait(semptr) < 0) report_and_exit("sem_wait()");
    size_t sz = 0;
    memcpy(&sz, shmptr, 4);
    fp = fopen("/tmp/relay.jpg", "wb");
    if (fp == NULL)  {
      perror("fopen()");
      break;
    }
    if (fwrite(shmptr + 4, 1, sz, fp) != sz) report_and_exit("fwrite()");
    printf("%lu bytes fwrite()'ed\n", sz);
    sem_post(semptr);
    fclose(fp);
    sleep(1);
  }

  printf("thread_capture_live_image() quits gracefully\n");


  munmap(shmptr, SHM_SIZE);
  close(fd);
  sem_close(semptr);
  unlink(SEM_NAME);
  return 0;
}