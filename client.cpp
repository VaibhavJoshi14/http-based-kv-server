#include "include/httplib.h"
#include <fstream>
#include <sched.h>

#define SERVER_ADDRESS "http://127.0.0.1:5000"
#define CPU_core_id 2 // used to pin the process to core. used for load testing.


int main() {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);          // Clear the CPU set
    CPU_SET(CPU_core_id, &cpuset);  // Add core_id to the set

    pid_t pid = getpid();  // Current process ID

    // Set CPU affinity for this process
    if (sched_setaffinity(pid, sizeof(cpu_set_t), &cpuset) != 0) {
        perror("sched_setaffinity");
        return 1;
    }

    std::cout << "Pinned client process " << pid << " to CPU core " << CPU_core_id << std::endl;

    httplib::Client cli(SERVER_ADDRESS); // IP:Port of server.
    std::string request;
    std::string token;
    httplib::Params params;
    httplib::Response res;

    while (1)
    {
        // read the input from user. User gives the path to an image as an input 
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
        
            // Read the image file as a string
            std::ifstream file(value, std::ios::binary);
            std::string data((std::istreambuf_iterator<char>(file)),
                     std::istreambuf_iterator<char>());

            // Multipart form upload
            httplib::UploadFormDataItems items = {
                {"file", data, key, "image/jpg"}
            };

            auto res = cli.Post("/create", items);
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
                if (res->body == "Key does not exist."){
                    std::cout << res->body << "\n";
                    continue;
                }
                if (res->body == "An error occurred in the database."){
                    std::cout << res->body << "\n";
                    continue;
                }
                std::ofstream ofs(token, std::ios::binary);
                ofs << res->body;
                ofs.close();
                std::cout << "Image stored as " << token << " in present working directory\n";
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
        else if (token == "rotate")
        {
            std::string angle, path;
            std::getline(iss, angle, ' ');
            std::getline(iss, path, '\0');
        
            // Read the image file as a string
            std::ifstream file(path, std::ios::binary);
            std::string img((std::istreambuf_iterator<char>(file)),
                     std::istreambuf_iterator<char>());

            // Multipart form upload
            httplib::UploadFormDataItems items = {
                {"file", img, angle, "image/jpg"}
            };

            auto res = cli.Post("/rotate", items);
            if (res)
            {
                std::ofstream ofs("rotated.jpg", std::ios::binary);
                ofs << res->body;
                ofs.close();
                std::cout << "Rotated data saved in rotated.jpg\n";
            } 
            else
            {
                std::cout << "Request failed\n";
            }
        }
        else if (token == "rotate2")
        {
            std::string angle, key;
            std::getline(iss, angle, ' ');
            std::getline(iss, key, '\0');

            params.emplace("key", key);
            params.emplace("angle", angle);
            auto res = cli.Post("/rotate2", params);
            if (res)
            {
                std::cout << res->body << "\n";
            }
            else
            {
                std::cout << "Request failed\n";
            }
            params.erase("key");
            params.erase("angle");
        }
        
    }

}