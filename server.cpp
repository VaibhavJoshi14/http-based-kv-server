// compile using 
// g++ server.cpp -o server `pkg-config --cflags --libs opencv4`
#include "include/httplib.h"
#include <iostream>
#include <unordered_map>
#include <mutex>
#include <queue>
#include <string>
#include <fstream>
#include <opencv2/opencv.hpp>
#include <vector>

using namespace cv;

#define IP "127.0.0.1"
#define port 5000
#define CACHE_SIZE 5
#define DATABASE_ADDRESS "http://127.0.0.1:5001"
// I should be able to generate two different workloads, one that is CPU bound, and other that is I/O bound.
// 1. Uploading an image to a server (IO bottleneck in the database server).
// 2. Rotate an image in the server (that user sends) and send it as a response (CPU bottleneck in the server). Optionally send the response to be saved/updated in the database.

// check browser on http://127.0.0.1/5000

// cpp-httplib uses a thread-pool by default, so no need to work on it, written here https://github.com/yhirose/cpp-httplib/issues/415.

// What does eviction mean in case of hash-table? When the hash-map has CACHE_SIZE number of elements, we will delete an element
// from it. Choosing the FCFS replacement policy because it is easy to implement. We need to store separately the order in
// which the keys arrive (stored in queue_of_keys queue).

// Since it is a key-value type data, relational databases may not be perfectly suitable here. So using NoSQL database
// Apache Cassandra (free and open source).
// Installing instructions:  https://cassandra.apache.org/doc/latest/cassandra/installing/installing.html.
// If cassandra hangs the system, then the OOM is killing cassandra because it is demanding too much heap. Reduce its max heap size (use gpt).
// Cassandra takes 20 to 60 seconds to load after boot, so wait.

// Server will connect to the database using http-based requests via tcp connection (not using rpc).

// This function stores a (key, value) pair in the cache, evicting a pair if cache is already full.
void store_in_cache(std::unordered_map<std::string, std::string>& CACHE,
                    std::list<std::string>& queue_of_keys, 
                    std::string& key, 
                    std::string& value)
{
    if (CACHE.size() == CACHE_SIZE) // evict an element when cache is full.
    {
        std::string front = queue_of_keys.front();
        CACHE.erase(front); // FCFS policy.
        queue_of_keys.pop_front(); 
    }

    queue_of_keys.push_back(key);
    CACHE[key] = value;
}

