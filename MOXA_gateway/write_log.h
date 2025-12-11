#ifndef LOGGER_H
#define LOGGER_H
#include <sqlite3.h>
// Ghi log vào file
void write_log_log(const char *filename, const char *level, const char *format, ...);
void write_log_db(sqlite3 *db, const char *level, const char *format, ...);

#endif
