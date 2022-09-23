#ifndef UTILS_H
#define UTILS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <onion/log.h>
#include <onion/codecs.h>
#include <onion/onion.h>
#include <pthread.h>
#include <string.h>
#include <fcntl.h>  // open()
#include <unistd.h> // close()
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <pthread.h>
#include <limits.h>         // PATH_MAX
#include <json-c/json.h>    // JSON
#include <libgen.h>         // dirname()

#define SEM_INITIAL_VALUE 1

#define SHM_SIZE 8388608 // 8MB

#define PERMS (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)
// o:wr, g:wr, i.e., 0660

#define MSG_BUF_SIZE 4096

extern char* settings_path;

struct CamPayload {
  char device_uri[PATH_MAX];
  char sem_name[32];
  char shm_name[32];
  int tid;
};

void initialize_paths(char* argv);

void free_paths();

#ifdef __cplusplus
}
#endif

#endif /* UTILS_H */
