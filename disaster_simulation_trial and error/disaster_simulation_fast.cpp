#include <iostream>
#include <vector>
#include <memory>
#include <queue>
#include <cmath>
#include <algorithm>
#include <random>
#include <string>
#include <windows.h>
#include <map>
#include <iomanip>

using namespace std;

// Constants
const int GRID_SIZE = 25;
const int DISPLAY_WIDTH = 80;
const int DISPLAY_HEIGHT = 30;

// Console Colors
const int COLORS[] = {
    0,  // BLACK
    1,  // BLUE
    2,  // GREEN
    3,  // CYAN
    4,  // RED
    5,  // MAGENTA
    6,  // YELLOW
    7,  // WHITE
    8,  // GRAY
    9,  // BRIGHT_BLUE
    10, // BRIGHT_GREEN
    11, // BRIGHT_CYAN
    12, // BRIGHT_RED
    13, // BRIGHT_MAGENTA
    14, // BRIGHT_YELLOW
    15  // BRIGHT_WHITE
};

enum class CellType { EMPTY, BUILDING, FIRE, VICTIM, HOSPITAL, DEBRIS, WATER };
enum class AgentType { RESCUE_TEAM, FIRE_BRIGADE, AMBULANCE };
enum class TaskType { EXTINGUISH_FIRE, RESCUE_VICTIM, TRANSPORT_TO_HOSPITAL, CLEAR_DEBRIS };

// ==================== UTILITY CLASSES ====================
class Position {
public:
    int x, y;
    Position(int x = 0, int y = 0) : x(x), y(y) {}
    bool operator==(const Position& other) const { return x == other.x && y == other.y; }
    double distanceTo(const Position& other) const { return sqrt(pow(x - other.x, 2) + pow(y - other.y, 2)); }
};

class ConsoleUtils {
public:
    static void setColor(int color) {
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
    }
    
    static void clearScreen() {
        system("cls");
    }
    
    static void sleep(int ms) {
        Sleep(ms);
    }
    
    static void setCursor(int x, int y) {
        COORD coord = {static_cast<SHORT>(x), static_cast<SHORT>(y)};
        SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
    }
};

// ==================== CELL CLASS ====================
class Cell {
public:
    Position pos;
    CellType type;
    int fireIntensity;
    int victimHealth;
    bool hasVictim;
    bool victimRescued;
    
    Cell(int x, int y) : pos(x, y), type(CellType::EMPTY), fireIntensity(0), 
                        victimHealth(100), hasVictim(false), victimRescued(false) {}
    
    void update() {
        // Fire spread logic
        if (type == CellType::FIRE && fireIntensity < 100) {
            fireIntensity = min(100, fireIntensity + 8);
        }
        
        // Victim health deterioration
        if (hasVictim && !victimRescued) {
            victimHealth = max(0, victimHealth - (type == CellType::FIRE ? 8 : 3));
        }
        
        // Random fire spread
        static random_device rd;
        static mt19937 gen(rd());
        if (type == CellType::FIRE && uniform_real_distribution<>(0,1)(gen) < 0.15) {
            // Mark for fire spread (handled in environment)
        }
    }
    
    char getChar() const {
        if (hasVictim && !victimRescued) return 'V';
        switch(type) {
            case CellType::EMPTY: return ' ';
            case CellType::BUILDING: return 'B';
            case CellType::FIRE: return fireIntensity > 50 ? 'F' : 'f';
            case CellType::HOSPITAL: return 'H';
            case CellType::DEBRIS: return 'D';
            case CellType::WATER: return 'W';
            default: return '?';
        }
    }
    
    int getColor() const {
        if (hasVictim && !victimRescued) {
            if (victimHealth > 60) return COLORS[13]; // Bright Magenta
            if (victimHealth > 30) return COLORS[5];  // Magenta
            return COLORS[4]; // Red
        }
        
        switch(type) {
            case CellType::EMPTY: return COLORS[0];
            case CellType::BUILDING: return COLORS[1];
            case CellType::FIRE: return fireIntensity > 50 ? COLORS[12] : COLORS[4];
            case CellType::HOSPITAL: return COLORS[10];
            case CellType::DEBRIS: return COLORS[6];
            case CellType::WATER: return COLORS[11];
            default: return COLORS[7];
        }
    }
    
