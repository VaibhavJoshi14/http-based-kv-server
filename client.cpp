#include "httplib.h"

int main() {
    httplib::Client cli("http://localhost:5000"); // IP:Port of server.
    auto res = cli.Get("/");
    if (res) {
        std::cout << res->status << "\n";
        std::cout << res->body << "\n";
    } 
    else {
        std::cout << "Request failed\n";
    }
}