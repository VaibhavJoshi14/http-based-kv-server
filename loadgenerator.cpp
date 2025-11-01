// example usage:
// ./ldgen 2 5
// this starts 2 clients which do load test for 5 minutes.

#include "include/httplib.h"
#include <fstream>
#include <sched.h>
#include <filesystem>
#include <chrono>
#include <thread>
#include <string>
#include <random>

namespace fs = std::filesystem;
using namespace std::chrono;

#define SERVER_ADDRESS "http://127.0.0.1:5000"
#define CPU_core_id 2 // used to pin the process to core.
int numthreads;
int duration_seconds; // each thread will run for this duration.
// Read all the images at once, since reading images from disk would take considerable time during load test, slowing 
// down the request generation rate.
std::vector<std::string> images;
std::unordered_map<std::string, std::string> images_sent;
std::vector<std::string> keys; // the keys used to send the images
int numimages;
std::vector<double> avg_throughput;
std::vector<double> avg_response_time;
std::vector<int> num_requests;

void create_all(int id)
{
    // each thread sends images with keys (std::to_string(id) + std::to_string(i)). sends the same set of images, but with
    // different keys, so every request gets executed on database.
    httplib::Client cli(SERVER_ADDRESS); // IP:Port of server.
    int i = 0;
    std::string _id_ = std::to_string(id);
    std::chrono::duration<double> elapsed;
    auto start = std::chrono::high_resolution_clock::now();
    
    do
    {
        // Multipart form upload
        std::string key = _id_ + std::to_string(i);
        i += 1;
        httplib::UploadFormDataItems items = {
            {"file", images[i % numimages], key, "image/jpeg"}
        };

        auto curr = std::chrono::high_resolution_clock::now();
        
        auto res = cli.Post("/create", items);
        
        auto end = std::chrono::high_resolution_clock::now();
        
        if (res) // successful completion of request
        {
            avg_throughput[id] += 1;
        } 
        else
        {
            std::cout << "Create request failed\n";
        }

        elapsed = end - start;
        auto resp_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - curr);
        avg_response_time[id] += resp_time.count();
        num_requests[id]++;

    }while (elapsed.count() < duration_seconds);
}

void read_all(int id)
{
    httplib::Client cli(SERVER_ADDRESS); // IP:Port of server.
    int i = 0;
    std::string _id_ = std::to_string(id);
    std::chrono::duration<double> elapsed;
    auto start = std::chrono::high_resolution_clock::now();
    
    do
    {
        std::string key = _id_ + std::to_string(i);
        i = i + 1;
        if (images_sent.count(key) == 0)
        {
            i = 0;
            key = _id_ + std::to_string(i);
        }
        auto curr = std::chrono::high_resolution_clock::now();

        auto res = cli.Get("/read?key=" + key); // send the key via a GET request
        
        auto end = std::chrono::high_resolution_clock::now();
        if (res)
        {
            if (res->body == "Key does not exist."){
                std::cout << res->body <<" [" << key << "]\n";
                continue;
            }
            if (res->body == "An error occurred in the database."){
                std::cout << res->body << "\n";
                continue;
            }
            avg_throughput[id] += 1;
        }
        else
        {
            std::cout << "Read request failed\n";
        }
        elapsed = end - start;
        auto resp_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - curr);
        avg_response_time[id] += resp_time.count();
        num_requests[id]++;

    }while (elapsed.count() < duration_seconds);

}

void rotate_all(int id)
{
    httplib::Client cli(SERVER_ADDRESS); // IP:Port of server.
    int i = 0;
    std::string _id_ = std::to_string(id);
    std::chrono::duration<double> elapsed;

    // Create a random device and seed generator
    std::random_device rd;
    std::mt19937 gen(rd());

    // Create a distribution
    std::uniform_int_distribution<> dist(1, 359);

    auto start = std::chrono::high_resolution_clock::now();
    do
    {
        // create a random angle to rotate
        std::string angle = std::to_string(dist(gen));
        std::string key = _id_ + std::to_string(i);
        i = i + 1;
        if (images_sent.count(key) == 0)
        {
            i = 0;
            key = _id_ + std::to_string(i);
        }
        // Multipart form upload
        httplib::UploadFormDataItems items = {
            {"file", images_sent[key], angle, "image/jpeg"}
        };
        auto curr = std::chrono::high_resolution_clock::now();
        auto res = cli.Post("/rotate", items);
        auto end = std::chrono::high_resolution_clock::now();
        if (res)
        {
            avg_throughput[id] += 1;
        }
        else
        {
            std::cout << "Rotate request failed\n";
        }
        elapsed = end - start;
        auto resp_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - curr);
        avg_response_time[id] += resp_time.count();
        num_requests[id]++;

    }while (elapsed.count() < duration_seconds);
}

