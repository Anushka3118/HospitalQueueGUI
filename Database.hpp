// Database.hpp
#pragma once

#include <string>
#include <vector>

struct DBPatient {
    int id;
    std::string name;
    int age;
    int severity;
    std::string checkup;     // NEW: type of checkup
    std::string visit_time;
    std::string status; // "waiting" or "served"
};

// Initialize DB (creates DB file and tables if missing)
bool db_init(const std::string &db_file = "patients.db");

// Add a patient; returns inserted patient id or -1 on error
int db_add_patient(const std::string &name, int age, int severity, const std::string &checkup);

// Load waiting patients (status = 'waiting'), ordered by severity DESC then id ASC
std::vector<DBPatient> db_load_waiting();

// Mark patient served by id
bool db_mark_served(int patient_id);

// Load full history (all patients ordered by visit_time DESC)
std::vector<DBPatient> db_load_history(int limit = 0); // limit=0 => no limit

// Search patients by name (case-insensitive substring search), returns matching records
std::vector<DBPatient> db_search_by_name(const std::string &query);