    bool isTraversable() const {
        return type != CellType::DEBRIS && fireIntensity < 60;
    }
};

// ==================== TASK CLASS ====================
class Task {
public:
    TaskType type;
    Position position;
    int urgency;
    string assignedTo;
    bool completed;
    
    Task(TaskType t, Position pos, int urg) 
        : type(t), position(pos), urgency(urg), completed(false) {}
    
    double calculateCost(const Position& agentPos, AgentType agentType) const {
        double dist = position.distanceTo(agentPos);
        double urgencyBonus = (100 - urgency) * 0.05;
        
        // Agent specialization
        double efficiency = 1.0;
        switch(agentType) {
            case AgentType::FIRE_BRIGADE: 
                efficiency = (type == TaskType::EXTINGUISH_FIRE) ? 0.3 : 1.5; break;
            case AgentType::RESCUE_TEAM: 
                efficiency = (type == TaskType::RESCUE_VICTIM || type == TaskType::CLEAR_DEBRIS) ? 0.4 : 1.3; break;
            case AgentType::AMBULANCE: 
                efficiency = (type == TaskType::TRANSPORT_TO_HOSPITAL) ? 0.3 : 1.6; break;
        }
        
        return dist + urgencyBonus + efficiency;
    }
    
    string getTypeName() const {
        switch(type) {
            case TaskType::EXTINGUISH_FIRE: return "FIRE";
            case TaskType::RESCUE_VICTIM: return "RESCUE";
            case TaskType::TRANSPORT_TO_HOSPITAL: return "TRANSPORT";
            case TaskType::CLEAR_DEBRIS: return "CLEAR";
            default: return "UNKNOWN";
        }
    }
};

// ==================== AGENT BASE CLASS ====================
class Agent {
protected:
    static int nextId;
    int id;
    AgentType type;
    Position position;
    vector<Position> path;
    int resources;
    bool busy;
    shared_ptr<Task> currentTask;
    int stepsToComplete;
    
public:
    Agent(AgentType t, Position pos) : id(nextId++), type(t), position(pos), 
                                      resources(100), busy(false), stepsToComplete(0) {}
    
    virtual ~Agent() = default;
    
    // Getters
    int getId() const { return id; }
    Position getPosition() const { return position; }
    bool isBusy() const { return busy; }
    string getTypeName() const {
        switch(type) {
            case AgentType::RESCUE_TEAM: return "RESCUE";
            case AgentType::FIRE_BRIGADE: return "FIRE";
            case AgentType::AMBULANCE: return "AMBUL";
            default: return "AGENT";
        }
    }
    
    char getChar() const {
        switch(type) {
            case AgentType::RESCUE_TEAM: return 'R';
            case AgentType::FIRE_BRIGADE: return 'F';
            case AgentType::AMBULANCE: return 'A';
            default: return '?';
        }
    }
    
    int getColor() const {
        switch(type) {
            case AgentType::RESCUE_TEAM: return COLORS[14]; // Bright Yellow
            case AgentType::FIRE_BRIGADE: return COLORS[12]; // Bright Red
            case AgentType::AMBULANCE: return COLORS[11]; // Bright Cyan
            default: return COLORS[15];
        }
    }
    
    void setPath(const vector<Position>& newPath) { 
        path = newPath; 
        if (!path.empty()) path.erase(path.begin()); // Remove current position
    }
    
    void assignTask(shared_ptr<Task> task) { 
        currentTask = task; 
        busy = true; 
        stepsToComplete = 3 + (rand() % 3);
        task->assignedTo = getTypeName() + to_string(id);
    }
    