int main()
{
    httplib::Server svr;
    std::unordered_map<std::string, std::string> CACHE; // Hash table as a cache to store kv pairs.
    std::list<std::string> queue_of_keys; // stores the order in which keys arrive. std::list is implemented as a doubly-linked list.
    std::mutex m; // lock used when storing data into CACHE.
    httplib::Client db_cli(DATABASE_ADDRESS);
    
    svr.Get("/welcome", [&](const httplib::Request&, httplib::Response& res) {
        res.set_content("Hello, You have connected to an http-based Key-Value server.", "text/plain");
    });
    
    // For the "create" command.
    svr.Post("/create", [&](const httplib::Request& req, httplib::Response& res) {
        auto it = req.form.files.find("file");
        const auto& file = it->second;

        std::string key = file.filename;
        std::string value = file.content; // value is the image

        // only kept for debugging.
        //std::ofstream ofs(key, std::ios::binary);
        //ofs << value;
        //ofs.close();
        
        // Read if the key is already present in database
        auto res2 = db_cli.Get("/read?key=" + key);
        if (!res2 || res2->status != 200){
            std::cout << "Error while accessing database.\n";
            res.set_content("An error occurred in the database.", "text/plain");
            return;
        }
        if (res2->body != "Key does not exist.") // If key is already present in database.
        {
            res.set_content("Key already present", "text/plain");
            return;
        }
        
        // Store the key-value pair in cache since it is a recently used item.
        m.lock();
        store_in_cache(CACHE, queue_of_keys, key, value);
        m.unlock();

        // Multipart form upload
        httplib::UploadFormDataItems items = {
            {"file", value, key, "image/jpeg"}
        };

        // send to database for persistent storage
        auto res3 = db_cli.Post("/create", items);
        if (!res3 || res3->status != 200)
        {
            std::cout << "Error: Could not create key in database.";
            res.set_content("An error occurred in the database.", "text/plain");
            return;
        }

        res.set_content("File uploaded successfully", "text/plain");
    });

    // For the "read" command
    svr.Get("/read", [&](const httplib::Request& req, httplib::Response& res) {
        std::string key = req.get_param_value("key");
        std::string value;
        
        m.lock();
        if (CACHE.count(key)) // If key is already in cache, then fetch from it directly.
        {
            //std::cout << "CACHE used\n"; // used for debugging
            value = CACHE[key];
            m.unlock();
            res.set_content(value, "image/jpeg");
        }
        else // Else fetch from database.
        {
            m.unlock();
            auto res2 = db_cli.Get("/read?key=" + key);
            if (!res2 || res2->status != 200) 
            {
                std::cout << "Error in database while reading\n";
                res.set_content("An error occurred in the database.", "text/plain");
                return;
            }
            // Store the key-value pair in cache since it is not in cache
            store_in_cache(CACHE, queue_of_keys, key, res2->body);
            res.set_content(res2->body, "image/jpeg");
        }
    });

    // For the "delete" command
    svr.Post("/delete", [&](const httplib::Request& req, httplib::Response& res) {
        std::string key = req.get_param_value("key");
        fflush(stdout);
        
        // delete from cache.
        m.lock();
        CACHE.erase(key);
        queue_of_keys.remove(key);
        m.unlock();

        // delete from the database.
        httplib::Params params;
        params.emplace("key", key);
        auto res2 = db_cli.Post("/delete", params);
        if (!res2 || res2->status != 200) 
        {
            std::cout << "Error in database while deleting the file\n";
            res.set_content("An error occurred in the database.", "text/plain");
        }
        else res.set_content("Image deleted successfully.", "text/plain");
    });

    // For the rotate command: which takes an image and an angle as an input and rotates the image by that angle in counter-clockwise direction.
    svr.Post("/rotate", [&](const httplib::Request& req, httplib::Response& res){
        auto it = req.form.files.find("file");
        const auto& file = it->second;

        int angle = std::stoi(file.filename);
        std::string img_data = file.content;

        // Convert binary string to vector<uchar> for OpenCV decoding
        std::vector<uchar> buffer(img_data.begin(), img_data.end());

        // Decode image from memory
        Mat img = imdecode(buffer, IMREAD_COLOR);
        if (img.empty()) {
            std::cerr << "Error: could not decode image data." << std::endl;
            res.set_content("Error: could not decode image data.", "text/plain");
            return;
        }

        // Get the rotation matrix
        Point2f center(img.cols / 2.0F, img.rows / 2.0F);

        Mat rotation_matrix = getRotationMatrix2D(center, angle, 1);

        // Compute bounding box so that the rotated image fits completely
        Rect2f bbox = RotatedRect(Point2f(), img.size(), angle).boundingRect2f();

        // Adjust transformation matrix to keep image centered
        rotation_matrix.at<double>(0, 2) += bbox.width / 2.0 - img.cols / 2.0;
        rotation_matrix.at<double>(1, 2) += bbox.height / 2.0 - img.rows / 2.0;

        // Apply the rotation
        Mat rotated;
        warpAffine(img, rotated, rotation_matrix, bbox.size());

        // Encode rotated image back to binary string (e.g. JPEG)
        std::vector<uchar> out_buf;
        imencode(".jpg", rotated, out_buf);

        // Convert back to std::string
        std::string rotated_data(out_buf.begin(), out_buf.end());

        res.set_content(rotated_data, "image/jpeg");
    });

    std::cout << "Server started at "<< IP << ":"<< port <<"\n";
    fflush(stdout);

    // Start listening to the server
    svr.listen(IP, port); // IP:Port of server 
}
