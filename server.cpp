#include "httplib.h"
#include <iostream>
#include <unordered_map>
#include <mutex>
#include <queue>
#include <string>
#include <fstream>

#define IP "127.0.0.1"
#define port 5000
#define CACHE_SIZE 5
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

int main()
{
    httplib::Server svr;
    std::unordered_map<int, std::string> CACHE; // Hash table as a cache to store kv pairs.
    std::list<int> queue_of_keys; // stores the order in which keys arrive. std::list is implemented as a doubly-linked list.
    std::mutex m; // lock used when storing data into CACHE.

    svr.Get("/welcome", [&](const httplib::Request&, httplib::Response& res) {
        res.set_content("Hello, You have connected to an http-based Key-Value server.", "text/plain");
    });
    
    // For the "create" command.
    svr.Post("/create", [&](const httplib::Request& req, httplib::Response& res) {
        auto it = req.form.files.find("file");
        
        const auto& file = it->second;
        std::ofstream ofs(file.filename, std::ios::binary);
        ofs << file.content;
        ofs.close();
        res.set_content("File uploaded successfully", "text/plain");

        res.set_content("Done", "text/plain");
        
        /*m.lock();
        
        if (0) // If key is already present in database.
        {
            m.unlock();
            res.set_content("Key already present", "text/plain");
            return;
        }
        
        if (CACHE.size() == CACHE_SIZE) // evict an element when cache is full.
        {
            int front = queue_of_keys.front();
            CACHE.erase(front); // FCFS policy.
            queue_of_keys.pop_front(); 
        }

        queue_of_keys.push_back(key);
        CACHE[key] = value;
        
        m.unlock();
        res.set_content("Done", "text/plain");
        */
    });

    // For the "read" command
    svr.Get("/read", [&](const httplib::Request& req, httplib::Response& res) {
        int key = std::stoi(req.get_param_value("key"));
        std::string value;
        std::cout << "Received " << key << "\n";
        fflush(stdout);
        
        m.lock();
        if (CACHE.count(key)) // If key is already in cache, then fetch from it directly.
        {
            value = CACHE[key];
        }
        else // Else fetch from database.
        {
            
            // If database does not have that value, then return "Key does not exist".
        }
        m.unlock();
        
        res.set_content(value, "text/plain");
    });

    // For the "delete" command
    svr.Post("/delete", [&](const httplib::Request& req, httplib::Response& res) {
        int key = std::stoi(req.get_param_value("key"));
        std::cout << "Received " << key << "\n";
        fflush(stdout);
        
        m.lock();
        if (CACHE.count(key))
        {
            // erase from cache.
            CACHE.erase(key);
            
            // erase the corresponding entry in queue_of_keys.
            queue_of_keys.remove(key);
        }
        // delete from the database.
        m.unlock();
        
        res.set_content("Done", "text/plain");
    });

    std::cout << "Server started at "<< IP << ":"<< port <<"\n";
    fflush(stdout);

    // Start listening to the server
    svr.listen(IP, port); // IP:Port of server 
}
