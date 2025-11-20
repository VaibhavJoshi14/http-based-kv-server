// Use cqlsh if u want to run apache_cassandra on shell. 
#include <cassandra.h>
#include <iostream>
#include <fstream>
#include "include/httplib.h"
#include <sched.h>
#include <csignal>
#include <fstream>
#include <unistd.h>
#include <sys/sysinfo.h>

#define DB_IP "127.0.0.1"
#define DB_port 5001
#define CPU_core_id 1 // used to pin the process to core. used for load testing.


struct CpuTimes {
    long long user, nice, system, idle, iowait, irq, softirq;
};

unsigned long readIOTime() {
    std::ifstream f("/sys/block/sda/stat");   // or nvme0n1
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
    double interval = 5*60*1000;      // measurement window in ms

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

    std::cout << "Percentage RAM used: " << p << "%\n";

    exit(0);
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

    std::cout << "Pinned database process " << pid << " to CPU core " << CPU_core_id << std::endl;

    httplib::Server db_svr; // used to communicate between server and database

    // 1. Connect to Cassandra
    CassCluster* cluster = cass_cluster_new();
    cass_cluster_set_contact_points(cluster, DB_IP);

    CassSession* session = cass_session_new();
    CassFuture* connect_future = cass_session_connect(session, cluster);
    cass_future_wait(connect_future);
    cass_future_free(connect_future);

    // 2. Create keyspace
    const char* create_keyspace =
        "CREATE KEYSPACE IF NOT EXISTS IMAGE_STORE "
        "WITH replication = {'class': 'SimpleStrategy', 'replication_factor': 1};";
    CassStatement* stmt = cass_statement_new(create_keyspace, 0);
    CassFuture* future = cass_session_execute(session, stmt);
    cass_future_wait(future);
    cass_statement_free(stmt);
    cass_future_free(future);

    // 3. Use keyspace
    stmt = cass_statement_new("USE IMAGE_STORE;", 0);
    future = cass_session_execute(session, stmt);
    cass_future_wait(future);
    cass_statement_free(stmt);
    cass_future_free(future);

    // 4. Create table
    const char* create_table =
        "CREATE TABLE IF NOT EXISTS image_store ("
        "image_id text PRIMARY KEY, " 
        "image_data BLOB);";
    stmt = cass_statement_new(create_table, 0);
    future = cass_session_execute(session, stmt);
    cass_future_wait(future);
    cass_statement_free(stmt);
    cass_future_free(future);

    const char* query;
    std::string key, img;
    db_svr.Post("/create", [&](const httplib::Request& req, httplib::Response& res){
        auto it = req.form.files.find("file");
        const auto& file = it->second;

        key = file.filename;
        img = file.content;

        query =
        "INSERT INTO image_store (image_id, image_data) "
        "VALUES (?, ?);";

        stmt = cass_statement_new(query, 2);
        cass_statement_bind_string(stmt, 0, key.c_str());
        cass_statement_bind_bytes(stmt, 1, reinterpret_cast<const cass_byte_t*>(img.data()), img.size());

        future = cass_session_execute(session, stmt);
        cass_future_wait(future);

        if (cass_future_error_code(future) == CASS_OK)
        {    //std::cout << "Image " << key << " stored successfully.\n";
        }
        else
            std::cerr << "Insert " << key << " failed.\n";

        cass_statement_free(stmt);
        cass_future_free(future);
    });

    db_svr.Get("/read", [&](const httplib::Request& req, httplib::Response& res){
        std::string key = req.get_param_value("key");
        std::string value;
        query = "SELECT image_data from image_store where image_id=?;";
        stmt = cass_statement_new(query, 1);
        cass_statement_bind_string(stmt, 0, key.c_str());
        future = cass_session_execute(session, stmt);
        cass_future_wait(future);

        if (cass_future_error_code(future) != CASS_OK)
            std::cerr << "Read failed.\n";
        
        const CassResult* result = cass_future_get_result(future);
        if (result != nullptr && cass_result_row_count(result) > 0)
        {
            // Get first (and only) row
            const CassRow* row = cass_result_first_row(result);
            const CassValue* img_val = cass_row_get_column_by_name(row, "image_data");

            // get the image data as bytes
            const cass_byte_t* img_bytes;
            size_t img_size;
            cass_value_get_bytes(img_val, &img_bytes, &img_size);

            std::string img_data(reinterpret_cast<const char*>(img_bytes), img_size); // send this
            res.set_content(img_data, "image/jpeg");
        }
        if (result != nullptr && cass_result_row_count(result) == 0)
        {
            res.set_content("Key does not exist.", "text/plain");
        }
    });
    
    db_svr.Post("/delete", [&](const httplib::Request& req, httplib::Response& res){
        std::string key = req.get_param_value("key");
        query = "DELETE FROM image_store WHERE image_id = ?;";
        stmt = cass_statement_new(query, 1);
        cass_statement_bind_string(stmt, 0, key.c_str());
        future = cass_session_execute(session, stmt);
        cass_future_wait(future);
        if (cass_future_error_code(future) == CASS_OK)
        {   //std::cout << "Image " << key<< " deleted successfully.\n"; 
        }
        else
            std::cerr << "Delete failed.\n";
        cass_statement_free(stmt);
        cass_future_free(future);
    });
    
    db_svr.Get("/printStatistics", [&](const httplib::Request& req, httplib::Response& res){
        printStats();
        t1 = readIOTime();
        c1 = readCPU();
    });

    db_svr.listen(DB_IP, DB_port); // start listening.

}