    virtual void move(vector<vector<Cell>>& grid) {
        if (!path.empty()) {
            Position next = path.front();
            if (next.x >= 0 && next.x < grid.size() && next.y >= 0 && next.y < grid[0].size() &&
                grid[next.x][next.y].isTraversable()) {
                position = next;
                path.erase(path.begin());
                resources--;
            } else {
                path.clear(); // Invalid path, clear it
            }
        }
    }
    
    virtual void performTask(vector<vector<Cell>>& grid, vector<shared_ptr<Task>>& tasks) {
        if (!busy || !currentTask) return;
        
        if (position == currentTask->position) {
            stepsToComplete--;
            if (stepsToComplete <= 0) {
                completeTask(grid);
                currentTask->completed = true;
                busy = false;
                currentTask = nullptr;
                
                // Remove task from list
                tasks.erase(remove_if(tasks.begin(), tasks.end(), 
                    [](const shared_ptr<Task>& t) { return t->completed; }), tasks.end());
            }
        }
    }
    
    virtual void completeTask(vector<vector<Cell>>& grid) = 0;
    virtual double calculateBid(shared_ptr<Task> task) = 0;
    virtual bool hasResources() const { return resources > 20; }
    
    void refill() { resources = 100; }
    
    string getStatus() const {
        string status = getTypeName() + to_string(id) + "(" + 
                       to_string(position.x) + "," + to_string(position.y) + ")";
        status += busy ? "->" + currentTask->getTypeName() : "[IDLE]";
        status += " R:" + to_string(resources);
        return status;
    }
};

int Agent::nextId = 1;

// ==================== SPECIFIC AGENT CLASSES ====================
class RescueTeam : public Agent {
public:
    RescueTeam(Position pos) : Agent(AgentType::RESCUE_TEAM, pos) {}
    
    double calculateBid(shared_ptr<Task> task) override {
        if (!hasResources()) return 9999;
        if (task->type == TaskType::RESCUE_VICTIM || task->type == TaskType::CLEAR_DEBRIS) {
            return task->calculateCost(position, type);
        }
        return 9999;
    }
    
    void completeTask(vector<vector<Cell>>& grid) override {
        Cell& cell = grid[position.x][position.y];
        if (currentTask->type == TaskType::RESCUE_VICTIM && cell.hasVictim) {
            cell.victimRescued = true;
        } else if (currentTask->type == TaskType::CLEAR_DEBRIS && cell.type == CellType::DEBRIS) {
            cell.type = CellType::EMPTY;
        }
    }
};

class FireBrigade : public Agent {
public:
    FireBrigade(Position pos) : Agent(AgentType::FIRE_BRIGADE, pos) {}
    
    double calculateBid(shared_ptr<Task> task) override {
        if (!hasResources()) return 9999;
        if (task->type == TaskType::EXTINGUISH_FIRE) {
            return task->calculateCost(position, type);
        }
        return 9999;
    }
    
    void completeTask(vector<vector<Cell>>& grid) override {
        Cell& cell = grid[position.x][position.y];
        if (cell.type == CellType::FIRE) {
            cell.fireIntensity = max(0, cell.fireIntensity - 60);
            if (cell.fireIntensity == 0) {
                cell.type = CellType::BUILDING;
            }
        }
    }
};

class Ambulance : public Agent {
private:
    bool hasPatient;
public:
    Ambulance(Position pos) : Agent(AgentType::AMBULANCE, pos), hasPatient(false) {}
    
    double calculateBid(shared_ptr<Task> task) override {
        if (!hasResources()) return 9999;
        if (task->type == TaskType::TRANSPORT_TO_HOSPITAL && !hasPatient) {
            return task->calculateCost(position, type);
        }
        return 9999;
    }
    
    void completeTask(vector<vector<Cell>>& grid) override {
        Cell& cell = grid[position.x][position.y];
        if (!hasPatient) {
            // Pick up victim
            if (cell.hasVictim && cell.victimRescued) {
                cell.hasVictim = false;
                cell.victimRescued = false;
                hasPatient = true;
                // Find nearest hospital for next task
            }
        } else {
            // Drop off at hospital
            if (cell.type == CellType::HOSPITAL) {
                hasPatient = false;
            }
        }
    }
};

