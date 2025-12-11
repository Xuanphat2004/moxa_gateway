#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>         // process multi-thread
#include <sqlite3.h>         // SQLite database
#include <hiredis/hiredis.h> // redis server
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <jansson.h>
#include <arpa/inet.h>
#include <modbus/modbus.h>
#include "write_log.h" // include write_log function

#define PORT 1502                 // TCP port for Cloud connection
#define CLOUD_ADDRESS "127.0.0.1" // IP address of Cloud server
#define BUFFER_SIZE 256           // Buffer size for TCP packets
#define MAX_QUEUE 100             // number of requests in queue
int lookup_mapped_address(sqlite3 *db, int rtu_id, int tcp_address);

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;         // declare mutex for queue access (mutex = mutual exclusion lock))
pthread_cond_t cond_var = PTHREAD_COND_INITIALIZER;        // condition variable for thread synchronization
pthread_mutex_t pending_mutex = PTHREAD_MUTEX_INITIALIZER; // mutex for protect response array
int queue_head_index = 0;                                  // head index
int queue_final_index = 0;                                 // final index
int pending_count = 0;                                     // number of responses pending

// ===== declare queue for request packets - FIFO structure =====
typedef struct
// structure for modbus TCP packet
{
    int transaction_id; // int = 4 bytes
    int protocol_id;
    int length;
    int rtu_id;
    int address;
    int function;
    int quantity;
    int client_sock;
} RequestPacket;
RequestPacket request_queue[MAX_QUEUE];

// ===== array contain RTU feedback for TCP server =====
typedef struct
{
    int transaction_id;
    int client_sock;
} corresponding_address;
corresponding_address pending_responses[100]; //  save response from RTU server

// ===== Function: add new request into queue =====
void add_queue(RequestPacket new_pkt)
{
    pthread_mutex_lock(&mutex);                              // lock before writing into Queue
    request_queue[queue_final_index] = new_pkt;              // writing new packet to queue at rear position
    queue_final_index = (queue_final_index + 1) % MAX_QUEUE; // increase index, % MAX_QUEUE help to return to queue_rear = 0 (index =0)
    pthread_cond_signal(&cond_var);                          // announce for thread is waiting
    pthread_mutex_unlock(&mutex);                            // unlock
}

//=====================================================================================================
// ===== Function: take packet out of queue ===========================================================
RequestPacket take_queue()
{
    pthread_mutex_lock(&mutex);
    while (queue_head_index == queue_final_index) // if queue is empty (index = 0)
    {
        pthread_cond_wait(&cond_var, &mutex); // waiting for new packet
    }
    RequestPacket next_packet = request_queue[queue_head_index]; // take request to process
    queue_head_index = (queue_head_index + 1) % MAX_QUEUE;
    pthread_mutex_unlock(&mutex); // unlock

    return next_packet;
}

// ===== thread 1: receive request packet from Cloud =====
void *tcp_receiver_thread(void *arg) // argument
{
    sqlite3 *db;
    sqlite3_open("modbus_mapping.db", &db);
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);  // -----------------------------------
    struct sockaddr_in addr = {0};                   // Cấu trúc địa chỉ server
    addr.sin_family = AF_INET;                       // AF_INET -> IPv4
    addr.sin_port = htons(PORT);                     // declare TCP port connection
    addr.sin_addr.s_addr = inet_addr(CLOUD_ADDRESS); // accept connection with IP address
    bind(listenfd, (struct sockaddr *)&addr, sizeof(addr));
    listen(listenfd, 5);
    printf("\n");
    printf("[TCP connect with Cloud] Listening on port %d...\n", PORT); // ------------------------------------
    // write_log_log("write_log.log", "INFO", "[TCP connect with Cloud] Listening on port %d...", PORT);

    while (1)
    {
        int client_sock = accept(listenfd, NULL, NULL);
        if (client_sock >= 0)
        {
            uint8_t buffer[260];                                      // data buffer
            int bytes = recv(client_sock, buffer, sizeof(buffer), 0); // receice data packet from Cloud
            if (bytes >= 12)                                          // default modbus TCP packet length >= 12 bytes
            {
                printf("[TCP Server receive packet] Received packet from Cloud\n");
                RequestPacket next_packet;
                next_packet.transaction_id = (buffer[0] << 8) | buffer[1];
                next_packet.protocol_id = (buffer[2] << 8) | buffer[3];
                next_packet.length = (buffer[4] << 8) | buffer[5];
                next_packet.rtu_id = (buffer[6] << 8) | buffer[7];
                next_packet.address = (buffer[8] << 8) | buffer[9];
                next_packet.function = (buffer[10] << 8) | buffer[11];
                next_packet.quantity = (buffer[12] << 8) | buffer[13];
                next_packet.client_sock = client_sock;
                add_queue(next_packet);
                // write_log_log("write_log.log", "INFO", "Received packet: transaction_id=%d, rtu_id=%d, address=%d, function=%d, quantity=%d",
                //           next_packet.transaction_id, next_packet.rtu_id, next_packet.address, next_packet.function, next_packet.quantity);
            }
            else
            {
                printf("[TCP Server receive packet] Invalid packet !!!\n");
                close(client_sock);
            }
        }
    }
    return NULL;
}

