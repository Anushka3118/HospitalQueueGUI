// main.cpp
#include <OpenGL/gl3.h>  // macOS friendly, no GLAD needed
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>

#include <sqlite3.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <string>
#include <cstring>

struct Patient {
    std::string name;
    int age;
    int severity;
    std::string checkup;
};

// Global queue
std::vector<Patient> patientQueue;

// SQLite DB pointer
sqlite3* db = nullptr;

// Sorting by severity DESC
void sortQueue() {
    std::sort(patientQueue.begin(), patientQueue.end(), [](const Patient &a, const Patient &b) {
        return a.severity > b.severity;
    });
}

// Initialize DB and table
bool initDB() {
    int rc = sqlite3_open("patients.db", &db);
    if(rc) {
        std::cerr << "Can't open database: " << sqlite3_errmsg(db) << "\n";
        return false;
    }
    const char* sqlCreate = R"(
        CREATE TABLE IF NOT EXISTS patients (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            age INTEGER,
            severity INTEGER,
            checkup TEXT
        );
    )";
    char* errMsg = nullptr;
    rc = sqlite3_exec(db, sqlCreate, nullptr, nullptr, &errMsg);
    if(rc != SQLITE_OK) {
        std::cerr << "SQL error: " << errMsg << "\n";
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

// Load all patients from DB
void loadPatientsFromDB() {
    const char* sqlSelect = "SELECT name, age, severity, checkup FROM patients ORDER BY severity DESC;";
    sqlite3_stmt* stmt;
    if(sqlite3_prepare_v2(db, sqlSelect, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare select statement\n";
        return;
    }
    while(sqlite3_step(stmt) == SQLITE_ROW) {
        std::string name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        int age = sqlite3_column_int(stmt, 1);
        int severity = sqlite3_column_int(stmt, 2);
        std::string checkup = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        patientQueue.push_back({name, age, severity, checkup});
    }
    sqlite3_finalize(stmt);
}

// Insert patient to DB
void insertPatientToDB(const Patient& p) {
    const char* sqlInsert = "INSERT INTO patients (name, age, severity, checkup) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    if(sqlite3_prepare_v2(db, sqlInsert, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare insert statement\n";
        return;
    }
    sqlite3_bind_text(stmt, 1, p.name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, p.age);
    sqlite3_bind_int(stmt, 3, p.severity);
    sqlite3_bind_text(stmt, 4, p.checkup.c_str(), -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

int main() {
    if(!initDB()) return -1;
    loadPatientsFromDB();

    // ------------------ GLFW INIT ------------------
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    GLFWwindow *window = glfwCreateWindow(1280, 720, "Hospital Queue GUI", NULL, NULL);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // ------------------ IMGUI INIT ------------------
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    const char *glsl_version = "#version 150";
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Input fields
    char nameBuffer[64] = "";
    int age = 0;
    int severity = 1;
    char checkupBuffer[64] = "";

    // Search
    char searchBuffer[64] = "";

    const int avgTime = 7; // min per patient

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Hospital Patient Queue");

        // ADD PATIENT
        ImGui::Text("Add New Patient");
        ImGui::InputText("Name", nameBuffer, IM_ARRAYSIZE(nameBuffer));
        ImGui::InputInt("Age", &age);
        ImGui::SliderInt("Severity (1-5)", &severity, 1, 5);
        ImGui::InputText("Checkup", checkupBuffer, IM_ARRAYSIZE(checkupBuffer));

        if (ImGui::Button("Add Patient")) {
            if(strlen(nameBuffer) > 0) {
                Patient p{nameBuffer, age, severity, checkupBuffer};
                patientQueue.push_back(p);
                insertPatientToDB(p);
                sortQueue();
                nameBuffer[0] = '\0'; age = 0; severity = 1; checkupBuffer[0] = '\0';
            }
        }

        ImGui::SameLine();
        if(ImGui::Button("Clear Queue")) patientQueue.clear();

        ImGui::Separator();

        // SEARCH
        ImGui::InputText("Search Patient", searchBuffer, IM_ARRAYSIZE(searchBuffer));
        std::string query = searchBuffer; std::transform(query.begin(), query.end(), query.begin(), ::tolower);

        ImGui::Separator();
        ImGui::Text("Queue:");

        if(ImGui::BeginChild("QueueScroll", ImVec2(0, 400), true)) {
            if(ImGui::BeginTable("QueueTable", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("Index");
                ImGui::TableSetupColumn("Name");
                ImGui::TableSetupColumn("Age");
                ImGui::TableSetupColumn("Severity");
                ImGui::TableSetupColumn("Checkup");
                ImGui::TableSetupColumn("Est. Wait Time");
                ImGui::TableHeadersRow();

                for(int i=0; i<patientQueue.size(); i++) {
                    const Patient& p = patientQueue[i];
                    std::string nameLow = p.name; std::transform(nameLow.begin(), nameLow.end(), nameLow.begin(), ::tolower);
                    if(!query.empty() && nameLow.find(query) == std::string::npos) continue;

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::Text("%d", i+1);
                    ImGui::TableSetColumnIndex(1); ImGui::Text("%s", p.name.c_str());
                    ImGui::TableSetColumnIndex(2); ImGui::Text("%d", p.age);
                    ImGui::TableSetColumnIndex(3);
                    ImVec4 color = (p.severity>=5) ? ImVec4(1,0.2,0.2,1) : (p.severity>=3)?ImVec4(1,0.6,0.2,1):ImVec4(0.2,1,0.2,1);
                    ImGui::TextColored(color, "%d", p.severity);
                    ImGui::TableSetColumnIndex(4); ImGui::Text("%s", p.checkup.c_str());
                    ImGui::TableSetColumnIndex(5); ImGui::Text("%d min", i*avgTime);
                }

                ImGui::EndTable();
            }
        }
        ImGui::EndChild();

        if(!patientQueue.empty() && ImGui::Button("Call Next Patient")) {
            std::cout << "Calling: " << patientQueue[0].name << "\n";
            patientQueue.erase(patientQueue.begin());
        }

        ImGui::End();

        // RENDER
        ImGui::Render();
        int w,h; glfwGetFramebufferSize(window,&w,&h);
        glViewport(0,0,w,h);
        glClearColor(0.2f,0.2f,0.2f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    sqlite3_close(db);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