// ==================== SIMULATION CONTROLLER ====================
class Simulation {
private:
    vector<vector<Cell>> grid;
    vector<shared_ptr<Agent>> agents;
    vector<shared_ptr<Task>> tasks;
    int stepCount;
    random_device rd;
    mt19937 gen;
    
    // Statistics
    int victimsSaved;
    int firesExtinguished;
    int debrisCleared;
    
public:
    Simulation(int size = GRID_SIZE) : grid(size, vector<Cell>(size)), stepCount(0), 
                                      victimsSaved(0), firesExtinguished(0), debrisCleared(0), gen(rd()) {
        initializeGrid();
        initializeAgents();
    }
    
    void initializeGrid() {
        // Initialize grid with cells
        for (int x = 0; x < GRID_SIZE; x++) {
            for (int y = 0; y < GRID_SIZE; y++) {
                grid[x][y] = Cell(x, y);
            }
        }
        
        // Create city layout
        createCityLayout();
    }
    
    void createCityLayout() {
        // Add hospitals
        grid[2][2].type = CellType::HOSPITAL;
        grid[GRID_SIZE-3][GRID_SIZE-3].type = CellType::HOSPITAL;
        grid[GRID_SIZE-3][2].type = CellType::HOSPITAL;
        grid[2][GRID_SIZE-3].type = CellType::HOSPITAL;
        
        // Create buildings in blocks
        for (int x = 5; x < GRID_SIZE-5; x += 4) {
            for (int y = 5; y < GRID_SIZE-5; y += 4) {
                if ((x + y) % 8 == 0) continue; // Leave some empty space
                
                // Create a 2x2 building block
                for (int dx = 0; dx < 2; dx++) {
                    for (int dy = 0; dy < 2; dy++) {
                        if (x+dx < GRID_SIZE && y+dy < GRID_SIZE) {
                            grid[x+dx][y+dy].type = CellType::BUILDING;
                            
                            // Randomly set some buildings on fire
                            if ((x * y) % 7 == 0) {
                                grid[x+dx][y+dy].type = CellType::FIRE;
                                grid[x+dx][y+dy].fireIntensity = 30 + (rand() % 50);
                            }
                            
                            // Add victims to some buildings
                            if ((x * y) % 5 == 0) {
                                grid[x+dx][y+dy].hasVictim = true;
                                grid[x+dx][y+dy].victimHealth = 30 + (rand() % 60);
                            }
                        }
                    }
                }
            }
        }
        
        // Add some debris on roads
        for (int i = 0; i < 15; i++) {
            int x = rand() % GRID_SIZE;
            int y = rand() % GRID_SIZE;
            if (grid[x][y].type == CellType::EMPTY) {
                grid[x][y].type = CellType::DEBRIS;
            }
        }
        
        // Add water sources
        for (int i = 0; i < 4; i++) {
            int x = rand() % GRID_SIZE;
            int y = rand() % GRID_SIZE;
            if (grid[x][y].type == CellType::EMPTY) {
                grid[x][y].type = CellType::WATER;
            }
        }
    }
    
    void initializeAgents() {
        // Create diverse agents at strategic positions
        agents.push_back(make_shared<RescueTeam>(Position(1, 1)));
        agents.push_back(make_shared<RescueTeam>(Position(GRID_SIZE-2, 1)));
        agents.push_back(make_shared<RescueTeam>(Position(1, GRID_SIZE-2)));
        agents.push_back(make_shared<RescueTeam>(Position(GRID_SIZE-2, GRID_SIZE-2)));
        
        agents.push_back(make_shared<FireBrigade>(Position(GRID_SIZE/2, 1)));
        agents.push_back(make_shared<FireBrigade>(Position(1, GRID_SIZE/2)));
        agents.push_back(make_shared<FireBrigade>(Position(GRID_SIZE-2, GRID_SIZE/2)));
        
        agents.push_back(make_shared<Ambulance>(Position(GRID_SIZE/2, GRID_SIZE-2)));
        agents.push_back(make_shared<Ambulance>(Position(GRID_SIZE/2, GRID_SIZE/2)));
    }
    