void delete_all(int id)
{
    httplib::Client cli(SERVER_ADDRESS); // IP:Port of server.
    int i = 0;
    std::string _id_ = std::to_string(id);
    std::chrono::duration<double> elapsed;
    httplib::Params params;

    auto start = std::chrono::high_resolution_clock::now();
    
    do
    {
        std::string key = _id_ + std::to_string(i);
        i = i + 1;
        if (images_sent.count(key) == 0)
        {
            i = 0;
            key = _id_ + std::to_string(i);
        }
        params.emplace("key", key);

        auto curr = std::chrono::high_resolution_clock::now();
        auto res = cli.Post("/delete", params);
        auto end = std::chrono::high_resolution_clock::now();

        if (res)
        {
            images_sent.erase(key);
            avg_throughput[id] += 1;
        } 
        else
        {
            std::cout << "Delete request failed\n";
        }
        params.erase("key");
        elapsed = end - start;
        auto resp_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - curr);
        avg_response_time[id] += resp_time.count();
        num_requests[id]++;

    }while (elapsed.count() < duration_seconds);
}


void create_read_delete_mix(int id)
{
    if (id % 3 == 0)
        create_all(id + numthreads);
    else if (id % 3 == 1)
        read_all(id);
    else
        delete_all(id);
}

int main(int argc, char* argv[]) 
{   
    if (argc < 3)
    {
       fprintf(stderr,"usage \n%s <num_client_threads> <duration_minutes>\n", argv[0]);
       exit(0);
    }
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);          // Clear the CPU set
    CPU_SET(CPU_core_id, &cpuset);  // Add core_id to the set

    pid_t pid = getpid();  // Current process ID

    // Set CPU affinity for this process
    if (sched_setaffinity(pid, sizeof(cpu_set_t), &cpuset) != 0) {
        perror("sched_setaffinity");
        return 1;
    }

    std::cout << "Pinned loadgenerator process " << pid << " to CPU core " << CPU_core_id << std::endl;

    std::cout << "Loading data in memory.\n";
    std::string path = "img/african_elephant";  // directory path
    try {
        for (const auto& entry : fs::directory_iterator(path)) 
        {
            // Read the image file as a string
            std::ifstream file(entry.path().string(), std::ios::binary);
            std::string data((std::istreambuf_iterator<char>(file)),
                     std::istreambuf_iterator<char>());
            images.push_back(data);
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    numimages = images.size();
    std::cout << "Load generator started...\n";

    numthreads = std::stoi(argv[1]);
    duration_seconds = std::stoi(argv[2]) * 60; 

    avg_throughput.resize(numthreads);
    avg_response_time.resize(numthreads);
    num_requests.resize(numthreads);

    // create_all(): each client will run this. (IO bound)
    std::cout << "---------------------------------------------------------------\n";
    std::cout << "Starting create_all load test\n";
    std::fill(avg_throughput.begin(), avg_throughput.end(), 0);
    std::fill(avg_response_time.begin(), avg_response_time.end(), 0);
    std::fill(num_requests.begin(), num_requests.end(), 0);

    std::vector<std::thread> threads;

    // launch multiple threads
    for (int i = 0; i < numthreads; ++i)
    {
        threads.emplace_back(create_all, i);  // create and start a new thread
    }

    // wait for all threads to finish
    for (auto& t : threads)
    {
        t.join();
    }

    double avg_throug = 0, avg_resp = 0;
    for (int i = 0; i < numthreads; ++i)
    {
        avg_response_time[i] /= num_requests[i];
        avg_throughput[i] /= duration_seconds;
        avg_throug += avg_throughput[i];
        avg_resp += avg_response_time[i];
    }
    avg_throug /= numthreads;
    avg_resp /= numthreads;
    threads.clear();

    std::cout << "Completed create_all load test\n";
    std::cout << "Average throughput (requests succesfully completed/sec): " << avg_throug << std::endl << "Average response time: " << avg_resp << "(ms) \n";
    // get_all(): each client will run this.
    std::cout << "---------------------------------------------------------------\n";
    std::cout << "Starting read_all load test\n";
    std::fill(avg_throughput.begin(), avg_throughput.end(), 0);
    std::fill(avg_response_time.begin(), avg_response_time.end(), 0);
    std::fill(num_requests.begin(), num_requests.end(), 0);

    // launch multiple threads
    for (int i = 0; i < numthreads; ++i) {
        threads.emplace_back(read_all, i);  // create and start a new thread
    }

    // wait for all threads to finish
    for (auto& t : threads) {
        t.join();
    }
    
    avg_throug = 0, avg_resp = 0;
    for (int i = 0; i < numthreads; ++i)
    {
        avg_response_time[i] /= num_requests[i];
        avg_throughput[i] /= duration_seconds;
        avg_throug += avg_throughput[i];
        avg_resp += avg_response_time[i];
    }
    avg_throug /= numthreads;
    avg_resp /= numthreads;
    threads.clear();

    std::cout << "Completed read_all load test\n";
    std::cout << "Average throughput (requests succesfully completed/sec): " << avg_throug << std::endl << "Average response time: " << avg_resp << "(ms) \n";
    
    // rotate(): each client will run this. (CPU bound)
    std::cout << "---------------------------------------------------------------\n";
    std::cout << "Starting rotate_all load test\n";
    std::fill(avg_throughput.begin(), avg_throughput.end(), 0);
    std::fill(avg_response_time.begin(), avg_response_time.end(), 0);
    std::fill(num_requests.begin(), num_requests.end(), 0);

    // launch multiple threads
    for (int i = 0; i < numthreads; ++i) {
        threads.emplace_back(rotate_all, i);  // create and start a new thread
    }

    // wait for all threads to finish
    for (auto& t : threads) {
        t.join();
    }

    avg_throug = 0, avg_resp = 0;
    for (int i = 0; i < numthreads; ++i)
    {
        avg_response_time[i] /= num_requests[i];
        avg_throughput[i] /= duration_seconds;
        avg_throug += avg_throughput[i];
        avg_resp += avg_response_time[i];
    }
    avg_throug /= numthreads;
    avg_resp /= numthreads;

    threads.clear();
    std::cout << "Completed rotate_all load test\n";
    std::cout << "Average throughput (requests succesfully completed/sec): " << avg_throug << std::endl << "Average response time: " << avg_resp << "(ms) \n";
    
    std::cout << "---------------------------------------------------------------\n";
    
    // create_read_delete_mix():
    std::cout << "Starting mix_all load test\n";
    std::fill(avg_throughput.begin(), avg_throughput.end(), 0);
    std::fill(avg_response_time.begin(), avg_response_time.end(), 0);
    std::fill(num_requests.begin(), num_requests.end(), 0);

    // launch multiple threads
    for (int i = 0; i < numthreads; ++i) {
        threads.emplace_back(create_read_delete_mix, i);  // create and start a new thread
    }

    // wait for all threads to finish
    for (auto& t : threads) {
        t.join();
    }

    avg_throug = 0, avg_resp = 0;
    for (int i = 0; i < numthreads; ++i)
    {
        avg_response_time[i] /= num_requests[i];
        avg_throughput[i] /= duration_seconds;
        avg_throug += avg_throughput[i];
        avg_resp += avg_response_time[i];
    }
    avg_throug /= numthreads;
    avg_resp /= numthreads;

    threads.clear();

    std::cout << "Completed mix_all load test\n";
    std::cout << "Average throughput (requests succesfully completed/sec): " << avg_throug << std::endl << "Average response time: " << avg_resp << "(ms) \n";
    
    std::cout << "---------------------------------------------------------------\n";

    // delete_all(): 
    std::cout << "Starting delete_all load test\n";
    std::fill(avg_throughput.begin(), avg_throughput.end(), 0);
    std::fill(avg_response_time.begin(), avg_response_time.end(), 0);
    std::fill(num_requests.begin(), num_requests.end(), 0);

    // launch multiple threads
    for (int i = 0; i < numthreads; ++i) {
        threads.emplace_back(delete_all, i);  // create and start a new thread
    }

    // wait for all threads to finish
    for (auto& t : threads) {
        t.join();
    }
    
    avg_throug = 0, avg_resp = 0;
    for (int i = 0; i < numthreads; ++i)
    {
        avg_response_time[i] /= num_requests[i];
        avg_throughput[i] /= duration_seconds;
        avg_throug += avg_throughput[i];
        avg_resp += avg_response_time[i];
    }
    avg_throug /= numthreads;
    avg_resp /= numthreads;

    threads.clear();
    std::cout << "Completed delete_all load test\n";
    std::cout << "Average throughput (requests succesfully completed/sec): " << avg_throug << std::endl << "Average response time: " << avg_resp << "(ms) \n";
    std::cout << "---------------------------------------------------------------\n";
    
}
