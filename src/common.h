#define SEM_INITIAL_VALUE 1
#define SEM_NAME "rtsp-cam.sem"

#define SHM_SIZE 8388608 // 8MB
#define SHM_NAME "/rtsp-cam.shm"

#define PERMS (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)
// o:wr, g:wr, i.e., 0660