    void spreadFire() {
        vector<Position> newFires;
        
        for (int x = 0; x < GRID_SIZE; x++) {
            for (int y = 0; y < GRID_SIZE; y++) {
                if (grid[x][y].type == CellType::FIRE && grid[x][y].fireIntensity > 40) {
                    // Chance to spread to adjacent cells
                    for (int dx = -1; dx <= 1; dx++) {
                        for (int dy = -1; dy <= 1; dy++) {
                            if (dx == 0 && dy == 0) continue;
                            
                            int nx = x + dx, ny = y + dy;
                            if (nx >= 0 && nx < GRID_SIZE && ny >= 0 && ny < GRID_SIZE) {
                                if ((grid[nx][ny].type == CellType::BUILDING || 
                                     grid[nx][ny].type == CellType::EMPTY) &&
                                    uniform_real_distribution<>(0,1)(gen) < 0.2) {
                                    newFires.push_back(Position(nx, ny));
                                }
                            }
                        }
                    }
                }
            }
        }
        
        // Apply new fires
        for (const auto& pos : newFires) {
            if (grid[pos.x][pos.y].type != CellType::HOSPITAL && 
                grid[pos.x][pos.y].type != CellType::WATER) {
                grid[pos.x][pos.y].type = CellType::FIRE;
                grid[pos.x][pos.y].fireIntensity = 20;
            }
        }
    }
    
    void updateTasks() {
        // Remove completed tasks
        tasks.erase(remove_if(tasks.begin(), tasks.end(), 
            [](const shared_ptr<Task>& t) { return t->completed; }), tasks.end());
        
        // Scan for new tasks
        for (int x = 0; x < GRID_SIZE; x++) {
            for (int y = 0; y < GRID_SIZE; y++) {
                Cell& cell = grid[x][y];
                Position pos(x, y);
                
                // Fire tasks
                if (cell.type == CellType::FIRE && cell.fireIntensity > 10) {
                    bool exists = false;
                    for (auto& task : tasks) {
                        if (task->position == pos && task->type == TaskType::EXTINGUISH_FIRE) {
                            exists = true; break;
                        }
                    }
                    if (!exists) {
                        tasks.push_back(make_shared<Task>(TaskType::EXTINGUISH_FIRE, pos, cell.fireIntensity));
                    }
                }
                
                // Victim rescue tasks
                if (cell.hasVictim && !cell.victimRescued) {
                    bool exists = false;
                    for (auto& task : tasks) {
                        if (task->position == pos && task->type == TaskType::RESCUE_VICTIM) {
                            exists = true; break;
                        }
                    }
                    if (!exists) {
                        tasks.push_back(make_shared<Task>(TaskType::RESCUE_VICTIM, pos, 100 - cell.victimHealth));
                    }
                }
                
                // Debris clearance tasks
                if (cell.type == CellType::DEBRIS) {
                    bool exists = false;
                    for (auto& task : tasks) {
                        if (task->position == pos && task->type == TaskType::CLEAR_DEBRIS) {
                            exists = true; break;
                        }
                    }
                    if (!exists) {
                        tasks.push_back(make_shared<Task>(TaskType::CLEAR_DEBRIS, pos, 50));
                    }
                }
            }
        }
    }
    
