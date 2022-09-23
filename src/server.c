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

#include <onion/types.h>
#include <onion/response.h>
#include <onion/log.h>
#include <onion/codecs.h>
#include <onion/onion.h>
#include <onion/version.h>


#include "utils.h"

volatile int done = 0;
json_object* root_users;
json_object* videos_devices;
onion *o=NULL;

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

char* authenticate(onion_request *req, onion_response *res, json_object* users) {
  const char *auth_header = onion_request_get_header(req, "Authorization");
  char *auth_header_val = NULL;
  char *supplied_username = NULL;
  char *supplied_passwd = NULL;
  if (auth_header && strncmp(auth_header, "Basic", 5) == 0) {
    auth_header_val = onion_base64_decode(&auth_header[6], NULL);
    supplied_username = auth_header_val;
    int i = 0;
    while (auth_header_val[i] != '\0' && auth_header_val[i] != ':' && i < MSG_BUF_SIZE + 1) { i++; }    
    if (auth_header_val[i] == ':' && i < MSG_BUF_SIZE) {
        auth_header_val[i] = '\0'; // supplied_username is set to auth_header_val, we terminate auth to make supplied_username work
        supplied_passwd = &auth_header_val[i + 1];
    }
    if (supplied_username != NULL && supplied_passwd != NULL) {
      json_object* users_passwd = json_object_object_get(users, supplied_username);
      
      const char* password = json_object_get_string(users_passwd);
      if (password != NULL && strncmp(supplied_passwd, password, strlen(password)) == 0) {
        char* authenticated_username = malloc(strlen(supplied_username) + 1);
        strcpy(authenticated_username, supplied_username);
        supplied_username = NULL;
        supplied_passwd = NULL;
        free(auth_header_val);
        return authenticated_username;
      }
    }
    free(auth_header_val);
  } 

  const char RESPONSE_UNAUTHORIZED[] = "<h1>Unauthorized access</h1>";
  // Not authorized. Ask for it.
  char temp[256];
  sprintf(temp, "Basic realm=PAC");
  onion_response_set_header(res, "WWW-Authenticate", temp);
  onion_response_set_code(res, HTTP_UNAUTHORIZED);
  onion_response_set_length(res, sizeof(RESPONSE_UNAUTHORIZED));
  onion_response_write(res, RESPONSE_UNAUTHORIZED, sizeof(RESPONSE_UNAUTHORIZED));
  return NULL;
}

int index_page(void *p, onion_request *req, onion_response *res) {

  char* authenticated_user = authenticate(req, res, root_users);
  if (authenticated_user == NULL) {
    ONION_WARNING("Failed login attempt");
    return OCS_PROCESSED;
  }
  free(authenticated_user);
  char msg[MSG_BUF_SIZE];
  const char* device_id_str = onion_request_get_query(req, "device_id");
  if (device_id_str == NULL) {
    return onion_shortcut_response("device_id not specified", HTTP_BAD_REQUEST, req, res);
  }
  if (strnlen(device_id_str, 5) > 4) {
    snprintf(msg, MSG_BUF_SIZE, "%s is too long for parameter device_id", device_id_str);
    return onion_shortcut_response(msg, HTTP_BAD_REQUEST, req, res);
  }
  int device_id = atoi(device_id_str);
  size_t videos_devices_len = json_object_array_length(videos_devices);
  if (device_id >= videos_devices_len) {
    snprintf(msg, MSG_BUF_SIZE, "%s is an invalid value for device_id", device_id_str);
    return onion_shortcut_response(msg, HTTP_BAD_REQUEST, req, res);
  }
  printf("videos_devices_len: %lu\n", videos_devices_len);
  char device_uri[PATH_MAX], sem_name[32], shm_name[32];
  sprintf(device_uri, "%s", json_object_get_string(json_object_array_get_idx(videos_devices, device_id)));
  
  sprintf(sem_name, "/odcs.sem%lu", device_id);
  sprintf(shm_name, "/odcs.shm%lu", device_id);
  printf("device_uri: %s, sem_name: %s, shm_name: %s\n", device_uri, sem_name, shm_name);
  int fd = shm_open(shm_name, O_RDWR, PERMS);  /* empty to begin */
  if (fd < 0) report_and_exit("shm_open()");

  /* get a pointer to memory */
  void* shmptr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if ((void*) -1 == shmptr) report_and_exit("mmap()");
  sem_t* semptr = sem_open(sem_name, O_RDWR);
  if (semptr == (void*) -1) report_and_exit("sem_open()");  
  if (sem_wait(semptr) < 0) report_and_exit("sem_wait()");
  size_t sz = 0;
  
  memcpy(&sz, shmptr, 4);
  printf("sz: %lu\n", sz);
  uint8_t* jpeg_copy = (uint8_t*)malloc(sz);
  memcpy(jpeg_copy, shmptr + 4, sz);
  onion_response_set_header(res, "Content-Type", "image/jpg");  
  onion_response_write(res, (char*)jpeg_copy, sz);
  free(jpeg_copy);
  sem_post(semptr);
  munmap(shmptr, SHM_SIZE);
  close(fd);
  sem_close(semptr);
  //unlink(sem_name);
  // shm_unlink(shm_name); can't shm_unlink() -> the shared memory should be managed by cap.out
  return OCS_PROCESSED;
}


int main(int argc, char **argv) {

  initialize_paths(argv[0]);
  printf("argv[0]: %s, settings_path: %s\n", argv[0], settings_path);
  json_object* root = json_object_from_file(settings_path);
  json_object* root_app = json_object_object_get(root, "app");
  json_object* root_app_port = json_object_object_get(root_app, "port");
  json_object* root_app_interface = json_object_object_get(root_app, "interface");
  json_object* root_app_ssl = json_object_object_get(root_app, "ssl");
  json_object* root_app_ssl_crt_path = json_object_object_get(root_app_ssl, "crt_path");
  json_object* root_app_ssl_key_path = json_object_object_get(root_app_ssl, "key_path");
  videos_devices = json_object_object_get(root_app, "video_devices"); 
  const char* ssl_crt_path = json_object_get_string(root_app_ssl_crt_path);
  const char* ssl_key_path = json_object_get_string(root_app_ssl_key_path);

  root_users = json_object_object_get(root_app, "users");
  ONION_VERSION_IS_COMPATIBLE_OR_ABORT();

  o=onion_new(O_THREADED);
  onion_set_timeout(o, 300 * 1000);
  // We set this to a large number, hoping the client closes the connection itself
  // If the server times out before client does, GnuTLS complains "The TLS connection was non-properly terminated."
  onion_set_certificate(o, O_SSL_CERTIFICATE_KEY, ssl_crt_path, ssl_key_path);
  onion_set_hostname(o, json_object_get_string(root_app_interface));
  onion_set_port(o, json_object_get_string(root_app_port));
  onion_url *urls=onion_root_url(o);
  onion_url_add(urls, "", index_page);
  ONION_INFO(
    "Valve controller listening on %s:%s",
    json_object_get_string(root_app_interface), json_object_get_string(root_app_port)
  );

  onion_listen(o);
  ONION_INFO("Onion server quits gracefully");

  free_paths();
  return 0;
}