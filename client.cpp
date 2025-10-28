/*
key is an integer, and value is a string.

create <key> <value>
create 1 hello world

read <key>
read 1

delete <key>
delete 1
*/

/*

key is a string, value is the path to an image (only 256x256 jpg images allowed).
key can be simply the name of the image.

create should store the image to the database with key k. (IO bound)
    create <key> <path to image>
    create 000.jpg img/african_elephant/000.jpg

read should return the image saved on database
    read <key>
    read 000.jpg

delete should delete the image from the database
    delete <key>
    delete 000.jpg

rotate the image sent to the server by some degrees, and then return the image. do not save the image on database.
use create to separately save the image. (leads to CPU bound process)
    rotate <angle degrees> <path to image>
    rotate 45 /img/african_elephant/000.jpg

rotate2 takes the image <key> from database, and saves it in the database (does not return the image to the user)
    rotate2 <angle degrees> <key>
    rotate2 45 000.jpg

*/

#include "httplib.h"
#include <fstream>

#define SERVER_ADDRESS "http://127.0.0.1:5000"


int main() {
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
            } 
            else
            {
                std::cout << "Request failed\n";
            }
        }
        
    }

}