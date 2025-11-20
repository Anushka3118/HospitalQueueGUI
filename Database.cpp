// Database.cpp
#include "Database.hpp"
#include <sqlite3.h>
#include <iostream>
#include <sstream>

static sqlite3 *g_db = nullptr;

static bool exec_sql(const std::string &sql) {
    char *err = nullptr;
    int rc = sqlite3_exec(g_db, sql.c_str(), nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::cerr << "SQLite error: " << (err ? err : "unknown") << "\n";
        sqlite3_free(err);
        return false;
    }
    return true;
}

bool db_init(const std::string &db_file) {
    int rc = sqlite3_open(db_file.c_str(), &g_db);
    if (rc != SQLITE_OK) {
        std::cerr << "Cannot open DB: " << sqlite3_errmsg(g_db) << "\n";
        sqlite3_close(g_db);
        g_db = nullptr;
        return false;
    }

    // Enable foreign keys (good practice)
    exec_sql("PRAGMA foreign_keys = ON;");

    // Create table patient_records (with checkup column)
    std::string create_patients =
        "CREATE TABLE IF NOT EXISTS patient_records ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL,"
        "age INTEGER,"
        "severity INTEGER,"
        "checkup TEXT,"
        "visit_time TEXT DEFAULT (datetime('now','localtime')),"
        "status TEXT DEFAULT 'waiting'"
        ");";

    if (!exec_sql(create_patients)) {
        std::cerr << "Failed to create table\n";
        return false;
    }

    return true;
}

int db_add_patient(const std::string &name, int age, int severity, const std::string &checkup) {
    if (!g_db) return -1;
    const char *sql = "INSERT INTO patient_records (name, age, severity, checkup, status) VALUES (?, ?, ?, ?, 'waiting');";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Prepare insert failed: " << sqlite3_errmsg(g_db) << "\n";
        return -1;
    }
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, age);
    sqlite3_bind_int(stmt, 3, severity);
    sqlite3_bind_text(stmt, 4, checkup.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        std::cerr << "Insert failed: " << sqlite3_errmsg(g_db) << "\n";
        sqlite3_finalize(stmt);
        return -1;
    }
    int id = (int)sqlite3_last_insert_rowid(g_db);
    sqlite3_finalize(stmt);
    return id;
}

std::vector<DBPatient> db_load_waiting() {
    std::vector<DBPatient> list;
    if (!g_db) return list;
    const char *sql = "SELECT id, name, age, severity, checkup, visit_time, status FROM patient_records WHERE status='waiting' ORDER BY severity DESC, id ASC;";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Prepare select failed: " << sqlite3_errmsg(g_db) << "\n";
        return list;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        DBPatient p;
        p.id = sqlite3_column_int(stmt, 0);
        p.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        p.age = sqlite3_column_int(stmt, 2);
        p.severity = sqlite3_column_int(stmt, 3);
        p.checkup = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4) ? sqlite3_column_text(stmt,4) : (const unsigned char*)(""));
        p.visit_time = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        p.status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        list.push_back(p);
    }
    sqlite3_finalize(stmt);
    return list;
}

bool db_mark_served(int patient_id) {
    if (!g_db) return false;
    const char *sql = "UPDATE patient_records SET status='served' WHERE id = ?;";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Prepare update failed: " << sqlite3_errmsg(g_db) << "\n";
        return false;
    }
    sqlite3_bind_int(stmt, 1, patient_id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

std::vector<DBPatient> db_load_history(int limit) {
    std::vector<DBPatient> list;
    if (!g_db) return list;
    std::stringstream ss;
    ss << "SELECT id, name, age, severity, checkup, visit_time, status FROM patient_records ORDER BY visit_time DESC";
    if (limit > 0) ss << " LIMIT " << limit;
    ss << ";";
    std::string sql = ss.str();
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(g_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Prepare history failed: " << sqlite3_errmsg(g_db) << "\n";
        return list;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        DBPatient p;
        p.id = sqlite3_column_int(stmt, 0);
        p.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        p.age = sqlite3_column_int(stmt, 2);
        p.severity = sqlite3_column_int(stmt, 3);
        p.checkup = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4) ? sqlite3_column_text(stmt,4) : (const unsigned char*)(""));
        p.visit_time = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        p.status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        list.push_back(p);
    }
    sqlite3_finalize(stmt);
    return list;
}

std::vector<DBPatient> db_search_by_name(const std::string &query) {
    std::vector<DBPatient> list;
    if (!g_db) return list;
    std::string sql = "SELECT id, name, age, severity, checkup, visit_time, status FROM patient_records WHERE lower(name) LIKE lower(?) ORDER BY visit_time DESC;";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(g_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Prepare search failed: " << sqlite3_errmsg(g_db) << "\n";
        return list;
    }
    std::string pattern = "%" + query + "%";
    sqlite3_bind_text(stmt, 1, pattern.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        DBPatient p;
        p.id = sqlite3_column_int(stmt, 0);
        p.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        p.age = sqlite3_column_int(stmt, 2);
        p.severity = sqlite3_column_int(stmt, 3);
        p.checkup = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4) ? sqlite3_column_text(stmt,4) : (const unsigned char*)(""));
        p.visit_time = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        p.status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        list.push_back(p);
    }
    sqlite3_finalize(stmt);
    return list;
}
