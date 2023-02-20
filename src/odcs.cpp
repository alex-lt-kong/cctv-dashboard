//#define CROW_ENABLE_SSL
#include<string>

#include <crow.h>

using namespace std;

int main()
{
    crow::SimpleApp app;

    CROW_ROUTE(app, "/")([](){
        return "Hello world";
    });

    CROW_ROUTE(app, "/json")([](const crow::request& req, crow::response& res){
        string myauth = req.get_header_value("Authorization");
        if (myauth.size() < 6) {
            res.code = 401;
            res.body = "error";
            res.end();
            return;
        } 
        string mycreds = myauth.substr(6);
        string d_mycreds = crow::utility::base64decode(mycreds, mycreds.size());
        size_t found = d_mycreds.find(':');
        string username = d_mycreds.substr(0, found);
        string password = d_mycreds.substr(found+1);
        cout << "username: " << username << "\n"
             << "password: " << password << endl;
        crow::json::wvalue x({{"message", "Hello, World!"}});
        x["message2"] = "Hello, World.. Again!";
        res.code = 200;
        res.body = x.dump();
        res.end();
        //return x;
    });

    app.port(18080).multithreaded()
    //.ssl_file("certificate.crt", "certificate.key")
    .run();
}