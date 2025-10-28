// Compile using
/* g++ database.cpp -o database \
    -I/usr/local/include \
    -L/usr/local/lib -lcassandra -luv
*/
#include <cassandra.h>
#include <iostream>
#include <fstream>

int main()
{
    // 1. Connect to Cassandra
    CassCluster* cluster = cass_cluster_new();
    cass_cluster_set_contact_points(cluster, "127.0.0.1");

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

    // arbitrary addition
        
    // Read the image file as a string
    std::ifstream file("img/african_elephant/001.jpg", std::ios::binary);
    std::string img((std::istreambuf_iterator<char>(file)),
                     std::istreambuf_iterator<char>());

    const char* query =
        "INSERT INTO image_store (image_id, image_data) "
        "VALUES (?, ?);";
    std::string key = "000.jpg";
    stmt = cass_statement_new(query, 2);
    cass_statement_bind_string(stmt, 0, key.c_str());
    cass_statement_bind_bytes(stmt, 1, reinterpret_cast<const cass_byte_t*>(img.data()), img.size());

    future = cass_session_execute(session, stmt);
    cass_future_wait(future);

    if (cass_future_error_code(future) == CASS_OK)
        std::cout << "Image " << "000.jpg"<< " stored successfully.\n";
    else
        std::cerr << "Insert failed.\n";

    cass_statement_free(stmt);
    cass_future_free(future);

    //read
    query = "SELECT image_data from image_store where image_id=?;";
    stmt = cass_statement_new(query, 1);
    cass_statement_bind_string(stmt, 0, key.c_str());
    future = cass_session_execute(session, stmt);
    cass_future_wait(future);

    if (cass_future_error_code(future) == CASS_OK)
        std::cout << "Image " << "000.jpg"<< " read successfully.\n";
    else
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

        std::string img_data(reinterpret_cast<const char*>(img_bytes), img_size); // send this via rpc

        //std::ofstream out(key, std::ios::binary);
        //out.write(reinterpret_cast<const char*>(img_bytes), img_size);
        //out.close();
    }
    if (result != nullptr && cass_result_row_count(result) == 0)
    {
        // send "Key does not exist."
    }
    cass_statement_free(stmt);
    cass_future_free(future);
    
    // delete 
    query = "DELETE FROM image_store WHERE image_id = ?;";
    stmt = cass_statement_new(query, 1);
    cass_statement_bind_string(stmt, 0, key.c_str());
    future = cass_session_execute(session, stmt);
    cass_future_wait(future);

    if (cass_future_error_code(future) == CASS_OK)
        std::cout << "Image " << "000.jpg"<< " deleted successfully.\n";
    else
        std::cerr << "Delete failed.\n";
    cass_statement_free(stmt);
    cass_future_free(future);
    
    //while (1)
    //{
            //wait for requests from server  
    //}

}