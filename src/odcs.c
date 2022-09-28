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
#include <onion/codecs.h>
#include <onion/onion.h>
#include <onion/version.h>
#include <onion/shortcuts.h>

#include "utils.h"

volatile int done = 0;
json_object* root_users;
json_object* video_devices;
size_t video_devices_len = -1;
onion *o=NULL;

char* authenticate(onion_request *req, onion_response *res, json_object* users) {
  const char *auth_header = onion_request_get_header(req, "Authorization");
  char *auth_header_val = NULL;
  char *supplied_username = NULL;
  char *supplied_passwd = NULL;
  if (auth_header && strncmp(auth_header, "Basic", 5) == 0) {
    auth_header_val = onion_base64_decode(&auth_header[6], NULL);
    supplied_username = auth_header_val;
    int i = 0;
    const int max_username = 32;
    while (auth_header_val[i] != '\0' && auth_header_val[i] != ':' && i < max_username + 1) { i++; }    
    if (auth_header_val[i] == ':' && i < max_username) {
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
  sprintf(temp, "Basic realm=On-demand CCTV server");
  onion_response_set_header(res, "WWW-Authenticate", temp);
  onion_response_set_code(res, HTTP_UNAUTHORIZED);
  onion_response_set_length(res, sizeof(RESPONSE_UNAUTHORIZED));
  onion_response_write(res, RESPONSE_UNAUTHORIZED, sizeof(RESPONSE_UNAUTHORIZED));
  return NULL;
}

int get_logged_in_user_json(void *p, onion_request *req, onion_response *res) {
  char* authenticated_user = authenticate(req, res, root_users);
  if (authenticated_user == NULL) {
    ONION_WARNING("Failed login attempt");
    return OCS_PROCESSED;
  }
  char msg[MSG_BUF_SIZE];
  snprintf(msg, MSG_BUF_SIZE, "{\"status\":\"success\",\"data\":\"%s\"}", authenticated_user);
  free(authenticated_user);
  return onion_shortcut_response(msg, HTTP_OK, req, res);
}

int get_device_count_json(void *p, onion_request *req, onion_response *res) {
  char* authenticated_user = authenticate(req, res, root_users);
  if (authenticated_user == NULL) {
    ONION_WARNING("Failed login attempt");
    return OCS_PROCESSED;
  }
  syslog(LOG_INFO, "User [%s] authenticated", authenticated_user);
  char msg[MSG_BUF_SIZE];
  snprintf(msg, MSG_BUF_SIZE, "{\"status\":\"success\",\"data\":%lu}", video_devices_len);
  free(authenticated_user);
  return onion_shortcut_response(msg, HTTP_OK, req, res);
}

int cctv(void *p, onion_request *req, onion_response *res) {

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
  int device_id = atoi(device_id_str);
  
  if (device_id >= video_devices_len || device_id < 0) {
    snprintf(msg, MSG_BUF_SIZE, "[%s] is an invalid value for device_id", device_id_str);
    return onion_shortcut_response(msg, HTTP_BAD_REQUEST, req, res);
  }
  
  char sem_name[32], shm_name[32]; 
  sprintf(sem_name, "/odcs.sem%d", device_id);
  sprintf(shm_name, "/odcs.shm%d", device_id);
  
  int fd = shm_open(shm_name, O_RDWR, PERMS);
  if (fd < 0) {
    syslog(LOG_ERR, "shm_open(): %s", strerror(errno));
    return onion_shortcut_response("Failed to read image from device", HTTP_INTERNAL_ERROR, req, res);
  }
  void* shmptr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if ((void*) -1 == shmptr) {
    syslog(LOG_ERR, "mmap(): %s", strerror(errno));
    return onion_shortcut_response("Failed to read image from device", HTTP_INTERNAL_ERROR, req, res);
  }
  sem_t* semptr = sem_open(sem_name, O_RDWR);
  if (semptr == (void*) -1) {
    syslog(LOG_ERR, "sem_open(): %s", strerror(errno));
    return onion_shortcut_response("Failed to read image from device", HTTP_INTERNAL_ERROR, req, res);
  }
  if (sem_wait(semptr) < 0) {
    syslog(LOG_ERR, "sem_wait(): %s", strerror(errno));
    return onion_shortcut_response("Failed to lock image for reading", HTTP_INTERNAL_ERROR, req, res);
  }

  size_t sz = 0;
  memcpy(&sz, shmptr, 4);
  uint8_t* jpeg_data = (uint8_t*)malloc(sz);
  memcpy(jpeg_data, (void*)(shmptr + 4), sz);
  onion_response_set_header(res, "Content-Type", "image/jpg");  
  onion_response_write(res, (char*)jpeg_data, sz);
  free(jpeg_data);
  sem_post(semptr);
  munmap(shmptr, SHM_SIZE);
  close(fd);
  sem_close(semptr);
  // sem_unlink(sem_name);
  // shm_unlink(shm_name); can't shm_unlink() -> the shared memory should be managed by cap.out
  return OCS_PROCESSED;
}

int index_page(void *p, onion_request *req, onion_response *res) {

  char* authenticated_user = authenticate(req, res, root_users);
  if (authenticated_user == NULL) {
    ONION_WARNING("Failed login attempt");
    return OCS_PROCESSED;
  }
  free(authenticated_user);
  char file_path[PATH_MAX] = "";
  const char* file_name = onion_request_get_query(req, "file_name");
  strcpy(file_path, public_dir);
  if (file_name == NULL) {
    strcat(file_path, "html/index.html");
  }
  else if (strcmp(file_name, "odcs.js") == 0) {
    strcat(file_path, "js/odcs.js");
  } else if (strcmp(file_name, "favicon.png") == 0) {
    strcat(file_path, "img/favicon.png");
  } else {
    strcat(file_path, "html/index.html");
  }
  return onion_shortcut_response_file(file_path, req, res);
}

int main(int argc, char **argv) {
  openlog("odcs", LOG_PID | LOG_CONS, 0);
  initialize_paths(argv[0]);

  json_object* root = json_object_from_file(settings_path);
  json_object* root_app = json_object_object_get(root, "app");
  json_object* root_app_port = json_object_object_get(root_app, "port");
  json_object* root_app_interface = json_object_object_get(root_app, "interface");
  json_object* root_app_ssl = json_object_object_get(root_app, "ssl");
  json_object* root_app_ssl_crt_path = json_object_object_get(root_app_ssl, "crt_path");
  json_object* root_app_ssl_key_path = json_object_object_get(root_app_ssl, "key_path");
  video_devices = json_object_object_get(root_app, "video_devices"); 
  video_devices_len = json_object_array_length(video_devices);
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
  
  onion_url_add(urls, "get_device_count_json/", get_device_count_json);
  onion_url_add(urls, "get_logged_in_user_json/", get_logged_in_user_json);
  onion_url_add(urls, "cctv/", cctv);
  onion_url_add(urls, "", index_page);
  syslog(
    LOG_INFO, "On-demand CCTV server listening on %s:%s",
    json_object_get_string(root_app_interface), json_object_get_string(root_app_port)
  );

  onion_listen(o);
  syslog(LOG_INFO, "Onion server quits gracefully");

  free_paths();
  return 0;
}