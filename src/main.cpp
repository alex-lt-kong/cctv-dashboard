#define FMT_HEADER_ONLY
#define CROW_STATIC_DIRECTORY PROJECT_SOURCE_DIR "/static/"
#define CROW_STATIC_ENDPOINT "/static/<path>"

#include "snapshot.pb.h"

#include <crow.h>
#include <crow/logging.h>
#include <curl/curl.h>
#include <cxxopts.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <fcntl.h>
#include <fcntl.h>  // open()
#include <libgen.h> // dirname()
#include <semaphore.h>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h> // close()

#define SHM_SIZE 2 * 1024 * 1024
#define PERMS (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)

using namespace std;
using namespace crow;
namespace fs = std::filesystem;
using njson = nlohmann::json;

njson json_settings;
string ipc_mode;

class CustomLogger : public crow::ILogHandler {
public:
  CustomLogger() {}
  void log(std::string msg, LogLevel lvl) {
    if (lvl == LogLevel::Critical) {
      spdlog::critical(msg);
    } else if (lvl == LogLevel::Error) {
      spdlog::error(msg);
    } else if (lvl == LogLevel::Warning) {
      spdlog::warn(msg);
    } else if (lvl == LogLevel::Debug) {
      spdlog::debug(msg);
    } else {
      spdlog::info(msg);
    }
  }
};

void load_settings(string settings_path) {

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
      goto curl_cleanup;
    }
    res.write(response_string);
    res.end();
  curl_cleanup:
    curl_easy_cleanup(curl);
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

string get_http_auth_username(const request &req) {
  string myauth = req.get_header_value("Authorization");
  if (myauth.size() < 6) {
    return "<NA>";
  }
  string mycreds = myauth.substr(6);
  string d_mycreds = crow::utility::base64decode(mycreds, mycreds.size());
  size_t found = d_mycreds.find(':');
  string username = d_mycreds.substr(0, found);
  string password = d_mycreds.substr(found + 1);
  return username;
}

int main(int argc, char *argv[]) {
  string config_path;
  cxxopts::Options options(argv[0], PROJECT_NAME);
  // clang-format off
  options.add_options()
    ("h,help", "print help message")
    ("c,config-path", "JSON configuration file path", cxxopts::value<string>()->default_value(config_path));
  // clang-format on
  auto result = options.parse(argc, argv);
  if (result.count("help") || !result.count("config-path")) {
    std::cout << options.help() << "\n";
    return 0;
  }
  config_path = result["config-path"].as<std::string>();

  load_settings(config_path);
  spdlog::set_pattern("%Y-%m-%d %T.%e | %7l | %5t | %v");
  spdlog::info(PROJECT_NAME " started (git commit: {})", GIT_COMMIT_HASH);
  if (ipc_mode == "http")
    curl_global_init(CURL_GLOBAL_DEFAULT);

  CustomLogger logger;
  crow::logger::setHandler(&logger);
  crow::SimpleApp app;
  CROW_ROUTE(app, "/")
  ([](response &res) {
    auto get_path_relative_to_cwd =
        [](const std::string &absolute_path) -> string {
      fs::path abs_path = fs::absolute(absolute_path);
      fs::path current_path = fs::current_path();
      return fs::relative(abs_path, current_path).string();
    };
    // set_static_file_info asks for a path relative to the current working
    // directory...
    res.set_static_file_info_unsafe(
        get_path_relative_to_cwd(PROJECT_SOURCE_DIR "/static/html/index.html"));
    res.end();
  });

  CROW_ROUTE(app, "/get_logged_in_user_json/")
  ([](const request &req) {
    return json::wvalue(
        {{"status", "success"}, {"data", get_http_auth_username(req)}});
  });

  CROW_ROUTE(app, "/get_device_count_json/")
  ([]() {
    return json::wvalue(
        {{"status", "success"},
         {"data", json_settings["app"]["video_sources"].size()}});
  });

  CROW_ROUTE(app, "/cctv/")
  ([](const request &req, response &res) {
    if (req.url_params.get("device_id") == nullptr) {
      res.code = 400;
      res.end(json::wvalue(
                  {{"status", "error"}, {"data", "device_id not specified"}})
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
      res.end(
          json::wvalue({{"status", "error"}, {"data", "Unsupported ipc_mode"}})
              .dump());
      return;
    }
  });

  app.bindaddr(json_settings.value("/app/interface"_json_pointer, "127.0.0.1"))
      .port(json_settings.value("/app/port"_json_pointer, 80))
      .multithreaded()
      .run();

  if (ipc_mode == "http")
    curl_global_cleanup();
  spdlog::info(PROJECT_NAME " exited");
}
