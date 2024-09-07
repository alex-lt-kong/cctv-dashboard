#include "snapshot.pb.h"
#include "utils.h"

#include <crow.h>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <fcntl.h>
#include <semaphore.h>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>

using namespace std;
using namespace crow;
using njson = nlohmann::json;

njson json_settings;
string ipc_mode;

void ask_for_cred(response &res) {
  res.set_header("WWW-Authenticate", "Basic realm=CCTV Dashboard");
  res.code = 401;
  res.write("<h1>Unauthorized access</h1>");
  res.end();
}

string http_authenticate(const request &req) {
  string myauth = req.get_header_value("Authorization");
  if (myauth.size() < 6) {
    return "";
  }
  string mycreds = myauth.substr(6);
  string d_mycreds = crow::utility::base64decode(mycreds, mycreds.size());
  size_t found = d_mycreds.find(':');
  string username = d_mycreds.substr(0, found);
  string password = d_mycreds.substr(found + 1);
  if (!json_settings["app"]["users"].contains(username)) {
    return "";
  }
  if (json_settings["app"]["users"][username] != password) {
    return "";
  }
  return username;
}

struct httpAuthMiddleware : ILocalMiddleware {
  struct context {};

  void before_handle(crow::request &req, crow::response &res,
                     __attribute__((unused)) context &ctx) {
    string username = http_authenticate(req);
    if (username.size() == 0) {
      ask_for_cred(res);
      return;
    }
  }

  void after_handle(__attribute__((unused)) crow::request &req,
                    __attribute__((unused)) crow::response &res,
                    __attribute__((unused)) context &ctx) {}
};

void load_settings(string settings_path) {
  cout << "settings_path: " << settings_path << endl;

  ifstream is(settings_path);
  json_settings = njson::parse(is,
                               /* callback */ nullptr,
                               /* allow exceptions */ true,
                               /* ignore_comments */ true);
  ipc_mode = json_settings["app"]["ipc_mode"];
}

size_t curl_http_callback(void *ptr, size_t size, size_t nmemb,
                          std::string *data) {
  data->append((char *)ptr, size * nmemb);
  return size * nmemb;
}

void load_image_from_http(uint32_t device_id, response &res) {
  auto curl = curl_easy_init();
  CURLcode curl_res;
  if (curl) {
    curl_easy_setopt(curl, CURLOPT_URL,
                     json_settings["app"]["video_sources"][device_id]["url"]
                         .get<string>()
                         .c_str());
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 50L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);

    std::string response_string;
    std::string header_string;
    // curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_http_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &header_string);
    curl_res = curl_easy_perform(curl);
    if (curl_res != CURLE_OK) {
      res.code = 500;
      res.end(json::wvalue({{"status", "error"},
                            {"data", "curl_easy_perform() error: " +
                                         string(curl_easy_strerror(curl_res))}})
                  .dump());
      return;
    }
    curl_easy_cleanup(curl);
    res.write(response_string);
    res.end();
    curl = NULL;
  } else {
    res.code = 400;
    res.end(
        json::wvalue({{"status", "error"}, {"data", "curl_easy_init() failed"}})
            .dump());
  }
}