// ===== thread 2: processing data and mapping address with SQite and send request for rtu server =====
void *process_request_thread(void *arg)
{
    sqlite3 *db;
    sqlite3_open("modbus_mapping.db", &db);                // connect to SQLite database mapping.db
    redisContext *redis = redisConnect("127.0.0.1", 6379); // connect with Redis

    while (1)
    {
        RequestPacket packet = take_queue(); // take next packet from queue
        printf("[TCP Server processing] Handling transaction ID: %d\n", packet.transaction_id);
        // printf("[DB] Lookup for address %d\n", packet.address);    // mapping address
        int new_address = lookup_mapped_address(db, packet.rtu_id, packet.address);
        if (new_address < 0)
        {
            printf("[TCP Server mapping] Failed to mapping address %d for RTU ID %d\n", packet.address, packet.rtu_id);
            continue; // skip this request if mapping failed
        }
        // send request to Redis server
        char json_packet[256];
        snprintf(json_packet, sizeof(json_packet),
                 "{\"transaction_id\":%d, \"protocol_id\":%d, \"length\":%d, \"rtu_id\":%d,\"rtu_address\":%d,\"function\":%d,\"quantity\":%d}",
                 packet.transaction_id,
                 packet.protocol_id,
                 packet.length,
                 packet.rtu_id,
                 new_address,
                 packet.function,
                 packet.quantity);

        printf("[TCP Server send request] Sending request to Redis: %s\n", json_packet);
        // write_log_log("write_log.log", "INFO", "[TCP Server send request] Sending request to Redis: %s", json_packet);
        // write_log_db(db, "INFO", "Sending request to Redis: %s", json_packet);

        redisCommand(redis, "PUBLISH modbus_request %s", json_packet); // send request to Redis channel - modbus_request
        pthread_mutex_lock(&pending_mutex);                            // save socket, is waiting for response from RTU server
        pending_responses[pending_count].transaction_id = packet.transaction_id;
        pending_responses[pending_count].client_sock = packet.client_sock;
        pending_count++;
        pthread_mutex_unlock(&pending_mutex);
    }

    redisFree(redis); // clean up Redis connection

    return NULL;
}

