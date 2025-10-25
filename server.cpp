#include "httplib.h"
#include <iostream>

#define IP "127.0.0.1"
#define port 5000
//check browser on http://127.0.0.1/5000

int main() {
    httplib::Server svr;

    svr.Get("/welcome", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("Hello, This is an http-based Key-Value server.", "text/plain");
    });
    
    // For the "create" command
    svr.Post("/create", [](const httplib::Request& req, httplib::Response& res) {
        std::cout << "Received " << req.get_param_value("key") << " " << req.get_param_value("value") << "\n";
        fflush(stdout);
        res.set_content("Done", "text/plain");
    });

    // For the "read" command
    svr.Get("/read", [](const httplib::Request& req, httplib::Response& res) {
        std::cout << "Received " << req.get_param_value("key") << "\n";
        fflush(stdout);
        res.set_content(req.body, "text/plain");
    });

    // For the "delete" command
    svr.Post("/delete", [](const httplib::Request& req, httplib::Response& res) {
        std::cout << "Received " << req.get_param_value("key") << " " << req.get_param_value("value") << "\n";
        fflush(stdout);
        res.set_content(req.body, "text/plain");
    });

    std::cout << "Server started at "<< IP << ":"<< port <<"\n";
    fflush(stdout);

    // Start listening to the server
    svr.listen(IP, port); // IP:Port of server 
}