/*
#ifndef CROW_STATIC_DIRECTORY
#define CROW_STATIC_DIRECTORY "public/"
#endif
*/

#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>

#include <nlohmann/json.hpp>
#include <crow.h>

#include "utils.h"

using namespace std;
using namespace crow;
using njson = nlohmann::json;

string homeDir(getenv("HOME"));
string settings_path_ = homeDir + "/bin/on-demand-cctv-server/settings.json";
njson json_settings;

void ask_for_cred(response& res) {
    res.set_header("WWW-Authenticate", "Basic realm=On-demand CCTV server");
    res.code = 401;
    res.write("<h1>Unauthorized access</h1>");
    res.end();
}


string http_authenticate(const request& req) {
    string myauth = req.get_header_value("Authorization");
    if (myauth.size() < 6) {
        return "";
    } 
    string mycreds = myauth.substr(6);
    string d_mycreds = crow::utility::base64decode(mycreds, mycreds.size());
    size_t found = d_mycreds.find(':');
    string username = d_mycreds.substr(0, found);
    string password = d_mycreds.substr(found+1);
    if (!json_settings["app"]["users"].contains(username)) {
        return "";
    }
    if (json_settings["app"]["users"][username] != password) {
        return "";
    }
    return username;
}

int main()
{
    crow::SimpleApp app;
    std::ifstream is(settings_path_);
    is >> json_settings;

    CROW_ROUTE(app, "/")([](response& res){
        res.redirect("/static/html/index.html");
        res.end();
    });

    CROW_ROUTE(app, "/get_logged_in_user_json/")([](
        const request& req, response& res){
        string username = http_authenticate(req);
        if (username.size() == 0) {
            ask_for_cred(res);            
            return;
        }
        res.end(json::wvalue({
            {"status", "success"}, {"data", username}
        }).dump());
    });

    CROW_ROUTE(app, "/get_device_count_json/")([](
        const request& req, response& res){
        string username = http_authenticate(req);
        if (username.size() == 0) {
            ask_for_cred(res);            
            return;
        }
        res.end(json::wvalue({
            {"status", "success"},
            {"data", json_settings["app"]["video_devices"].size()}
        }).dump());
    });

    CROW_ROUTE(app, "/cctv/")([](const request& req, response& res){
        string username = http_authenticate(req);
        if (username.size() == 0) {
            ask_for_cred(res);            
            return;
        }
        if (req.url_params.get("device_id") == nullptr) {
            res.code = 400;
            res.end(json::wvalue({
                {"status", "error"}, {"data", "device_id not specified"}
            }).dump());
            return;
        }
        uint32_t device_id = atoi(req.url_params.get("device_id"));
        if (device_id >= json_settings["app"]["video_devices"].size()) {
            res.code = 400;
            res.end(json::wvalue({
                {"status", "error"},
                {"data", "device_id " + to_string(device_id) + " is invalid"}
            }).dump());
            return;
        }
        char sem_name[32], shm_name[32]; 
        sprintf(sem_name, "/odcs.sem%d", device_id);
        sprintf(shm_name, "/odcs.shm%d", device_id);

        int fd = shm_open(shm_name, O_RDWR, PERMS);
        if (fd < 0) {
            cerr << "shm_open(): " << strerror(errno) << endl;
            res.code = 500;
            res.end(json::wvalue({
                {"status", "error"},
                {"data", "Failed to read image from device"}
            }).dump());
            return;
        }
        void* shmptr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
            fd, 0);
        if ((void*) -1 == shmptr) {
            cerr << "mmap(): " << strerror(errno) << endl;            
            res.body = json::wvalue({
                {"status", "error"},
                {"data", "Failed to read image from device"}
            }).dump();
            res.code = 500;
            res.end();
            return;
        }
        sem_t* semptr = sem_open(sem_name, O_RDWR);
        if (semptr == (void*) -1) {
            cerr << "sem_open(): " << strerror(errno) << endl;
            res.body = json::wvalue({
                {"status", "error"},
                {"data", "Failed to read image from device"}
            }).dump();
            res.code = 500;
            res.end();
            return;
        }
        if (sem_wait(semptr) < 0) {
            cerr << "sem_wait(): " << strerror(errno) << endl;
            res.code = 500;
            res.end(json::wvalue({
                {"status", "error"},
                {"data", "Failed to lock image for reading"}
            }).dump());
            return;
        }

        size_t jpeg_size = 0;
        memcpy(&jpeg_size, shmptr, 4);
        res.set_header("Content-Type", "image/jpg");
        res.end(string((char*)(shmptr + 4), jpeg_size));
        sem_post(semptr);
        munmap(shmptr, SHM_SIZE);
        close(fd);
        sem_close(semptr);
    });

    app.port(json_settings["app"]["port"].get<int>()).multithreaded()
    .ssl_file(json_settings["app"]["ssl"]["crt_path"],
        json_settings["app"]["ssl"]["key_path"])
    .run();
}