// ===== thread 3: listen response form Redis and send for TCP client =====
void *response_listener_thread(void *arg)
{
    sqlite3 *db;
    sqlite3_open("modbus_mapping.db", &db);
    redisContext *redis = redisConnect("127.0.0.1", 6379); // connect to Redis
    redisReply *reply = redisCommand(redis, "SUBSCRIBE modbus_response");
    if (reply) // wait for response from Redis channel - modbus_response
    {
        freeReplyObject(reply);
        printf("[TCP Server wait for response] waiting for responses from RTU server ...\n");
    }

    while (1)
    {
        redisReply *message_reply; // declare a msg pointer of type redisReply (data type of hiredis library) to contain the response received from Redis.
        if (redisGetReply(redis, (void **)&message_reply) == REDIS_OK && message_reply)
        {

            //-------------------------------------------------------------------------------------------------
            //                     JSON data format:
            //                        "message"                    -> element[0] - type of message,
            //                    "modbus_response"                -> element[1] - channel name,
            // "{\"transaction_id\":1,\"status\":0,\"value\":123}" -> element[2] - main data
            //-------------------------------------------------------------------------------------------------

            if (message_reply->type == REDIS_REPLY_ARRAY && message_reply->elements == 3)
            {
                const char *json_str = message_reply->element[2]->str;
                json_error_t error;
                json_t *root = json_loads(json_str, 0, &error); // Parse JSON
                if (!root)
                {
                    fprintf(stderr, "JSON parse error: %s\n", error.text);
                    freeReplyObject(message_reply);
                    continue;
                }

                uint8_t transaction_id = json_integer_value(json_object_get(root, "transaction_id"));
                uint8_t rtu_id = json_integer_value(json_object_get(root, "rtu_id"));
                int address = json_integer_value(json_object_get(root, "rtu_address"));
                int function = json_integer_value(json_object_get(root, "function"));
                // int status = json_integer_value(json_object_get(root, "status"));
                int value = json_integer_value(json_object_get(root, "value"));

                printf("[TCP Server receive response] Received data for transaction_id %d with value %d\n", transaction_id, value);
                // write_log_log("write_log.log", "INFO", "[TCP Server receive response] Received data for transaction_id %d with value %d", transaction_id, value);
                // write_log_db(db, "INFO", "Received data for transaction_id %d with value %d", transaction_id, value);

                pthread_mutex_lock(&pending_mutex);
                int found = 0;
                for (int i = 0; i < pending_count; ++i)
                {
                    if (pending_responses[i].transaction_id == transaction_id)
                    {
                        int client_sock = pending_responses[i].client_sock;

                        uint8_t response[8] = {transaction_id, rtu_id, address, function, (value >> 8) & 0xFF, value & 0xFF, 0, 0};
                        send(client_sock, response, 8, 0); // feedback response to Cloud server
                        printf("[TCP Server receive packet] Value response for client have device ID: %d is %d\n", rtu_id, value);

                        printf("\n");
                        close(client_sock); // close client socket

                        for (int j = i; j < (pending_count - 1); j++) // delete response from pending_responses
                        {
                            pending_responses[j] = pending_responses[j + 1];
                        }
                        pending_count--;
                        found = 1;

                        break;
                    }
                }
                pthread_mutex_unlock(&pending_mutex);
                if (!found)
                {
                    printf("[TCP Server status] Unknown transaction_id: %d\n", transaction_id);
                }
                json_decref(root);
            }
            freeReplyObject(message_reply);
        }
    }
    redisFree(redis);
    return NULL;
}

// ==== Function: mapping address in SQLite ====
int lookup_mapped_address(sqlite3 *db, int rtu_id, int tcp_address)
{
    int new_address = tcp_address;
    const char *sql = "SELECT rtu_address FROM mapping WHERE rtu_id = ? AND tcp_address = ?";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, rtu_id);
        sqlite3_bind_int(stmt, 2, tcp_address);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            new_address = sqlite3_column_int(stmt, 0);
            // write_log_log("write_log.log", "INFO", "[TCP Server mapping] Found mapping: %d -> %d", tcp_address, new_address);
            // write_log_db(db, "INFO", "Found mapping: %d -> %d", tcp_address, new_address);
            printf("[TCP Server mapping] Found mapping: %d -> %d\n", tcp_address, new_address);
        }
        else
        {
            printf("[TCP Server mapping] No mapping found for address %d !!!\n", tcp_address);
            new_address = -1;
        }
        sqlite3_finalize(stmt); // clean up SQLite memory
    }
    else
    {
        printf("[TCP Server mapping] Table don't have columm match !!! \n");
        new_address = -1;
    }

    return new_address;
}
void get_data_mannual()
{
    // This function is currently not implemented.
}

// ===== main: create and run tasks =====
int main()
{
    pthread_t receive_thread, process_thread, response_thread; // contain ID of threads
    pthread_create(&receive_thread, NULL, tcp_receiver_thread, NULL);
    pthread_create(&process_thread, NULL, process_request_thread, NULL);
    pthread_create(&response_thread, NULL, response_listener_thread, NULL);
    pthread_join(receive_thread, NULL);
    pthread_join(process_thread, NULL);
    pthread_join(response_thread, NULL);

    return 0;
}
