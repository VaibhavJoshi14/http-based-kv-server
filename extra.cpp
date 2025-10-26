#include <cassandra.h>
#include <iostream>

void check_future_error(CassFuture* future, const std::string& msg) {
    if (cass_future_error_code(future) != CASS_OK) {
        const char* message;
        size_t message_length;
        cass_future_error_message(future, &message, &message_length);
        std::cerr << msg << ": " << std::string(message, message_length) << std::endl;
    }
}

int main() {
    // 1. Connect to Cassandra
    CassCluster* cluster = cass_cluster_new();
    cass_cluster_set_contact_points(cluster, "127.0.0.1");

    CassSession* session = cass_session_new();
    CassFuture* connect_future = cass_session_connect(session, cluster);
    cass_future_wait(connect_future);
    check_future_error(connect_future, "Connection failed");
    cass_future_free(connect_future);

    // 2. Create keyspace
    const char* create_keyspace =
        "CREATE KEYSPACE IF NOT EXISTS user_profiles "
        "WITH replication = {'class': 'SimpleStrategy', 'replication_factor': 1};";
    CassStatement* stmt = cass_statement_new(create_keyspace, 0);
    CassFuture* future = cass_session_execute(session, stmt);
    cass_future_wait(future);
    check_future_error(future, "Create keyspace failed");
    cass_statement_free(stmt);
    cass_future_free(future);

    // 3. Use keyspace
    stmt = cass_statement_new("USE user_profiles;", 0);
    future = cass_session_execute(session, stmt);
    cass_future_wait(future);
    check_future_error(future, "USE keyspace failed");
    cass_statement_free(stmt);
    cass_future_free(future);

    // 4. Create table
    const char* create_table =
        "CREATE TABLE IF NOT EXISTS users ("
        "user_id uuid PRIMARY KEY, "
        "name text, "
        "email text, "
        "age int);";
    stmt = cass_statement_new(create_table, 0);
    future = cass_session_execute(session, stmt);
    cass_future_wait(future);
    check_future_error(future, "Create table failed");
    cass_statement_free(stmt);
    cass_future_free(future);

    // 5. Insert a user
    const char* insert_user =
        "INSERT INTO users (user_id, name, email, age) "
        "VALUES (uuid(), 'Charlie', 'charlie@example.com', 28);";
    stmt = cass_statement_new(insert_user, 0);
    future = cass_session_execute(session, stmt);
    cass_future_wait(future);
    check_future_error(future, "Insert user failed");
    cass_statement_free(stmt);
    cass_future_free(future);

    // 6. Query all users
    stmt = cass_statement_new("SELECT user_id, name, email, age FROM users;", 0);
    future = cass_session_execute(session, stmt);
    cass_future_wait(future);

    if (cass_future_error_code(future) == CASS_OK) {
        const CassResult* result = cass_future_get_result(future);
        if (result) {
            CassIterator* iterator = cass_iterator_from_result(result);
            while (cass_iterator_next(iterator)) {
                const CassRow* row = cass_iterator_get_row(iterator);

                const char* name;
                size_t name_len;
                cass_value_get_string(cass_row_get_column_by_name(row, "name"), &name, &name_len);

                const char* email;
                size_t email_len;
                cass_value_get_string(cass_row_get_column_by_name(row, "email"), &email, &email_len);

                int age;
                cass_value_get_int32(cass_row_get_column_by_name(row, "age"), &age);

                std::cout << "User: " << std::string(name, name_len)
                          << ", Email: " << std::string(email, email_len)
                          << ", Age: " << age << std::endl;
            }
            cass_iterator_free(iterator);
            cass_result_free(result);
        } else {
            std::cerr << "SELECT returned no result" << std::endl;
        }
    } else {
        check_future_error(future, "SELECT query failed");
    }

    cass_statement_free(stmt);
    cass_future_free(future);

    // 7. Close session
    CassFuture* close_future = cass_session_close(session);
    cass_future_wait(close_future);
    cass_future_free(close_future);

    cass_cluster_free(cluster);
    cass_session_free(session);

    return 0;
}
