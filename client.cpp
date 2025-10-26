/*
key is an integer, and value is a string.

create <key> <value>
create 1 hello world

read <key>
read 1

delete <key>
delete 1
*/

#include "httplib.h"

#define SERVER_ADDRESS "http://127.0.0.1:5000"


int main() {
    httplib::Client cli(SERVER_ADDRESS); // IP:Port of server.
    std::string request;
    std::string token;
    httplib::Params params;
    httplib::Response res;

    while (1)
    {
        // read the input from user
        std::cout << "$ ";
        std::getline(std::cin, request);
        
        // get the command type
        std::istringstream iss(request);
        std::getline(iss, token, ' '); // breaks the string by ' ' and returns the answer in token

        params.erase("key");
        params.erase("value");

        if (token == "create")
        {
            std::string key, value;
            std::getline(iss, key, ' ');
            std::getline(iss, value, '\0');
            
            params.emplace("key", key);
            params.emplace("value", value);

            auto res = cli.Post("/create", params);
            if (res)
            {
                std::cout << res->body << "\n";
            } 
            else
            {
                std::cout << "Request failed\n";
            }
        }
        else if (token == "read")
        {
            std::getline(iss, token, '\0'); //get the key

            auto res = cli.Get("/read?key=" + token); // send the key via a GET request
            if (res)
            {
                std::cout << res->body << "\n";
            } 
            else
            {
                std::cout << "Request failed\n";
            }
        }
        else if (token == "delete")
        {
            std::getline(iss, token, '\0');
            params.emplace("key", token);

            auto res = cli.Post("/delete", params);
            if (res)
            {
                std::cout << res->body << "\n";
            } 
            else
            {
                std::cout << "Request failed\n";
            }
        }
        
    }

}