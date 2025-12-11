#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>
#include "write_log.h"
#include <sqlite3.h>

// ==============================================================
// Function: write log to write_log.log file
void write_log_log(const char *write_log, const char *level, const char *format, ...)
{
    //==============================================================
    // const char *write_log:  table name in SQLite database
    // const char *level: log level (e.g., "INFO", "ERROR")
    // const char *format: format string for the log message
    //==============================================================
    FILE *fp = fopen(write_log, "a"); // open file in append mode
    if (!fp)
    {
        return;
    }

    time_t now = time(NULL); // get time lolcal
    struct tm *t = localtime(&now);
    fprintf(fp, "%04d-%02d-%02d %02d:%02d:%02d [%s] ",
            t->tm_year + 1900, t->tm_mon + 1,
            t->tm_mday, t->tm_hour,
            t->tm_min,
            t->tm_sec, level);

    va_list args; // wirte log message
    va_start(args, format);
    vfprintf(fp, format, args);
    va_end(args);

    fprintf(fp, "\n");
    fclose(fp);
}

//==============================================================
// Function: write log to logs table in modbus_mapping.db
void write_log_db(sqlite3 *db, const char *level, const char *format, ...)
{
    char message[512];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    // Lấy thời gian hiện tại dạng YYYY-MM-DD HH:MM:SS
    char timestamp[64];
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);

    // Câu lệnh SQL - bảng cố định là 'logs'
    char sql[1024];
    snprintf(sql, sizeof(sql), "INSERT INTO logs (timestamp, service, message) VALUES ('%s', '%s', '%s');", timestamp, level, message);

    // Thực thi câu lệnh
    char *errmsg = NULL;
    if (sqlite3_exec(db, sql, 0, 0, &errmsg) != SQLITE_OK)
    {
        fprintf(stderr, "Lỗi ghi log vào DB: %s\n", errmsg);
        sqlite3_free(errmsg);
    }
}