    void assignTasks() {
        // Contract Net Protocol
        for (auto& task : tasks) {
            if (task->completed || !task->assignedTo.empty()) continue;
            
            shared_ptr<Agent> bestAgent = nullptr;
            double bestBid = 9999;
            
            for (auto& agent : agents) {
                if (!agent->isBusy() && agent->hasResources()) {
                    double bid = agent->calculateBid(task);
                    if (bid < bestBid) {
                        bestBid = bid;
                        bestAgent = agent;
                    }
                }
            }
            
            if (bestAgent && bestBid < 50) { // Reasonable distance
                bestAgent->assignTask(task);
                
                // Simple direct path (replace with A* for complex maps)
                vector<Position> path;
                Position current = bestAgent->getPosition();
                Position target = task->position;
                
                // Simple path: move horizontally then vertically
                int dx = target.x > current.x ? 1 : -1;
                int dy = target.y > current.y ? 1 : -1;
                
                for (int x = current.x; x != target.x; x += dx) {
                    if (x >= 0 && x < GRID_SIZE) path.push_back(Position(x, current.y));
                }
                for (int y = current.y; y != target.y; y += dy) {
                    if (y >= 0 && y < GRID_SIZE) path.push_back(Position(target.x, y));
                }
                path.push_back(target);
                
                bestAgent->setPath(path);
            }
        }
    }
    
    void update() {
        stepCount++;
        
        // Update environment
        for (int x = 0; x < GRID_SIZE; x++) {
            for (int y = 0; y < GRID_SIZE; y++) {
                grid[x][y].update();
            }
        }
        
        // Spread fire
        if (stepCount % 3 == 0) {
            spreadFire();
        }
        
        // Update tasks
        updateTasks();
        
        // Assign tasks
        assignTasks();
        
        // Update agents
        for (auto& agent : agents) {
            agent->move(grid);
            agent->performTask(grid, tasks);
            
            // Random resource refill
            if (!agent->isBusy() && (rand() % 20 == 0)) {
                agent->refill();
            }
        }
        
        // Update statistics
        updateStatistics();
    }
    
    void updateStatistics() {
        victimsSaved = 0;
        firesExtinguished = 0;
        debrisCleared = 0;
        
        for (int x = 0; x < GRID_SIZE; x++) {
            for (int y = 0; y < GRID_SIZE; y++) {
                if (grid[x][y].hasVictim && grid[x][y].victimRescued) victimsSaved++;
                if (grid[x][y].type == CellType::BUILDING && grid[x][y].fireIntensity == 0) firesExtinguished++;
                if (grid[x][y].type == CellType::EMPTY) debrisCleared++; // Simplified
            }
        }
    }
    