void load_image_from_shm(uint32_t device_id, response &res) {
  string sem_name =
      json_settings["app"]["video_sources"][device_id]["semaphore_name"];
  string shm_name =
      json_settings["app"]["video_sources"][device_id]["shared_mem_name"];

  int fd = shm_open(shm_name.c_str(), O_RDONLY, PERMS);
  if (fd < 0) {
    cerr << "shm_open(): " << strerror(errno) << endl;
    res.code = 500;
    res.end(json::wvalue({{"status", "error"},
                          {"data", "Failed to read image from device"}})
                .dump());
    return;
  }
  void *shmptr = mmap(NULL, SHM_SIZE, PROT_READ, MAP_SHARED, fd, 0);
  if (shmptr == MAP_FAILED) {
    cerr << "mmap(): " << strerror(errno) << endl;
    res.body = json::wvalue({{"status", "error"},
                             {"data", "Failed to read image from device"}})
                   .dump();
    res.code = 500;
    res.end();
    return;
  }
  sem_t *semptr = sem_open(sem_name.c_str(), O_RDWR);
  if (semptr == SEM_FAILED) {
    cerr << "sem_open(): " << strerror(errno) << endl;
    res.body = json::wvalue({{"status", "error"},
                             {"data", "Failed to read image from device"}})
                   .dump();
    res.code = 500;
    res.end();
    return;
  }
  if (sem_wait(semptr) < 0) {
    cerr << "sem_wait(): " << strerror(errno) << endl;
    res.code = 500;
    res.end(json::wvalue({{"status", "error"},
                          {"data", "Failed to lock image for reading"}})
                .dump());
    return;
  }
  size_t payload_size;
  memcpy(&payload_size, shmptr, sizeof(size_t));
  SnapshotMsg msg;
  msg.ParseFromString(
      string((char *)((uint8_t *)shmptr + sizeof(size_t)), payload_size));
  cout << msg.rateofchange() << endl;
  res.set_header("Content-Type", "image/jpg");
  res.end(msg.jpegbytes());
  if (sem_post(semptr) != 0) {
    perror("sem_post()");
  }
  if (munmap(shmptr, SHM_SIZE) != 0) {
    perror("munmap()");
  }
  /* On the reader side, we need to close() ONLY, no shm_unlink() */
  if (close(fd) != 0) {
    perror("close()");
  }
  if (sem_close(semptr) != 0) {
    perror("sem_close()");
  }
}

int main(int argc, char *argv[]) {
  string settings_path =
      string(getenv("HOME")) + "/.config/ak-studio/cctv-dashboard.jsonc";
  if (argc > 2) {
    cerr << "Usage: ./cd.out [config-file.jsonc]" << endl;
    return EXIT_FAILURE;
  } else if (argc == 2) {
    settings_path = string(argv[1]);
  }
  load_settings(settings_path);

  if (ipc_mode == "http")
    curl_global_init(CURL_GLOBAL_DEFAULT);

  App<httpAuthMiddleware> app;
  CROW_ROUTE(app, "/").CROW_MIDDLEWARES(app,
                                        httpAuthMiddleware)([](response &res) {
    res.set_static_file_info("./static/html/index.html");
    res.end();
  });

  CROW_ROUTE(app, "/get_logged_in_user_json/")
      .CROW_MIDDLEWARES(app, httpAuthMiddleware)([](const request &req) {
        return json::wvalue(
            {{"status", "success"}, {"data", http_authenticate(req)}});
      });

  CROW_ROUTE(app, "/get_device_count_json/")
      .CROW_MIDDLEWARES(app, httpAuthMiddleware)([]() {
        return json::wvalue(
            {{"status", "success"},
             {"data", json_settings["app"]["video_sources"].size()}});
      });

  CROW_ROUTE(app, "/cctv/")
      .CROW_MIDDLEWARES(
          app, httpAuthMiddleware)([](const request &req, response &res) {
        if (req.url_params.get("device_id") == nullptr) {
          res.code = 400;
          res.end(json::wvalue({{"status", "error"},
                                {"data", "device_id not specified"}})
                      .dump());
          return;
        }
        uint32_t device_id = atoi(req.url_params.get("device_id"));
        if (device_id >= json_settings["app"]["video_sources"].size()) {
          res.code = 400;
          res.end(json::wvalue({{"status", "error"},
                                {"data", "device_id " + to_string(device_id) +
                                             " is invalid"}})
                      .dump());
          return;
        }
        if (ipc_mode == "shared_memory") {
          load_image_from_shm(device_id, res);
        } else if (ipc_mode == "http") {
          load_image_from_http(device_id, res);
        } else {
          res.code = 400;
          res.end(json::wvalue(
                      {{"status", "error"}, {"data", "Unsupported ipc_mode"}})
                      .dump());
          return;
        }
      });

  if (json_settings["app"]["ssl"]["enabled"]) {
    app.bindaddr(json_settings["app"]["interface"])
        .port(json_settings["app"]["port"].get<int>())
        .multithreaded()
        .ssl_file(json_settings["app"]["ssl"]["crt_path"],
                  json_settings["app"]["ssl"]["key_path"])
        .run();
  } else {
    app.bindaddr(json_settings["app"]["interface"])
        .port(json_settings["app"]["port"].get<int>())
        .multithreaded()
        .run();
  }
  if (ipc_mode == "http")
    curl_global_cleanup();
}
