#include "httplib.h"
#include <iostream>

int main() {
    httplib::Server svr;

    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("Hello, cpp-httplib!", "text/plain");
    });

    std::cout << "Server started at http://localhost:5000\n";
    svr.listen("0.0.0.0", 5000); // IP:Port of server 
}