    void display() {
        ConsoleUtils::clearScreen();
        
        // Header
        ConsoleUtils::setColor(COLORS[15]);
        cout << "🚨 DISASTER RESPONSE SIMULATION 🚨 - Step: " << stepCount << endl;
        cout << "==============================================" << endl;
        
        // Display grid with border
        cout << "CITY MAP:" << endl;
        cout << "┌";
        for (int x = 0; x < GRID_SIZE; x++) cout << "──";
        cout << "┐" << endl;
        
        for (int y = 0; y < GRID_SIZE; y++) {
            cout << "│";
            for (int x = 0; x < GRID_SIZE; x++) {
                bool agentHere = false;
                for (const auto& agent : agents) {
                    if (agent->getPosition().x == x && agent->getPosition().y == y) {
                        ConsoleUtils::setColor(agent->getColor());
                        cout << agent->getChar() << " ";
                        agentHere = true;
                        break;
                    }
                }
                
                if (!agentHere) {
                    ConsoleUtils::setColor(grid[x][y].getColor());
                    cout << grid[x][y].getChar() << " ";
                }
            }
            ConsoleUtils::setColor(COLORS[15]);
            cout << "│" << endl;
        }
        
        cout << "└";
        for (int x = 0; x < GRID_SIZE; x++) cout << "──";
        cout << "┘" << endl;
        
        // Legend
        ConsoleUtils::setColor(COLORS[15]);
        cout << "\nLEGEND: ";
        ConsoleUtils::setColor(COLORS[14]); cout << "R"; ConsoleUtils::setColor(COLORS[15]); cout << "=Rescue ";
        ConsoleUtils::setColor(COLORS[12]); cout << "F"; ConsoleUtils::setColor(COLORS[15]); cout << "=Fire ";
        ConsoleUtils::setColor(COLORS[11]); cout << "A"; ConsoleUtils::setColor(COLORS[15]); cout << "=Ambulance ";
        ConsoleUtils::setColor(COLORS[13]); cout << "V"; ConsoleUtils::setColor(COLORS[15]); cout << "=Victim ";
        ConsoleUtils::setColor(COLORS[4]); cout << "F/f"; ConsoleUtils::setColor(COLORS[15]); cout << "=Fire ";
        ConsoleUtils::setColor(COLORS[10]); cout << "H"; ConsoleUtils::setColor(COLORS[15]); cout << "=Hospital ";
        ConsoleUtils::setColor(COLORS[6]); cout << "D"; ConsoleUtils::setColor(COLORS[15]); cout << "=Debris" << endl;
        
        // Agent Status
        ConsoleUtils::setColor(COLORS[15]);
        cout << "\nAGENT STATUS:" << endl;
        for (const auto& agent : agents) {
            ConsoleUtils::setColor(agent->getColor());
            cout << "  " << agent->getStatus() << endl;
        }
        
        // Tasks
        ConsoleUtils::setColor(COLORS[15]);
        cout << "\nACTIVE TASKS: " << tasks.size() << endl;
        for (const auto& task : tasks) {
            if (!task->completed) {
                ConsoleUtils::setColor(COLORS[12]);
                cout << "  " << task->getTypeName() << " @(" << task->position.x << "," << task->position.y 
                     << ") Urg:" << task->urgency;
                if (!task->assignedTo.empty()) {
                    ConsoleUtils::setColor(COLORS[14]);
                    cout << " [ASSIGNED: " << task->assignedTo << "]";
                }
                cout << endl;
            }
        }
        
        // Statistics
        ConsoleUtils::setColor(COLORS[15]);
        cout << "\n📊 STATISTICS:" << endl;
        ConsoleUtils::setColor(COLORS[10]); cout << "  Victims Saved: " << victimsSaved << endl;
        ConsoleUtils::setColor(COLORS[12]); cout << "  Fires Extinguished: " << firesExtinguished << endl;
        ConsoleUtils::setColor(COLORS[14]); cout << "  Debris Cleared: " << debrisCleared << endl;
        
        int activeFires = 0, waitingVictims = 0;
        for (int x = 0; x < GRID_SIZE; x++) {
            for (int y = 0; y < GRID_SIZE; y++) {
                if (grid[x][y].type == CellType::FIRE) activeFires++;
                if (grid[x][y].hasVictim && !grid[x][y].victimRescued) waitingVictims++;
            }
        }
        
        ConsoleUtils::setColor(activeFires > 5 ? COLORS[12] : COLORS[10]);
        cout << "  Active Fires: " << activeFires << endl;
        ConsoleUtils::setColor(waitingVictims > 3 ? COLORS[13] : COLORS[10]);
        cout << "  Waiting Victims: " << waitingVictims << endl;
        
        ConsoleUtils::setColor(COLORS[15]);
        cout << "\nPress Ctrl+C to stop | Next step in 1 second..." << endl;
    }
    
    void run() {
        while (true) {
            display();
            update();
            ConsoleUtils::sleep(1000); // 1 second between steps
        }
    }
};

// ==================== MAIN FUNCTION ====================
int main() {
    // Set console window size
    system("mode con: cols=80 lines=50");
    
    ConsoleUtils::setColor(COLORS[15]);
    cout << "🚨 DISASTER RESPONSE SIMULATION 🚨" << endl;
    cout << "==============================================" << endl;
    cout << "Starting simulation..." << endl;
    cout << "City Size: " << GRID_SIZE << "x" << GRID_SIZE << endl;
    cout << "Agents: 4 Rescue, 3 Fire, 2 Ambulance" << endl;
    cout << "Initializing disaster scenario..." << endl;
    
    ConsoleUtils::sleep(2000);
    
    try {
        Simulation simulation;
        simulation.run();
    } catch (const exception& e) {
        ConsoleUtils::setColor(COLORS[12]);
        cerr << "Simulation Error: " << e.what() << endl;
        ConsoleUtils::setColor(COLORS[15]);
        cout << "Press Enter to exit..." << endl;
        cin.get();
        return 1;
    }
    
    return 0;
}