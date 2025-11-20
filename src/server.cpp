#include "include/httplib.h"
#include <iostream>
#include <unordered_map>
#include <mutex>
#include <queue>
#include <string>
#include <fstream>
#include <opencv2/opencv.hpp>
#include <vector>
#include <sched.h>
#include <csignal>
#include <fstream>
#include <unistd.h>
#include <sys/sysinfo.h>

using namespace cv;

#define IP "127.0.0.1"
#define port 5000
#define CACHE_SIZE 5
#define DATABASE_ADDRESS "http://127.0.0.1:5001"
#define CPU_core_id 0 // used to pin the process to core. used for load testing.

struct CpuTimes {
    long long user, nice, system, idle, iowait, irq, softirq;
};

unsigned long readIOTime() {
    std::ifstream f("/sys/block/sda/stat");
    unsigned long fields[11];
    for (int i = 0; i < 11; i++)
        f >> fields[i];
    return fields[9];   // column 10: time spent doing I/O (ms)
}

CpuTimes readCPU() {
    std::ifstream file("/proc/stat");
    std::string cpu;
    CpuTimes t;
    file >> cpu >> t.user >> t.nice >> t.system >> t.idle >> t.iowait >> t.irq >> t.softirq;
    return t;
}

unsigned long t1 = readIOTime();
CpuTimes c1 = readCPU();

void printStats() {
    CpuTimes c2 = readCPU();

    long long idle1 = c1.idle + c1.iowait;
    long long idle2 = c2.idle + c2.iowait;

    long long nonIdle1 = c1.user + c1.nice + c1.system + c1.irq + c1.softirq;
    long long nonIdle2 = c2.user + c2.nice + c2.system + c2.irq + c2.softirq;

    long long total1 = idle1 + nonIdle1;
    long long total2 = idle2 + nonIdle2;

    double totald = total2 - total1;
    double idled = idle2 - idle1;

    double cpu_percentage = (1.0 - idled / totald) * 100.0;

    std::cout << "Percentage CPU used: " << cpu_percentage << "%\n";

    unsigned long t2 = readIOTime();

    double busy = t2 - t1;      // ms disk was busy
    double interval = 5*60*1000;      // measurement window in ms. 5min is written here.

    std::cout << "Percentage Disk used: " << (busy / interval) * 100.0 << "%\n";
    
    struct sysinfo info;
    sysinfo(&info);

    unsigned long total = info.totalram;
    unsigned long free  = info.freeram;

    // Adjust by memory unit size
    total *= info.mem_unit;
    free  *= info.mem_unit;

    unsigned long used = total - free;

    double p = (used / (double)total) * 100.0;

    std::cout << "Percentage RAM used: " << p << "%\n\n";

}


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
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);          // Clear the CPU set
    CPU_SET(CPU_core_id, &cpuset);  // Add core_id to the set

    pid_t pid = getpid();  // Current process ID

    // Set CPU affinity for this process
    if (sched_setaffinity(pid, sizeof(cpu_set_t), &cpuset) != 0) {
        perror("sched_setaffinity");
        return 1;
    }

    std::cout << "Pinned server process " << pid << " to CPU core " << CPU_core_id << std::endl;

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

    // For the rotate2 command: which takes a key and an angle as an input and rotates the image by that angle in counter-clockwise direction, and saves it in the database
    svr.Post("/rotate2", [&](const httplib::Request& req, httplib::Response& res){
        std::string key = req.get_param_value("key");
        int angle = std::stoi(req.get_param_value("angle"));
        std::string img_data;
        
        // Get the image.
        m.lock();
        if (CACHE.count(key)) // If key is already in cache, then fetch from it directly.
        {
            //std::cout << "CACHE used\n"; // used for debugging
            img_data = CACHE[key];
            m.unlock();
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
            img_data = res2->body;
        }
        
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

        // send to the database for saving.
        // INSERT on the same key UPDATEs the key in cassandra.
        
        // Multipart form upload
        httplib::UploadFormDataItems items = {
            {"file", rotated_data, key, "image/jpeg"}
        };

        // send to database for persistent storage
        auto res3 = db_cli.Post("/create", items);
        if (!res3 || res3->status != 200)
        {
            std::cout << "Error: Could not update key in database.";
            res.set_content("An error occurred in the database.", "text/plain");
            return;
        }
        // If key was in cache, we also need to update the cache.
        if (CACHE.count(key))
        {
            CACHE[key] = rotated_data;
        }
        res.set_content("Image " + key + " rotated and saved in database.", "image/jpeg");
    });

    svr.Get("/printStatistics", [&](const httplib::Request& req, httplib::Response& res){
        printStats();
        t1 = readIOTime();
        c1 = readCPU();
    });

    std::cout << "Server started at "<< IP << ":"<< port <<"\n";
    fflush(stdout);

    // Start listening to the server
    svr.listen(IP, port); // IP:Port of server 
}
