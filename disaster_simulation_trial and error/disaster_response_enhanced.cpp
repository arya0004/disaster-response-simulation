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
#include <set>
#include <iomanip>
#include <ctime>

using namespace std;

// ==================== CONSTANTS AND ENUMS ====================
const int GRID_SIZE = 25;
const int WINDOW_WIDTH = 90;
const int WINDOW_HEIGHT = 45;

// Console Colors
enum ConsoleColor {
    BLACK = 0, BLUE = 1, GREEN = 2, CYAN = 3, RED = 4, MAGENTA = 5, YELLOW = 6, WHITE = 7,
    BRIGHT_BLACK = 8, BRIGHT_BLUE = 9, BRIGHT_GREEN = 10, BRIGHT_CYAN = 11,
    BRIGHT_RED = 12, BRIGHT_MAGENTA = 13, BRIGHT_YELLOW = 14, BRIGHT_WHITE = 15
};

enum class CellType { EMPTY, BUILDING, FIRE, VICTIM, HOSPITAL, DEBRIS, ROAD };
enum class AgentType { RESCUE_TEAM, FIRE_BRIGADE, AMBULANCE };
enum class TaskType { EXTINGUISH_FIRE, RESCUE_VICTIM, TRANSPORT_TO_HOSPITAL, CLEAR_DEBRIS };
enum class AgentState { IDLE, MOVING, WORKING, RETURNING };

// ==================== UTILITY CLASSES ====================
class Position {
public:
    int x, y;
    Position(int x = 0, int y = 0) : x(x), y(y) {}
    
    bool operator==(const Position& other) const { return x == other.x && y == other.y; }
    bool operator<(const Position& other) const { return x < other.x || (x == other.x && y < other.y); }
    
    double distanceTo(const Position& other) const {
        return sqrt(pow(x - other.x, 2) + pow(y - other.y, 2));
    }
    
    vector<Position> getNeighbors() const {
        return {{x-1,y}, {x+1,y}, {x,y-1}, {x,y+1}};
    }
};

class ConsoleUtils {
public:
    static void setColor(int color) {
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
    }
    
    static void clearScreen() { system("cls"); }
    static void sleep(int ms) { Sleep(ms); }
    
    static void setCursor(int x, int y) {
        COORD coord = {static_cast<SHORT>(x), static_cast<SHORT>(y)};
        SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
    }
    
    static void setWindowSize(int width, int height) {
        string command = "mode con: cols=" + to_string(width) + " lines=" + to_string(height);
        system(command.c_str());
    }
};

// ==================== USER CONFIGURATION ====================
struct SimulationConfig {
    int numFires;
    int numVictims;
    int numRescueTeams;
    int numFireBrigades;
    int numAmbulances;
    int citySize;
    
    SimulationConfig() 
        : numFires(8), numVictims(15), numRescueTeams(4), 
          numFireBrigades(6), numAmbulances(4), citySize(GRID_SIZE) {}
};

class UserInterface {
public:
    static SimulationConfig getConfiguration() {
        SimulationConfig config;
        ConsoleUtils::clearScreen();
        
        ConsoleUtils::setColor(BRIGHT_CYAN);
        cout << "╔══════════════════════════════════════════════════════════════╗\n";
        cout << "║                 DISASTER RESPONSE SIMULATION                ║\n";
        cout << "║                    CONFIGURATION MENU                       ║\n";
        cout << "╚══════════════════════════════════════════════════════════════╝\n\n";
        
        ConsoleUtils::setColor(BRIGHT_WHITE);
        cout << "Configure the disaster scenario:\n\n";
        
        config.numFires = getNumberInput("Number of buildings on fire (5-20): ", 5, 20);
        config.numVictims = getNumberInput("Number of victims (10-30): ", 10, 30);
        config.numRescueTeams = getNumberInput("Number of rescue teams (2-8): ", 2, 8);
        config.numFireBrigades = getNumberInput("Number of fire brigades (3-10): ", 3, 10);
        config.numAmbulances = getNumberInput("Number of ambulances (2-8): ", 2, 8);
        
        ConsoleUtils::setColor(BRIGHT_GREEN);
        cout << "\n✅ Configuration complete! Starting simulation...\n";
        ConsoleUtils::sleep(2000);
        
        return config;
    }
    
private:
    static int getNumberInput(const string& prompt, int minVal, int maxVal) {
        int value;
        while (true) {
            ConsoleUtils::setColor(BRIGHT_YELLOW);
            cout << prompt;
            ConsoleUtils::setColor(WHITE);
            
            if (cin >> value) {
                if (value >= minVal && value <= maxVal) {
                    break;
                }
            }
            ConsoleUtils::setColor(BRIGHT_RED);
            cout << "❌ Please enter a number between " << minVal << " and " << maxVal << "!\n";
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
        }
        return value;
    }
};

// ==================== TASK CLASS ====================
class Task {
public:
    TaskType type;
    Position position;
    int urgency;        // 0-100, higher = more urgent
    int priority;       // Calculated priority
    string id;
    bool assigned;
    string assignedAgent;
    int baseCost;
    
    Task(TaskType t, Position pos, int urg, string taskId) 
        : type(t), position(pos), urgency(urg), assigned(false), baseCost(0) {
        id = taskId;
        priority = calculatePriority();
    }
    
    int calculatePriority() {
        switch(type) {
            case TaskType::EXTINGUISH_FIRE: return urgency * 3;
            case TaskType::RESCUE_VICTIM: return urgency * 2;
            case TaskType::TRANSPORT_TO_HOSPITAL: return urgency * 2;
            case TaskType::CLEAR_DEBRIS: return urgency;
            default: return urgency;
        }
    }
    
    double calculateCost(const Position& agentPos, AgentType agentType) const {
        double distance = position.distanceTo(agentPos);
        double urgencyFactor = (100 - urgency) * 0.1;
        
        // Agent specialization multipliers
        double efficiency = 1.0;
        switch(agentType) {
            case AgentType::FIRE_BRIGADE:
                efficiency = (type == TaskType::EXTINGUISH_FIRE) ? 0.2 : 1.8;
                break;
            case AgentType::RESCUE_TEAM:
                efficiency = (type == TaskType::RESCUE_VICTIM) ? 0.3 : 
                            (type == TaskType::CLEAR_DEBRIS) ? 0.4 : 1.6;
                break;
            case AgentType::AMBULANCE:
                efficiency = (type == TaskType::TRANSPORT_TO_HOSPITAL) ? 0.2 : 1.7;
                break;
        }
        
        return (distance * 0.6) + (urgencyFactor * 0.2) + (efficiency * 0.2);
    }
    
    string getTypeName() const {
        switch(type) {
            case TaskType::EXTINGUISH_FIRE: return "FIRE_EXTINGUISH";
            case TaskType::RESCUE_VICTIM: return "VICTIM_RESCUE";
            case TaskType::TRANSPORT_TO_HOSPITAL: return "HOSPITAL_TRANSPORT";
            case TaskType::CLEAR_DEBRIS: return "DEBRIS_CLEAR";
            default: return "UNKNOWN";
        }
    }
    
    string getDisplayName() const {
        switch(type) {
            case TaskType::EXTINGUISH_FIRE: return "🔥 FIRE";
            case TaskType::RESCUE_VICTIM: return "❤️ VICTIM";
            case TaskType::TRANSPORT_TO_HOSPITAL: return "🚑 TRANSPORT";
            case TaskType::CLEAR_DEBRIS: return "🚧 DEBRIS";
            default: return "❓ UNKNOWN";
        }
    }
};

// ==================== CELL CLASS ====================
class Cell {
public:
    Position pos;
    CellType type;
    int fireIntensity;  // 0-100
    int victimHealth;   // 0-100
    bool hasVictim;
    bool victimRescued;
    int smokeLevel;     // Visual effect
    
    Cell() : pos(0,0), type(CellType::EMPTY), fireIntensity(0), victimHealth(100),
            hasVictim(false), victimRescued(false), smokeLevel(0) {}
    
    Cell(int x, int y) : pos(x,y), type(CellType::EMPTY), fireIntensity(0), victimHealth(100),
                        hasVictim(false), victimRescued(false), smokeLevel(0) {}
    
    void update() {
        // Fire dynamics - SLOWER fire increase
        if (type == CellType::FIRE) {
            fireIntensity = min(100, fireIntensity + 1); // Reduced from 2 to 1
            smokeLevel = min(3, smokeLevel + 1);
            
            // Victims in fire take more damage
            if (hasVictim && !victimRescued) {
                victimHealth = max(0, victimHealth - 5); // Reduced from 8 to 5
            }
        } else if (hasVictim && !victimRescued) {
            // Normal health deterioration - SLOWER
            victimHealth = max(0, victimHealth - 1);
        }
        
        // Smoke dissipation
        if (smokeLevel > 0 && type != CellType::FIRE) {
            smokeLevel--;
        }
    }
    
    bool isTraversable() const {
        return type != CellType::DEBRIS && fireIntensity < 60; // Increased threshold
    }
    
    bool isAccessible() const {
        return type != CellType::DEBRIS && fireIntensity < 80; // Increased threshold
    }
    
    char getChar() const {
        if (hasVictim && !victimRescued) {
            if (victimHealth > 70) return 'V';
            if (victimHealth > 30) return 'v';
            return '!'; // Critical victim
        }
        
        switch(type) {
            case CellType::EMPTY: 
                if (smokeLevel > 0) return '~';
                return ' ';
            case CellType::BUILDING: return 'B';
            case CellType::FIRE: 
                if (fireIntensity > 75) return 'F';
                if (fireIntensity > 50) return 'f';
                return 'w'; // weak fire
            case CellType::HOSPITAL: return 'H';
            case CellType::DEBRIS: return 'D';
            case CellType::ROAD: return '.';
            default: return '?';
        }
    }
    
    int getColor() const {
        if (hasVictim && !victimRescued) {
            if (victimHealth > 70) return BRIGHT_MAGENTA;
            if (victimHealth > 30) return MAGENTA;
            return BRIGHT_RED; // Critical
        }
        
        switch(type) {
            case CellType::EMPTY: 
                if (smokeLevel > 0) return BRIGHT_BLACK;
                return BLACK;
            case CellType::BUILDING: return BLUE;
            case CellType::FIRE: 
                if (fireIntensity > 75) return BRIGHT_RED;
                if (fireIntensity > 50) return RED;
                return YELLOW;
            case CellType::HOSPITAL: return BRIGHT_GREEN;
            case CellType::DEBRIS: return YELLOW;
            case CellType::ROAD: return BRIGHT_BLACK;
            default: return WHITE;
        }
    }
};

// ==================== PATHFINDING - A* ALGORITHM ====================
class AStar {
private:
    struct Node {
        Position pos;
        double f, g, h;
        shared_ptr<Node> parent;
        
        Node(Position p, double g, double h, shared_ptr<Node> pnt = nullptr)
            : pos(p), f(g + h), g(g), h(h), parent(pnt) {}
        
        bool operator>(const Node& other) const { return f > other.f; }
    };

public:
    static vector<Position> findPath(const Position& start, const Position& goal, 
                                   const vector<vector<Cell>>& grid) {
        vector<Position> path;
        if (!isValid(goal, grid) || !grid[goal.x][goal.y].isTraversable()) {
            return path;
        }
        
        auto cmp = [](const shared_ptr<Node>& a, const shared_ptr<Node>& b) { 
            return a->f > b->f; 
        };
        priority_queue<shared_ptr<Node>, vector<shared_ptr<Node>>, decltype(cmp)> openList(cmp);
        map<Position, bool> closedList;
        
        openList.push(make_shared<Node>(start, 0, heuristic(start, goal)));
        
        while (!openList.empty()) {
            auto current = openList.top();
            openList.pop();
            
            if (current->pos == goal) {
                // Reconstruct path
                while (current) {
                    path.push_back(current->pos);
                    current = current->parent;
                }
                reverse(path.begin(), path.end());
                return path;
            }
            
            closedList[current->pos] = true;
            
            for (const auto& neighborPos : current->pos.getNeighbors()) {
                if (!isValid(neighborPos, grid) || closedList[neighborPos] || 
                    !grid[neighborPos.x][neighborPos.y].isTraversable()) {
                    continue;
                }
                
                double g = current->g + 1.0;
                double h = heuristic(neighborPos, goal);
                auto neighborNode = make_shared<Node>(neighborPos, g, h, current);
                
                openList.push(neighborNode);
            }
        }
        
        return path;
    }

private:
    static bool isValid(const Position& pos, const vector<vector<Cell>>& grid) {
        return pos.x >= 0 && pos.x < grid.size() && pos.y >= 0 && pos.y < grid[0].size();
    }
    
    static double heuristic(const Position& a, const Position& b) {
        return abs(a.x - b.x) + abs(a.y - b.y);
    }
};

// ==================== AGENT BASE CLASS ====================
class Agent {
protected:
    static int nextId;
    int id;
    AgentType type;
    Position position;
    Position basePosition;
    vector<Position> path;
    AgentState state;
    
    int fuel;
    int water;
    int medicalKits;
    int efficiency;
    
    shared_ptr<Task> currentTask;
    int workProgress;
    int workRequired;
    
    // Reference to environment grid
    vector<vector<Cell>>* environmentGrid;
    
public:
    Agent(AgentType t, Position pos, vector<vector<Cell>>& grid) 
        : id(nextId++), type(t), position(pos), basePosition(pos), state(AgentState::IDLE),
          fuel(200), water(200), medicalKits(8), efficiency(100), workProgress(0), workRequired(2), // Increased resources
          environmentGrid(&grid) {}
    
    virtual ~Agent() = default;
    
    // Getters
    int getId() const { return id; }
    AgentType getType() const { return type; }
    Position getPosition() const { return position; }
    AgentState getState() const { return state; }
    bool isBusy() const { return state != AgentState::IDLE; }
    shared_ptr<Task> getCurrentTask() const { return currentTask; }
    
    string getTypeName() const {
        switch(type) {
            case AgentType::RESCUE_TEAM: return "RESCUE";
            case AgentType::FIRE_BRIGADE: return "FIRE";
            case AgentType::AMBULANCE: return "AMBUL";
            default: return "UNKNOWN";
        }
    }
    
    string getStateName() const {
        switch(state) {
            case AgentState::IDLE: return "IDLE";
            case AgentState::MOVING: return "MOVING";
            case AgentState::WORKING: return "WORKING";
            case AgentState::RETURNING: return "RETURNING";
            default: return "UNKNOWN";
        }
    }
    
    char getDisplayChar() const {
        switch(type) {
            case AgentType::RESCUE_TEAM: return 'R';
            case AgentType::FIRE_BRIGADE: return 'W';
            case AgentType::AMBULANCE: return 'A';
            default: return '?';
        }
    }
    
    int getColor() const {
        switch(type) {
            case AgentType::RESCUE_TEAM: return BRIGHT_YELLOW;
            case AgentType::FIRE_BRIGADE: return BRIGHT_RED;
            case AgentType::AMBULANCE: return BRIGHT_CYAN;
            default: return WHITE;
        }
    }
    
    // Resource management - MORE FORGIVING
    bool hasSufficientResources() const {
        if (fuel < 50) return false; // Increased threshold
        switch(type) {
            case AgentType::FIRE_BRIGADE: return water > 20; // Increased threshold
            case AgentType::AMBULANCE: return medicalKits > 0;
            case AgentType::RESCUE_TEAM: return true;
            default: return true;
        }
    }
    
    void consumeResources() {
        fuel = max(0, fuel - 1);
        if (type == AgentType::FIRE_BRIGADE && state == AgentState::WORKING) {
            water = max(0, water - 8); // Reduced consumption
        }
    }
    
    void refillResources() {
        fuel = 200;
        if (type == AgentType::FIRE_BRIGADE) water = 200;
        if (type == AgentType::AMBULANCE) medicalKits = min(8, medicalKits + 3);
    }
    
    // Task management
    virtual double calculateBid(shared_ptr<Task> task) = 0;
    
    void assignTask(shared_ptr<Task> task) {
        currentTask = task;
        task->assigned = true;
        task->assignedAgent = getTypeName() + to_string(id);
        state = AgentState::MOVING;
        
        // Calculate path to task
        path = AStar::findPath(position, task->position, *environmentGrid);
        if (path.empty()) {
            state = AgentState::IDLE;
            currentTask = nullptr;
            task->assigned = false;
            task->assignedAgent = "";
        }
    }
    
    void move() {
        if (path.empty()) {
            if (state == AgentState::MOVING && currentTask && position == currentTask->position) {
                state = AgentState::WORKING;
                workProgress = 0;
            } else if (state == AgentState::RETURNING && position == basePosition) {
                state = AgentState::IDLE;
                refillResources();
            }
            return;
        }
        
        Position nextPos = path.front();
        path.erase(path.begin());
        
        if (isValidMove(nextPos)) {
            position = nextPos;
            consumeResources();
        }
    }
    
    virtual void performTask() = 0;
    
    void update() {
        if (state == AgentState::MOVING) {
            move();
        } else if (state == AgentState::WORKING) {
            performTask();
            workProgress++;
            
            if (workProgress >= workRequired) {
                completeTask();
            }
        }
    }
    
    void completeTask() {
        if (currentTask) {
            currentTask->assigned = false;
            currentTask->assignedAgent = "";
        }
        
        // Return to base if low on resources
        if (!hasSufficientResources()) {
            state = AgentState::RETURNING;
            path = AStar::findPath(position, basePosition, *environmentGrid);
        } else {
            state = AgentState::IDLE;
        }
        
        currentTask = nullptr;
        workProgress = 0;
    }
    
    string getStatus() const {
        string status = getTypeName() + to_string(id) + " @(" + 
                       to_string(position.x) + "," + to_string(position.y) + ") ";
        status += "[" + getStateName() + "] ";
        
        if (currentTask) {
            status += "-> " + currentTask->getDisplayName();
        }
        
        status += " F:" + to_string(fuel);
        if (type == AgentType::FIRE_BRIGADE) status += " W:" + to_string(water);
        if (type == AgentType::AMBULANCE) status += " M:" + to_string(medicalKits);
        
        return status;
    }

protected:
    bool isValidMove(const Position& pos) {
        if (pos.x < 0 || pos.x >= environmentGrid->size() || 
            pos.y < 0 || pos.y >= (*environmentGrid)[0].size()) {
            return false;
        }
        return (*environmentGrid)[pos.x][pos.y].isTraversable();
    }
};

int Agent::nextId = 1;

// ==================== SPECIFIC AGENT CLASSES ====================
class RescueTeam : public Agent {
public:
    RescueTeam(Position pos, vector<vector<Cell>>& grid) 
        : Agent(AgentType::RESCUE_TEAM, pos, grid) {}
    
    double calculateBid(shared_ptr<Task> task) override {
        if (!hasSufficientResources()) return 9999.0;
        
        switch(task->type) {
            case TaskType::RESCUE_VICTIM:
            case TaskType::CLEAR_DEBRIS:
                return task->calculateCost(position, type);
            default:
                return 9999.0;
        }
    }
    
    void performTask() override {
        if (!currentTask) return;
        
        Cell& cell = (*environmentGrid)[position.x][position.y];
        
        switch(currentTask->type) {
            case TaskType::RESCUE_VICTIM:
                if (cell.hasVictim && !cell.victimRescued) {
                    cell.victimRescued = true;
                    medicalKits = max(0, medicalKits - 1);
                }
                break;
                
            case TaskType::CLEAR_DEBRIS:
                if (cell.type == CellType::DEBRIS) {
                    cell.type = CellType::ROAD;
                    fuel = max(0, fuel - 3); // Reduced consumption
                }
                break;
        }
    }
};

class FireBrigade : public Agent {
public:
    FireBrigade(Position pos, vector<vector<Cell>>& grid) 
        : Agent(AgentType::FIRE_BRIGADE, pos, grid) {}
    
    double calculateBid(shared_ptr<Task> task) override {
        if (!hasSufficientResources()) return 9999.0;
        
        switch(task->type) {
            case TaskType::EXTINGUISH_FIRE:
                return task->calculateCost(position, type);
            default:
                return 9999.0;
        }
    }
    
    void performTask() override {
        if (!currentTask) return;
        
        Cell& cell = (*environmentGrid)[position.x][position.y];
        
        if (cell.type == CellType::FIRE) {
            // MORE EFFICIENT FIRE EXTINGUISHING
            cell.fireIntensity = max(0, cell.fireIntensity - 40); // Increased from 25 to 40
            water = max(0, water - 8); // Reduced consumption
            
            if (cell.fireIntensity == 0) {
                cell.type = CellType::BUILDING;
                cell.smokeLevel = 2;
            }
        }
    }
};

class Ambulance : public Agent {
private:
    bool transportingVictim;
    Position hospitalTarget;
    
public:
    Ambulance(Position pos, vector<vector<Cell>>& grid) 
        : Agent(AgentType::AMBULANCE, pos, grid), transportingVictim(false) {}
    
    double calculateBid(shared_ptr<Task> task) override {
        if (!hasSufficientResources() || transportingVictim) return 9999.0;
        
        switch(task->type) {
            case TaskType::TRANSPORT_TO_HOSPITAL:
                return task->calculateCost(position, type);
            default:
                return 9999.0;
        }
    }
    
    void performTask() override {
        if (!currentTask) return;
        
        Cell& cell = (*environmentGrid)[position.x][position.y];
        
        if (!transportingVictim) {
            // Pick up victim
            if (cell.hasVictim && cell.victimRescued) {
                cell.hasVictim = false;
                cell.victimRescued = false;
                transportingVictim = true;
                medicalKits = max(0, medicalKits - 1);
                
                // Find nearest hospital
                hospitalTarget = findNearestHospital();
                if (hospitalTarget.x != -1) {
                    path = AStar::findPath(position, hospitalTarget, *environmentGrid);
                }
            }
        } else {
            // Deliver to hospital
            if (cell.type == CellType::HOSPITAL) {
                transportingVictim = false;
                hospitalTarget = Position(-1, -1);
            }
        }
    }
    
    void completeTask() {
        if (transportingVictim && position == hospitalTarget) {
            transportingVictim = false;
        }
        
        Agent::completeTask();
    }
    
    string getStatus() const {
        string status = Agent::getStatus();
        if (transportingVictim) {
            status += " [TRANSPORTING]";
        }
        return status;
    }

private:
    Position findNearestHospital() {
        Position nearest(-1, -1);
        double minDist = numeric_limits<double>::max();
        
        for (int x = 0; x < environmentGrid->size(); x++) {
            for (int y = 0; y < (*environmentGrid)[0].size(); y++) {
                if ((*environmentGrid)[x][y].type == CellType::HOSPITAL) {
                    double dist = position.distanceTo(Position(x, y));
                    if (dist < minDist) {
                        minDist = dist;
                        nearest = Position(x, y);
                    }
                }
            }
        }
        return nearest;
    }
};

// ==================== SIMULATION ENGINE ====================
class DisasterSimulation {
private:
    vector<vector<Cell>> grid;
    vector<shared_ptr<Agent>> agents;
    vector<shared_ptr<Task>> tasks;
    int stepCount;
    SimulationConfig config;
    
    // Statistics
    int totalVictims;
    int rescuedVictims;
    int deceasedVictims;
    int extinguishedFires;
    int clearedDebris;
    int tasksCompleted;
    
public:
    DisasterSimulation(const SimulationConfig& cfg) 
        : grid(cfg.citySize, vector<Cell>(cfg.citySize)), stepCount(0), config(cfg),
          totalVictims(0), rescuedVictims(0), deceasedVictims(0), 
          extinguishedFires(0), clearedDebris(0), tasksCompleted(0) {
        
        initializeCity();
        initializeAgents();
        initializeDisaster();
    }
    
    void initializeCity() {
        // Initialize grid
        for (int x = 0; x < config.citySize; x++) {
            for (int y = 0; y < config.citySize; y++) {
                grid[x][y] = Cell(x, y);
            }
        }
        
        createCityLayout();
    }
    
    void createCityLayout() {
        // Create roads (every 3rd row and column)
        for (int i = 0; i < config.citySize; i++) {
            for (int j = 0; j < config.citySize; j++) {
                if (i % 3 == 0 || j % 3 == 0) {
                    grid[i][j].type = CellType::ROAD;
                }
            }
        }
        
        // Place hospitals at strategic locations
        vector<Position> hospitalPositions = {
            {2, 2}, {2, config.citySize-3}, {config.citySize-3, 2}, 
            {config.citySize-3, config.citySize-3}, {config.citySize/2, config.citySize/2}
        };
        
        for (const auto& pos : hospitalPositions) {
            if (pos.x < config.citySize && pos.y < config.citySize) {
                grid[pos.x][pos.y].type = CellType::HOSPITAL;
            }
        }
        
        // Create residential and commercial blocks
        for (int x = 1; x < config.citySize-1; x += 4) {
            for (int y = 1; y < config.citySize-1; y += 4) {
                // 2x2 building blocks
                for (int dx = 0; dx < 2; dx++) {
                    for (int dy = 0; dy < 2; dy++) {
                        int nx = x + dx, ny = y + dy;
                        if (nx < config.citySize-1 && ny < config.citySize-1 && 
                            grid[nx][ny].type != CellType::ROAD) {
                            grid[nx][ny].type = CellType::BUILDING;
                        }
                    }
                }
            }
        }
    }
    
    void initializeDisaster() {
        srand(time(0));
        
        // Set initial fires based on user configuration
        int firesPlaced = 0;
        while (firesPlaced < config.numFires) {
            int x = rand() % config.citySize, y = rand() % config.citySize;
            if (grid[x][y].type == CellType::BUILDING) {
                grid[x][y].type = CellType::FIRE;
                grid[x][y].fireIntensity = 20 + rand() % 40; // Lower initial intensity
                firesPlaced++;
            }
        }
        
        // Place victims based on user configuration
        int victimsPlaced = 0;
        while (victimsPlaced < config.numVictims) {
            int x = rand() % config.citySize, y = rand() % config.citySize;
            if (grid[x][y].type == CellType::BUILDING || grid[x][y].type == CellType::FIRE) {
                grid[x][y].hasVictim = true;
                grid[x][y].victimHealth = 40 + rand() % 50; // Higher initial health
                totalVictims++;
                victimsPlaced++;
            }
        }
        
        // Add debris on some roads
        for (int i = 0; i < 8; i++) {
            int x = rand() % config.citySize, y = rand() % config.citySize;
            if (grid[x][y].type == CellType::ROAD) {
                grid[x][y].type = CellType::DEBRIS;
            }
        }
    }
    
    void initializeAgents() {
        // Strategic starting positions
        vector<Position> startPositions = {
            {1, 1}, {1, config.citySize-2}, {config.citySize-2, 1}, 
            {config.citySize-2, config.citySize-2}, {config.citySize/2, 1}, 
            {1, config.citySize/2}, {config.citySize-2, config.citySize/2}, 
            {config.citySize/2, config.citySize-2}, {config.citySize/4, config.citySize/4},
            {3*config.citySize/4, 3*config.citySize/4}
        };
        
        // Create agents based on user configuration
        for (int i = 0; i < config.numRescueTeams && i < startPositions.size(); i++) {
            agents.push_back(make_shared<RescueTeam>(startPositions[i], grid));
        }
        
        for (int i = 0; i < config.numFireBrigades && i < startPositions.size(); i++) {
            agents.push_back(make_shared<FireBrigade>(startPositions[i], grid));
        }
        
        for (int i = 0; i < config.numAmbulances && i < startPositions.size(); i++) {
            agents.push_back(make_shared<Ambulance>(startPositions[i], grid));
        }
    }
    
    void spreadFire() {
        vector<Position> newFires;
        
        for (int x = 0; x < config.citySize; x++) {
            for (int y = 0; y < config.citySize; y++) {
                if (grid[x][y].type == CellType::FIRE && grid[x][y].fireIntensity > 50) {
                    // REDUCED chance to spread to adjacent cells
                    for (const auto& neighbor : grid[x][y].pos.getNeighbors()) {
                        if (neighbor.x >= 0 && neighbor.x < config.citySize && 
                            neighbor.y >= 0 && neighbor.y < config.citySize) {
                            
                            Cell& neighborCell = grid[neighbor.x][neighbor.y];
                            if ((neighborCell.type == CellType::BUILDING || 
                                 neighborCell.type == CellType::ROAD) &&
                                rand() % 6 == 0) { // Reduced from 4 to 6
                                
                                newFires.push_back(neighbor);
                            }
                        }
                    }
                }
            }
        }
        
        // Apply new fires
        for (const auto& pos : newFires) {
            if (grid[pos.x][pos.y].type != CellType::HOSPITAL) {
                grid[pos.x][pos.y].type = CellType::FIRE;
                grid[pos.x][pos.y].fireIntensity = 15; // Lower initial intensity
            }
        }
    }
    
    void updateVictims() {
        for (int x = 0; x < config.citySize; x++) {
            for (int y = 0; y < config.citySize; y++) {
                if (grid[x][y].hasVictim && !grid[x][y].victimRescued) {
                    if (grid[x][y].victimHealth <= 0) {
                        grid[x][y].hasVictim = false;
                        deceasedVictims++;
                    }
                }
            }
        }
    }
    
    void updateTasks() {
        // Remove completed tasks
        tasks.erase(remove_if(tasks.begin(), tasks.end(),
            [](const shared_ptr<Task>& t) { return t->assigned && t->assignedAgent.empty(); }),
            tasks.end());
        
        // Scan for new tasks
        for (int x = 0; x < config.citySize; x++) {
            for (int y = 0; y < config.citySize; y++) {
                Position pos(x, y);
                
                // Fire tasks
                if (grid[x][y].type == CellType::FIRE && grid[x][y].fireIntensity > 0) {
                    if (!isTaskExists(TaskType::EXTINGUISH_FIRE, pos)) {
                        string taskId = "FIRE_" + to_string(x) + "_" + to_string(y);
                        tasks.push_back(make_shared<Task>(TaskType::EXTINGUISH_FIRE, pos, 
                                                         grid[x][y].fireIntensity, taskId));
                    }
                }
                
                // Victim rescue tasks
                if (grid[x][y].hasVictim && !grid[x][y].victimRescued) {
                    if (!isTaskExists(TaskType::RESCUE_VICTIM, pos)) {
                        string taskId = "VICTIM_" + to_string(x) + "_" + to_string(y);
                        tasks.push_back(make_shared<Task>(TaskType::RESCUE_VICTIM, pos, 
                                                        100 - grid[x][y].victimHealth, taskId));
                    }
                }
                
                // Debris clearance tasks
                if (grid[x][y].type == CellType::DEBRIS) {
                    if (!isTaskExists(TaskType::CLEAR_DEBRIS, pos)) {
                        string taskId = "DEBRIS_" + to_string(x) + "_" + to_string(y);
                        tasks.push_back(make_shared<Task>(TaskType::CLEAR_DEBRIS, pos, 50, taskId));
                    }
                }
                
                // Transport tasks for rescued victims
                if (grid[x][y].hasVictim && grid[x][y].victimRescued) {
                    if (!isTaskExists(TaskType::TRANSPORT_TO_HOSPITAL, pos)) {
                        string taskId = "TRANSPORT_" + to_string(x) + "_" + to_string(y);
                        tasks.push_back(make_shared<Task>(TaskType::TRANSPORT_TO_HOSPITAL, pos, 
                                                        100 - grid[x][y].victimHealth, taskId));
                    }
                }
            }
        }
        
        // Sort tasks by priority
        sort(tasks.begin(), tasks.end(), [](const shared_ptr<Task>& a, const shared_ptr<Task>& b) {
            return a->priority > b->priority;
        });
    }
    
    bool isTaskExists(TaskType type, const Position& pos) {
        for (const auto& task : tasks) {
            if (task->type == type && task->position == pos && !task->assigned) {
                return true;
            }
        }
        return false;
    }
    
    void assignTasks() {
        // Contract Net Protocol
        for (auto& task : tasks) {
            if (task->assigned) continue;
            
            shared_ptr<Agent> bestAgent = nullptr;
            double bestBid = numeric_limits<double>::max();
            
            for (auto& agent : agents) {
                if (!agent->isBusy() && agent->hasSufficientResources()) {
                    double bid = agent->calculateBid(task);
                    if (bid < bestBid) {
                        bestBid = bid;
                        bestAgent = agent;
                    }
                }
            }
            
            if (bestAgent && bestBid < 150.0) { // Increased bid threshold
                bestAgent->assignTask(task);
                tasksCompleted++;
            }
        }
    }
    
    void updateAgents() {
        for (auto& agent : agents) {
            agent->update();
        }
    }
    
    void updateStatistics() {
        // Count current state
        rescuedVictims = 0;
        extinguishedFires = 0;
        clearedDebris = 0;
        
        for (int x = 0; x < config.citySize; x++) {
            for (int y = 0; y < config.citySize; y++) {
                if (grid[x][y].hasVictim && grid[x][y].victimRescued) rescuedVictims++;
                if (grid[x][y].type == CellType::BUILDING && grid[x][y].fireIntensity == 0) extinguishedFires++;
                if (grid[x][y].type == CellType::ROAD && grid[x][y].type != CellType::DEBRIS) clearedDebris++;
            }
        }
    }
    
    void update() {
        stepCount++;
        
        // 1. Environment evolution
        for (int x = 0; x < config.citySize; x++) {
            for (int y = 0; y < config.citySize; y++) {
                grid[x][y].update();
            }
        }
        
        // 2. Disaster spread - SLOWER
        if (stepCount % 3 == 0) { // Reduced frequency
            spreadFire();
        }
        
        // 3. Victim status update
        updateVictims();
        
        // 4. Task management
        updateTasks();
        
        // 5. Agent assignment
        assignTasks();
        
        // 6. Agent actions
        updateAgents();
        
        // 7. Statistics
        updateStatistics();
    }
    
    void display() {
        ConsoleUtils::clearScreen();
        
        // Header with better UI
        ConsoleUtils::setColor(BRIGHT_CYAN);
        cout << "╔══════════════════════════════════════════════════════════════════════════╗\n";
        cout << "║                    🚨 DISASTER RESPONSE SIMULATION 🚨                   ║\n";
        cout << "╚══════════════════════════════════════════════════════════════════════════╝\n";
        
        ConsoleUtils::setColor(BRIGHT_WHITE);
        cout << "⏰ Step: " << stepCount << " | 🕐 Time: " << stepCount << " minutes";
        cout << " | 👥 Agents: " << agents.size() << " | 📋 Tasks: " << tasks.size() << "\n\n";
        
        // City Map
        ConsoleUtils::setColor(BRIGHT_WHITE);
        cout << "🏙️  CITY MAP:\n";
        cout << "┌";
        for (int x = 0; x < config.citySize; x++) cout << "──";
        cout << "┐\n";
        
        for (int y = 0; y < config.citySize; y++) {
            cout << "│";
            for (int x = 0; x < config.citySize; x++) {
                // Check for agents
                bool agentFound = false;
                for (const auto& agent : agents) {
                    if (agent->getPosition().x == x && agent->getPosition().y == y) {
                        ConsoleUtils::setColor(agent->getColor());
                        cout << agent->getDisplayChar() << " ";
                        agentFound = true;
                        break;
                    }
                }
                
                if (!agentFound) {
                    ConsoleUtils::setColor(grid[x][y].getColor());
                    cout << grid[x][y].getChar() << " ";
                }
            }
            ConsoleUtils::setColor(WHITE);
            cout << "│\n";
        }
        
        cout << "└";
        for (int x = 0; x < config.citySize; x++) cout << "──";
        cout << "┘\n";
        
        // Legend
        ConsoleUtils::setColor(BRIGHT_WHITE);
        cout << "\n📖 LEGEND: ";
        ConsoleUtils::setColor(BRIGHT_YELLOW); cout << "R"; ConsoleUtils::setColor(WHITE); cout << "=Rescue ";
        ConsoleUtils::setColor(BRIGHT_RED); cout << "F"; ConsoleUtils::setColor(WHITE); cout << "=Fire ";
        ConsoleUtils::setColor(BRIGHT_CYAN); cout << "A"; ConsoleUtils::setColor(WHITE); cout << "=Ambulance ";
        ConsoleUtils::setColor(BRIGHT_MAGENTA); cout << "V"; ConsoleUtils::setColor(WHITE); cout << "=Victim ";
        ConsoleUtils::setColor(RED); cout << "F/f/w"; ConsoleUtils::setColor(WHITE); cout << "=Fire ";
        ConsoleUtils::setColor(BRIGHT_GREEN); cout << "H"; ConsoleUtils::setColor(WHITE); cout << "=Hospital ";
        ConsoleUtils::setColor(YELLOW); cout << "D"; ConsoleUtils::setColor(WHITE); cout << "=Debris\n";
        
        // Statistics in a better layout
        ConsoleUtils::setColor(BRIGHT_WHITE);
        cout << "\n📊 MISSION STATISTICS:\n";
        cout << "╔══════════════════════════════════════════════════════════════════════════╗\n";
        
        int activeFires = 0, waitingVictims = 0;
        for (int x = 0; x < config.citySize; x++) {
            for (int y = 0; y < config.citySize; y++) {
                if (grid[x][y].type == CellType::FIRE) activeFires++;
                if (grid[x][y].hasVictim && !grid[x][y].victimRescued) waitingVictims++;
            }
        }
        
        // Left column
        ConsoleUtils::setColor(BRIGHT_GREEN);
        cout << "║  ✅ Victims Rescued: " << setw(3) << rescuedVictims << "/" << setw(3) << totalVictims;
        ConsoleUtils::setColor(BRIGHT_RED);
        cout << "   |  🔥 Active Fires: " << setw(3) << activeFires << "          ║\n";
        
        ConsoleUtils::setColor(BRIGHT_RED);
        cout << "║  ❌ Victims Deceased: " << setw(3) << deceasedVictims;
        ConsoleUtils::setColor(BRIGHT_MAGENTA);
        cout << "         |  ❤️ Waiting Victims: " << setw(3) << waitingVictims << "     ║\n";
        
        ConsoleUtils::setColor(BRIGHT_YELLOW);
        cout << "║  🎯 Fires Extinguished: " << setw(3) << extinguishedFires;
        ConsoleUtils::setColor(BRIGHT_CYAN);
        cout << "     |  📋 Tasks Completed: " << setw(3) << tasksCompleted << "   ║\n";
        
        // Performance
        double successRate = totalVictims > 0 ? (rescuedVictims * 100.0 / totalVictims) : 0;
        ConsoleUtils::setColor(successRate > 80 ? BRIGHT_GREEN : successRate > 50 ? BRIGHT_YELLOW : BRIGHT_RED);
        cout << "║  📈 Success Rate: " << setw(6) << fixed << setprecision(1) << successRate << "%";
        cout << "           |  🚧 Debris Cleared: " << setw(3) << clearedDebris << "   ║\n";
        
        ConsoleUtils::setColor(BRIGHT_WHITE);
        cout << "╚══════════════════════════════════════════════════════════════════════════╝\n";
        
        // Agent Status (only show first few to save space)
        ConsoleUtils::setColor(BRIGHT_WHITE);
        cout << "\n👥 ACTIVE AGENTS (showing first 8):\n";
        int count = 0;
        for (const auto& agent : agents) {
            if (count >= 8) break;
            ConsoleUtils::setColor(agent->getColor());
            cout << "  " << agent->getStatus() << "\n";
            count++;
        }
        if (agents.size() > 8) {
            ConsoleUtils::setColor(BRIGHT_BLACK);
            cout << "  ... and " << (agents.size() - 8) << " more agents\n";
        }
        
        ConsoleUtils::setColor(BRIGHT_WHITE);
        cout << "\n⏭️  Press Ctrl+C to stop | Next update in 1 second...\n";
    }
    
    void run() {
        ConsoleUtils::setWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
        
        while (true) {
            display();
            update();
            ConsoleUtils::sleep(1000);
        }
    }
};

// ==================== MAIN FUNCTION ====================
int main() {
    try {
        ConsoleUtils::setWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
        
        // Get user configuration
        SimulationConfig config = UserInterface::getConfiguration();
        
        ConsoleUtils::setColor(BRIGHT_WHITE);
        cout << "\nInitializing Disaster Response Simulation...\n";
        cout << "🏙️  City Size: " << config.citySize << "x" << config.citySize << "\n";
        cout << "👥 Agents: " << config.numRescueTeams << " Rescue, " 
             << config.numFireBrigades << " Fire, " << config.numAmbulances << " Ambulances\n";
        cout << "🔥 Initial Fires: " << config.numFires << " | ❤️ Initial Victims: " << config.numVictims << "\n";
        cout << "🚀 Starting simulation...\n";
        
        ConsoleUtils::sleep(3000);
        
        DisasterSimulation simulation(config);
        simulation.run();
        
    } catch (const exception& e) {
        ConsoleUtils::setColor(BRIGHT_RED);
        cerr << "Simulation Error: " << e.what() << endl;
        ConsoleUtils::setColor(WHITE);
        cout << "Press Enter to exit..." << endl;
        cin.get();
        return 1;
    }
    
    return 0;
}

// #include <iostream>
// #include <vector>
// #include <memory>
// #include <queue>
// #include <cmath>
// #include <algorithm>
// #include <random>
// #include <string>
// #include <windows.h>
// #include <map>
// #include <set>
// #include <iomanip>
// #include <ctime>

// using namespace std;

// // ==================== CONSTANTS AND ENUMS ====================
// const int GRID_SIZE = 25;
// const int WINDOW_WIDTH = 90;
// const int WINDOW_HEIGHT = 45;

// // Console Colors
// enum ConsoleColor {
//     BLACK = 0, BLUE = 1, GREEN = 2, CYAN = 3, RED = 4, MAGENTA = 5, YELLOW = 6, WHITE = 7,
//     BRIGHT_BLACK = 8, BRIGHT_BLUE = 9, BRIGHT_GREEN = 10, BRIGHT_CYAN = 11,
//     BRIGHT_RED = 12, BRIGHT_MAGENTA = 13, BRIGHT_YELLOW = 14, BRIGHT_WHITE = 15
// };

// enum class CellType { EMPTY, BUILDING, FIRE, VICTIM, HOSPITAL, DEBRIS, ROAD };
// enum class AgentType { RESCUE_TEAM, FIRE_BRIGADE, AMBULANCE };
// enum class TaskType { EXTINGUISH_FIRE, RESCUE_VICTIM, TRANSPORT_TO_HOSPITAL, CLEAR_DEBRIS };
// enum class AgentState { IDLE, MOVING, WORKING, RETURNING };

// // ==================== UTILITY CLASSES ====================
// class Position {
// public:
//     int x, y;
//     Position(int x = 0, int y = 0) : x(x), y(y) {}
    
//     bool operator==(const Position& other) const { return x == other.x && y == other.y; }
//     bool operator<(const Position& other) const { return x < other.x || (x == other.x && y < other.y); }
    
//     double distanceTo(const Position& other) const {
//         return sqrt(pow(x - other.x, 2) + pow(y - other.y, 2));
//     }
    
//     vector<Position> getNeighbors() const {
//         return {{x-1,y}, {x+1,y}, {x,y-1}, {x,y+1}};
//     }
// };

// class ConsoleUtils {
// public:
//     static void setColor(int color) {
//         SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
//     }
    
//     static void clearScreen() { system("cls"); }
//     static void sleep(int ms) { Sleep(ms); }
    
//     static void setCursor(int x, int y) {
//         COORD coord = {static_cast<SHORT>(x), static_cast<SHORT>(y)};
//         SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
//     }
    
//     static void setWindowSize(int width, int height) {
//         string command = "mode con: cols=" + to_string(width) + " lines=" + to_string(height);
//         system(command.c_str());
//     }
// };

// // ==================== USER CONFIGURATION ====================
// struct SimulationConfig {
//     int numFires;
//     int numVictims;
//     int numRescueTeams;
//     int numFireBrigades;
//     int numAmbulances;
//     int citySize;
    
//     SimulationConfig() 
//         : numFires(3), numVictims(5), numRescueTeams(2), 
//           numFireBrigades(3), numAmbulances(2), citySize(GRID_SIZE) {}
// };

// class UserInterface {
// public:
//     static SimulationConfig getConfiguration() {
//         SimulationConfig config;
//         ConsoleUtils::clearScreen();
        
//         ConsoleUtils::setColor(BRIGHT_CYAN);
//         cout << "╔══════════════════════════════════════════════════════════════╗\n";
//         cout << "║                 DISASTER RESPONSE SIMULATION                ║\n";
//         cout << "║                    CONFIGURATION MENU                       ║\n";
//         cout << "╚══════════════════════════════════════════════════════════════╝\n\n";
        
//         ConsoleUtils::setColor(BRIGHT_WHITE);
//         cout << "Configure the disaster scenario:\n\n";
        
//         config.numFires = getNumberInput("Number of buildings on fire (1-10): ", 1, 10);
//         config.numVictims = getNumberInput("Number of victims (1-15): ", 1, 15);
//         config.numRescueTeams = getNumberInput("Number of rescue teams (1-8): ", 1, 8);
//         config.numFireBrigades = getNumberInput("Number of fire brigades (1-8): ", 1, 8);
//         config.numAmbulances = getNumberInput("Number of ambulances (1-6): ", 1, 6);
        
//         ConsoleUtils::setColor(BRIGHT_GREEN);
//         cout << "\n✅ Configuration complete! Starting simulation...\n";
//         ConsoleUtils::sleep(2000);
        
//         return config;
//     }
    
// private:
//     static int getNumberInput(const string& prompt, int minVal, int maxVal) {
//         int value;
//         while (true) {
//             ConsoleUtils::setColor(BRIGHT_YELLOW);
//             cout << prompt;
//             ConsoleUtils::setColor(WHITE);
            
//             if (cin >> value) {
//                 if (value >= minVal && value <= maxVal) {
//                     break;
//                 }
//             }
//             ConsoleUtils::setColor(BRIGHT_RED);
//             cout << "❌ Please enter a number between " << minVal << " and " << maxVal << "!\n";
//             cin.clear();
//             cin.ignore(numeric_limits<streamsize>::max(), '\n');
//         }
//         return value;
//     }
// };

// // ==================== TASK CLASS ====================
// class Task {
// public:
//     TaskType type;
//     Position position;
//     int urgency;
//     int priority;
//     string id;
//     bool assigned;
    
//     Task(TaskType t, Position pos, int urg, string taskId) 
//         : type(t), position(pos), urgency(urg), assigned(false) {
//         id = taskId;
//         priority = calculatePriority();
//     }
    
//     int calculatePriority() {
//         switch(type) {
//             case TaskType::EXTINGUISH_FIRE: return urgency * 3;
//             case TaskType::RESCUE_VICTIM: return urgency * 2;
//             case TaskType::TRANSPORT_TO_HOSPITAL: return urgency * 2;
//             case TaskType::CLEAR_DEBRIS: return urgency;
//             default: return urgency;
//         }
//     }
    
//     double calculateCost(const Position& agentPos, AgentType agentType) const {
//         double distance = position.distanceTo(agentPos);
        
//         // Agent specialization - much better at their jobs
//         double efficiency = 1.0;
//         switch(agentType) {
//             case AgentType::FIRE_BRIGADE:
//                 efficiency = (type == TaskType::EXTINGUISH_FIRE) ? 0.1 : 2.0;
//                 break;
//             case AgentType::RESCUE_TEAM:
//                 efficiency = (type == TaskType::RESCUE_VICTIM) ? 0.1 : 
//                             (type == TaskType::CLEAR_DEBRIS) ? 0.2 : 1.8;
//                 break;
//             case AgentType::AMBULANCE:
//                 efficiency = (type == TaskType::TRANSPORT_TO_HOSPITAL) ? 0.1 : 1.8;
//                 break;
//         }
        
//         return distance * efficiency;
//     }
    
//     string getDisplayName() const {
//         switch(type) {
//             case TaskType::EXTINGUISH_FIRE: return "🔥 FIRE";
//             case TaskType::RESCUE_VICTIM: return "❤️ VICTIM";
//             case TaskType::TRANSPORT_TO_HOSPITAL: return "🚑 TRANSPORT";
//             case TaskType::CLEAR_DEBRIS: return "🚧 DEBRIS";
//             default: return "❓ UNKNOWN";
//         }
//     }
// };

// // ==================== CELL CLASS ====================
// class Cell {
// public:
//     Position pos;
//     CellType type;
//     int fireIntensity;
//     int victimHealth;
//     bool hasVictim;
//     bool victimRescued;
    
//     Cell() : pos(0,0), type(CellType::EMPTY), fireIntensity(0), victimHealth(100),
//             hasVictim(false), victimRescued(false) {}
    
//     Cell(int x, int y) : pos(x,y), type(CellType::EMPTY), fireIntensity(0), victimHealth(100),
//                         hasVictim(false), victimRescued(false) {}
    
//     void update() {
//         // Fire dynamics - much slower
//         if (type == CellType::FIRE) {
//             fireIntensity = min(100, fireIntensity + 1);
            
//             // Victims in fire take damage
//             if (hasVictim && !victimRescued) {
//                 victimHealth = max(0, victimHealth - 3);
//             }
//         } else if (hasVictim && !victimRescued) {
//             // Normal health deterioration - much slower
//             victimHealth = max(0, victimHealth - 1);
//         }
//     }
    
//     bool isTraversable() const {
//         return type != CellType::DEBRIS && fireIntensity < 70;
//     }
    
//     char getChar() const {
//         if (hasVictim && !victimRescued) {
//             if (victimHealth > 70) return 'V';
//             if (victimHealth > 30) return 'v';
//             return '!';
//         }
        
//         switch(type) {
//             case CellType::EMPTY: return ' ';
//             case CellType::BUILDING: return 'B';
//             case CellType::FIRE: 
//                 if (fireIntensity > 75) return 'F';
//                 if (fireIntensity > 50) return 'f';
//                 return 'w';
//             case CellType::HOSPITAL: return 'H';
//             case CellType::DEBRIS: return 'D';
//             case CellType::ROAD: return '.';
//             default: return '?';
//         }
//     }
    
//     int getColor() const {
//         if (hasVictim && !victimRescued) {
//             if (victimHealth > 70) return BRIGHT_MAGENTA;
//             if (victimHealth > 30) return MAGENTA;
//             return BRIGHT_RED;
//         }
        
//         switch(type) {
//             case CellType::EMPTY: return BLACK;
//             case CellType::BUILDING: return BLUE;
//             case CellType::FIRE: 
//                 if (fireIntensity > 75) return BRIGHT_RED;
//                 if (fireIntensity > 50) return RED;
//                 return YELLOW;
//             case CellType::HOSPITAL: return BRIGHT_GREEN;
//             case CellType::DEBRIS: return YELLOW;
//             case CellType::ROAD: return BRIGHT_BLACK;
//             default: return WHITE;
//         }
//     }
// };

// // ==================== PATHFINDING - A* ALGORITHM ====================
// class AStar {
// public:
//     static vector<Position> findPath(const Position& start, const Position& goal, 
//                                    const vector<vector<Cell>>& grid) {
//         vector<Position> path;
//         if (!isValid(goal, grid) || !grid[goal.x][goal.y].isTraversable()) {
//             return path;
//         }
        
//         auto cmp = [](const pair<Position, double>& a, const pair<Position, double>& b) { 
//             return a.second > b.second; 
//         };
//         priority_queue<pair<Position, double>, vector<pair<Position, double>>, decltype(cmp)> openList(cmp);
//         map<Position, Position> cameFrom;
//         map<Position, double> costSoFar;
        
//         openList.push({start, 0});
//         cameFrom[start] = start;
//         costSoFar[start] = 0;
        
//         while (!openList.empty()) {
//             Position current = openList.top().first;
//             openList.pop();
            
//             if (current == goal) {
//                 // Reconstruct path
//                 while (!(current == start)) {
//                     path.push_back(current);
//                     current = cameFrom[current];
//                 }
//                 reverse(path.begin(), path.end());
//                 return path;
//             }
            
//             for (const auto& neighbor : current.getNeighbors()) {
//                 if (!isValid(neighbor, grid) || !grid[neighbor.x][neighbor.y].isTraversable()) {
//                     continue;
//                 }
                
//                 double newCost = costSoFar[current] + 1;
//                 if (!costSoFar.count(neighbor) || newCost < costSoFar[neighbor]) {
//                     costSoFar[neighbor] = newCost;
//                     double priority = newCost + heuristic(neighbor, goal);
//                     openList.push({neighbor, priority});
//                     cameFrom[neighbor] = current;
//                 }
//             }
//         }
        
//         return path;
//     }

// private:
//     static bool isValid(const Position& pos, const vector<vector<Cell>>& grid) {
//         return pos.x >= 0 && pos.x < grid.size() && pos.y >= 0 && pos.y < grid[0].size();
//     }
    
//     static double heuristic(const Position& a, const Position& b) {
//         return abs(a.x - b.x) + abs(a.y - b.y);
//     }
// };

// // ==================== AGENT BASE CLASS ====================
// class Agent {
// protected:
//     static int nextId;
//     int id;
//     AgentType type;
//     Position position;
//     Position basePosition;
//     vector<Position> path;
//     AgentState state;
    
//     int water;
//     int medicalKits;
    
//     shared_ptr<Task> currentTask;
//     int workProgress;
    
//     vector<vector<Cell>>* environmentGrid;
    
// public:
//     Agent(AgentType t, Position pos, vector<vector<Cell>>& grid) 
//         : id(nextId++), type(t), position(pos), basePosition(pos), state(AgentState::IDLE),
//           water(100), medicalKits(5), workProgress(0), environmentGrid(&grid) {}
    
//     virtual ~Agent() = default;
    
//     // Getters
//     int getId() const { return id; }
//     AgentType getType() const { return type; }
//     Position getPosition() const { return position; }
//     AgentState getState() const { return state; }
//     bool isBusy() const { return state != AgentState::IDLE; }
//     shared_ptr<Task> getCurrentTask() const { return currentTask; }
    
//     string getTypeName() const {
//         switch(type) {
//             case AgentType::RESCUE_TEAM: return "RESCUE";
//             case AgentType::FIRE_BRIGADE: return "FIRE";
//             case AgentType::AMBULANCE: return "AMBUL";
//             default: return "UNKNOWN";
//         }
//     }
    
//     string getStateName() const {
//         switch(state) {
//             case AgentState::IDLE: return "IDLE";
//             case AgentState::MOVING: return "MOVING";
//             case AgentState::WORKING: return "WORKING";
//             case AgentState::RETURNING: return "RETURNING";
//             default: return "UNKNOWN";
//         }
//     }
    
//     char getDisplayChar() const {
//         switch(type) {
//             case AgentType::RESCUE_TEAM: return 'R';
//             case AgentType::FIRE_BRIGADE: return 'W';
//             case AgentType::AMBULANCE: return 'A';
//             default: return '?';
//         }
//     }
    
//     int getColor() const {
//         switch(type) {
//             case AgentType::RESCUE_TEAM: return BRIGHT_YELLOW;
//             case AgentType::FIRE_BRIGADE: return BRIGHT_BLUE;
//             case AgentType::AMBULANCE: return BRIGHT_CYAN;
//             default: return WHITE;
//         }
//     }
    
//     bool hasSufficientResources() const {
//         switch(type) {
//             case AgentType::FIRE_BRIGADE: return water > 10;
//             case AgentType::AMBULANCE: return medicalKits > 0;
//             case AgentType::RESCUE_TEAM: return true;
//             default: return true;
//         }
//     }
    
//     void refillResources() {
//         if (type == AgentType::FIRE_BRIGADE) water = 100;
//         if (type == AgentType::AMBULANCE) medicalKits = 5;
//     }
    
//     // Task management
//     virtual double calculateBid(shared_ptr<Task> task) = 0;
    
//     void assignTask(shared_ptr<Task> task) {
//         currentTask = task;
//         task->assigned = true;
//         state = AgentState::MOVING;
        
//         path = AStar::findPath(position, task->position, *environmentGrid);
//         if (path.empty()) {
//             state = AgentState::IDLE;
//             currentTask = nullptr;
//             task->assigned = false;
//         }
//     }
    
//     void move() {
//         if (path.empty()) {
//             if (state == AgentState::MOVING && currentTask && position == currentTask->position) {
//                 state = AgentState::WORKING;
//                 workProgress = 0;
//             } else if (state == AgentState::RETURNING && position == basePosition) {
//                 state = AgentState::IDLE;
//                 refillResources();
//             }
//             return;
//         }
        
//         Position nextPos = path.front();
//         path.erase(path.begin());
        
//         if (isValidMove(nextPos)) {
//             position = nextPos;
//         }
//     }
    
//     virtual void performTask() = 0;
    
//     void update() {
//         if (state == AgentState::MOVING) {
//             move();
//         } else if (state == AgentState::WORKING) {
//             performTask();
//             workProgress++;
            
//             if (workProgress >= 2) { // Only 2 steps to complete task
//                 completeTask();
//             }
//         }
//     }
    
//     void completeTask() {
//         if (currentTask) {
//             currentTask->assigned = false;
//         }
        
//         if (!hasSufficientResources()) {
//             state = AgentState::RETURNING;
//             path = AStar::findPath(position, basePosition, *environmentGrid);
//         } else {
//             state = AgentState::IDLE;
//         }
        
//         currentTask = nullptr;
//         workProgress = 0;
//     }
    
//     string getStatus() const {
//         string status = getTypeName() + to_string(id) + " @(" + 
//                        to_string(position.x) + "," + to_string(position.y) + ") ";
//         status += "[" + getStateName() + "] ";
        
//         if (currentTask) {
//             status += "-> " + currentTask->getDisplayName();
//         }
        
//         if (type == AgentType::FIRE_BRIGADE) status += " W:" + to_string(water);
//         if (type == AgentType::AMBULANCE) status += " M:" + to_string(medicalKits);
        
//         return status;
//     }

// protected:
//     bool isValidMove(const Position& pos) {
//         if (pos.x < 0 || pos.x >= environmentGrid->size() || 
//             pos.y < 0 || pos.y >= (*environmentGrid)[0].size()) {
//             return false;
//         }
//         return (*environmentGrid)[pos.x][pos.y].isTraversable();
//     }
// };

// int Agent::nextId = 1;

// // ==================== SPECIFIC AGENT CLASSES ====================
// class RescueTeam : public Agent {
// public:
//     RescueTeam(Position pos, vector<vector<Cell>>& grid) 
//         : Agent(AgentType::RESCUE_TEAM, pos, grid) {}
    
//     double calculateBid(shared_ptr<Task> task) override {
//         if (!hasSufficientResources()) return 9999.0;
        
//         switch(task->type) {
//             case TaskType::RESCUE_VICTIM:
//             case TaskType::CLEAR_DEBRIS:
//                 return task->calculateCost(position, type);
//             default:
//                 return 9999.0;
//         }
//     }
    
//     void performTask() override {
//         if (!currentTask) return;
        
//         Cell& cell = (*environmentGrid)[position.x][position.y];
        
//         switch(currentTask->type) {
//             case TaskType::RESCUE_VICTIM:
//                 if (cell.hasVictim && !cell.victimRescued) {
//                     cell.victimRescued = true;
//                 }
//                 break;
                
//             case TaskType::CLEAR_DEBRIS:
//                 if (cell.type == CellType::DEBRIS) {
//                     cell.type = CellType::ROAD;
//                 }
//                 break;
//         }
//     }
// };

// class FireBrigade : public Agent {
// public:
//     FireBrigade(Position pos, vector<vector<Cell>>& grid) 
//         : Agent(AgentType::FIRE_BRIGADE, pos, grid) {}
    
//     double calculateBid(shared_ptr<Task> task) override {
//         if (!hasSufficientResources()) return 9999.0;
        
//         switch(task->type) {
//             case TaskType::EXTINGUISH_FIRE:
//                 return task->calculateCost(position, type);
//             default:
//                 return 9999.0;
//         }
//     }
    
//     void performTask() override {
//         if (!currentTask) return;
        
//         Cell& cell = (*environmentGrid)[position.x][position.y];
        
//         if (cell.type == CellType::FIRE) {
//             // Much more effective fire extinguishing
//             cell.fireIntensity = max(0, cell.fireIntensity - 80);
//             water = max(0, water - 5);
            
//             if (cell.fireIntensity == 0) {
//                 cell.type = CellType::BUILDING;
//             }
//         }
//     }
// };

// class Ambulance : public Agent {
// public:
//     Ambulance(Position pos, vector<vector<Cell>>& grid) 
//         : Agent(AgentType::AMBULANCE, pos, grid) {}
    
//     double calculateBid(shared_ptr<Task> task) override {
//         if (!hasSufficientResources()) return 9999.0;
        
//         switch(task->type) {
//             case TaskType::TRANSPORT_TO_HOSPITAL:
//                 return task->calculateCost(position, type);
//             default:
//                 return 9999.0;
//         }
//     }
    
//     void performTask() override {
//         if (!currentTask) return;
        
//         Cell& cell = (*environmentGrid)[position.x][position.y];
        
//         if (cell.hasVictim && cell.victimRescued) {
//             cell.hasVictim = false;
//             cell.victimRescued = false;
//             medicalKits = max(0, medicalKits - 1);
//         }
//     }
// };

// // ==================== SIMULATION ENGINE ====================
// class DisasterSimulation {
// private:
//     vector<vector<Cell>> grid;
//     vector<shared_ptr<Agent>> agents;
//     vector<shared_ptr<Task>> tasks;
//     int stepCount;
//     SimulationConfig config;
//     bool simulationOver;
//     bool victory;
    
//     int totalVictims;
//     int rescuedVictims;
//     int deceasedVictims;
//     int tasksCompleted;
    
// public:
//     DisasterSimulation(const SimulationConfig& cfg) 
//         : grid(cfg.citySize, vector<Cell>(cfg.citySize)), stepCount(0), config(cfg),
//           totalVictims(0), rescuedVictims(0), deceasedVictims(0), tasksCompleted(0),
//           simulationOver(false), victory(false) {
        
//         initializeCity();
//         initializeAgents();
//         initializeDisaster();
//     }
    
//     void initializeCity() {
//         for (int x = 0; x < config.citySize; x++) {
//             for (int y = 0; y < config.citySize; y++) {
//                 grid[x][y] = Cell(x, y);
//             }
//         }
        
//         createCityLayout();
//     }
    
//     void createCityLayout() {
//         // Create roads
//         for (int i = 0; i < config.citySize; i++) {
//             for (int j = 0; j < config.citySize; j++) {
//                 if (i % 3 == 0 || j % 3 == 0) {
//                     grid[i][j].type = CellType::ROAD;
//                 }
//             }
//         }
        
//         // Place hospitals
//         vector<Position> hospitalPositions = {
//             {2, 2}, {2, config.citySize-3}, {config.citySize-3, 2}, 
//             {config.citySize-3, config.citySize-3}
//         };
        
//         for (const auto& pos : hospitalPositions) {
//             if (pos.x < config.citySize && pos.y < config.citySize) {
//                 grid[pos.x][pos.y].type = CellType::HOSPITAL;
//             }
//         }
        
//         // Create buildings
//         for (int x = 1; x < config.citySize-1; x += 4) {
//             for (int y = 1; y < config.citySize-1; y += 4) {
//                 for (int dx = 0; dx < 2; dx++) {
//                     for (int dy = 0; dy < 2; dy++) {
//                         int nx = x + dx, ny = y + dy;
//                         if (nx < config.citySize-1 && ny < config.citySize-1 && 
//                             grid[nx][ny].type != CellType::ROAD) {
//                             grid[nx][ny].type = CellType::BUILDING;
//                         }
//                     }
//                 }
//             }
//         }
//     }
    
//     void initializeDisaster() {
//         srand(time(0));
        
//         // Set initial fires
//         int firesPlaced = 0;
//         while (firesPlaced < config.numFires) {
//             int x = rand() % config.citySize, y = rand() % config.citySize;
//             if (grid[x][y].type == CellType::BUILDING) {
//                 grid[x][y].type = CellType::FIRE;
//                 grid[x][y].fireIntensity = 30 + rand() % 40;
//                 firesPlaced++;
//             }
//         }
        
//         // Place victims
//         int victimsPlaced = 0;
//         while (victimsPlaced < config.numVictims) {
//             int x = rand() % config.citySize, y = rand() % config.citySize;
//             if (grid[x][y].type == CellType::BUILDING || grid[x][y].type == CellType::FIRE) {
//                 grid[x][y].hasVictim = true;
//                 grid[x][y].victimHealth = 60 + rand() % 40; // Higher starting health
//                 totalVictims++;
//                 victimsPlaced++;
//             }
//         }
        
//         // Add less debris
//         for (int i = 0; i < 3; i++) {
//             int x = rand() % config.citySize, y = rand() % config.citySize;
//             if (grid[x][y].type == CellType::ROAD) {
//                 grid[x][y].type = CellType::DEBRIS;
//             }
//         }
//     }
    
//     void initializeAgents() {
//         vector<Position> startPositions = {
//             {1, 1}, {1, config.citySize-2}, {config.citySize-2, 1}, 
//             {config.citySize-2, config.citySize-2}, {config.citySize/2, 1}
//         };
        
//         for (int i = 0; i < config.numRescueTeams && i < startPositions.size(); i++) {
//             agents.push_back(make_shared<RescueTeam>(startPositions[i], grid));
//         }
        
//         for (int i = 0; i < config.numFireBrigades && i < startPositions.size(); i++) {
//             agents.push_back(make_shared<FireBrigade>(startPositions[i], grid));
//         }
        
//         for (int i = 0; i < config.numAmbulances && i < startPositions.size(); i++) {
//             agents.push_back(make_shared<Ambulance>(startPositions[i], grid));
//         }
//     }
    
//     void spreadFire() {
//         vector<Position> newFires;
        
//         for (int x = 0; x < config.citySize; x++) {
//             for (int y = 0; y < config.citySize; y++) {
//                 if (grid[x][y].type == CellType::FIRE && grid[x][y].fireIntensity > 50) {
//                     for (const auto& neighbor : grid[x][y].pos.getNeighbors()) {
//                         if (neighbor.x >= 0 && neighbor.x < config.citySize && 
//                             neighbor.y >= 0 && neighbor.y < config.citySize) {
                            
//                             Cell& neighborCell = grid[neighbor.x][neighbor.y];
//                             if ((neighborCell.type == CellType::BUILDING) && rand() % 8 == 0) {
//                                 newFires.push_back(neighbor);
//                             }
//                         }
//                     }
//                 }
//             }
//         }
        
//         for (const auto& pos : newFires) {
//             if (grid[pos.x][pos.y].type != CellType::HOSPITAL) {
//                 grid[pos.x][pos.y].type = CellType::FIRE;
//                 grid[pos.x][pos.y].fireIntensity = 20;
//             }
//         }
//     }
    
//     void updateVictims() {
//         for (int x = 0; x < config.citySize; x++) {
//             for (int y = 0; y < config.citySize; y++) {
//                 if (grid[x][y].hasVictim && !grid[x][y].victimRescued) {
//                     if (grid[x][y].victimHealth <= 0) {
//                         grid[x][y].hasVictim = false;
//                         deceasedVictims++;
//                     }
//                 }
//             }
//         }
//     }
    
//     void updateTasks() {
//         // Remove completed tasks
//         tasks.erase(remove_if(tasks.begin(), tasks.end(),
//             [](const shared_ptr<Task>& t) { return t->assigned && !t->assigned; }),
//             tasks.end());
        
//         // Scan for new tasks
//         for (int x = 0; x < config.citySize; x++) {
//             for (int y = 0; y < config.citySize; y++) {
//                 Position pos(x, y);
                
//                 // Fire tasks
//                 if (grid[x][y].type == CellType::FIRE && grid[x][y].fireIntensity > 0) {
//                     if (!isTaskExists(TaskType::EXTINGUISH_FIRE, pos)) {
//                         string taskId = "FIRE_" + to_string(x) + "_" + to_string(y);
//                         tasks.push_back(make_shared<Task>(TaskType::EXTINGUISH_FIRE, pos, 
//                                                          grid[x][y].fireIntensity, taskId));
//                     }
//                 }
                
//                 // Victim rescue tasks
//                 if (grid[x][y].hasVictim && !grid[x][y].victimRescued) {
//                     if (!isTaskExists(TaskType::RESCUE_VICTIM, pos)) {
//                         string taskId = "VICTIM_" + to_string(x) + "_" + to_string(y);
//                         tasks.push_back(make_shared<Task>(TaskType::RESCUE_VICTIM, pos, 
//                                                         100 - grid[x][y].victimHealth, taskId));
//                     }
//                 }
                
//                 // Debris clearance tasks
//                 if (grid[x][y].type == CellType::DEBRIS) {
//                     if (!isTaskExists(TaskType::CLEAR_DEBRIS, pos)) {
//                         string taskId = "DEBRIS_" + to_string(x) + "_" + to_string(y);
//                         tasks.push_back(make_shared<Task>(TaskType::CLEAR_DEBRIS, pos, 50, taskId));
//                     }
//                 }
                
//                 // Transport tasks for rescued victims
//                 if (grid[x][y].hasVictim && grid[x][y].victimRescued) {
//                     if (!isTaskExists(TaskType::TRANSPORT_TO_HOSPITAL, pos)) {
//                         string taskId = "TRANSPORT_" + to_string(x) + "_" + to_string(y);
//                         tasks.push_back(make_shared<Task>(TaskType::TRANSPORT_TO_HOSPITAL, pos, 
//                                                         100 - grid[x][y].victimHealth, taskId));
//                     }
//                 }
//             }
//         }
        
//         // Sort tasks by priority
//         sort(tasks.begin(), tasks.end(), [](const shared_ptr<Task>& a, const shared_ptr<Task>& b) {
//             return a->priority > b->priority;
//         });
//     }
    
//     bool isTaskExists(TaskType type, const Position& pos) {
//         for (const auto& task : tasks) {
//             if (task->type == type && task->position == pos && !task->assigned) {
//                 return true;
//             }
//         }
//         return false;
//     }
    
//     void assignTasks() {
//         for (auto& task : tasks) {
//             if (task->assigned) continue;
            
//             shared_ptr<Agent> bestAgent = nullptr;
//             double bestBid = 9999.0;
            
//             for (auto& agent : agents) {
//                 if (!agent->isBusy() && agent->hasSufficientResources()) {
//                     double bid = agent->calculateBid(task);
//                     if (bid < bestBid) {
//                         bestBid = bid;
//                         bestAgent = agent;
//                     }
//                 }
//             }
            
//             if (bestAgent && bestBid < 50.0) {
//                 bestAgent->assignTask(task);
//                 tasksCompleted++;
//             }
//         }
//     }
    
//     void updateAgents() {
//         for (auto& agent : agents) {
//             agent->update();
//         }
//     }
    
//     void updateStatistics() {
//         rescuedVictims = 0;
//         for (int x = 0; x < config.citySize; x++) {
//             for (int y = 0; y < config.citySize; y++) {
//                 if (grid[x][y].hasVictim && grid[x][y].victimRescued) rescuedVictims++;
//             }
//         }
//     }
    
//     bool checkWinCondition() {
//         int activeFires = 0;
//         int waitingVictims = 0;
        
//         for (int x = 0; x < config.citySize; x++) {
//             for (int y = 0; y < config.citySize; y++) {
//                 if (grid[x][y].type == CellType::FIRE) activeFires++;
//                 if (grid[x][y].hasVictim && !grid[x][y].victimRescued) waitingVictims++;
//             }
//         }
        
//         // WIN: All fires out and victims saved
//         if (activeFires == 0 && waitingVictims == 0) {
//             victory = true;
//             return true;
//         }
        
//         // LOSE: Too many deaths or too long
//         if (deceasedVictims > totalVictims * 0.4 || stepCount > 200) {
//             victory = false;
//             return true;
//         }
        
//         return false;
//     }
    
//     void update() {
//         if (simulationOver) return;
        
//         stepCount++;
        
//         // Update environment
//         for (int x = 0; x < config.citySize; x++) {
//             for (int y = 0; y < config.citySize; y++) {
//                 grid[x][y].update();
//             }
//         }
        
//         // Slower fire spread
//         if (stepCount % 4 == 0) {
//             spreadFire();
//         }
        
//         updateVictims();
//         updateTasks();
//         assignTasks();
//         updateAgents();
//         updateStatistics();
        
//         simulationOver = checkWinCondition();
//     }
    
//     void display() {
//         ConsoleUtils::clearScreen();
        
//         ConsoleUtils::setColor(BRIGHT_CYAN);
//         cout << "╔══════════════════════════════════════════════════════════════════════════╗\n";
        
//         if (simulationOver) {
//             if (victory) {
//                 ConsoleUtils::setColor(BRIGHT_GREEN);
//                 cout << "║                    🎉 MISSION ACCOMPLISHED! 🎉                      ║\n";
//             } else {
//                 ConsoleUtils::setColor(BRIGHT_RED);
//                 cout << "║                    💀 MISSION FAILED! 💀                           ║\n";
//             }
//         } else {
//             cout << "║                    🚨 DISASTER RESPONSE SIMULATION 🚨                   ║\n";
//         }
//         cout << "╚══════════════════════════════════════════════════════════════════════════╝\n";
        
//         ConsoleUtils::setColor(BRIGHT_WHITE);
//         cout << "⏰ Step: " << stepCount << " | Agents: " << agents.size() << " | Tasks: " << tasks.size() << "\n\n";
        
//         // City Map
//         ConsoleUtils::setColor(BRIGHT_WHITE);
//         cout << "CITY MAP:\n";
//         cout << "┌";
//         for (int x = 0; x < config.citySize; x++) cout << "──";
//         cout << "┐\n";
        
//         for (int y = 0; y < config.citySize; y++) {
//             cout << "│";
//             for (int x = 0; x < config.citySize; x++) {
//                 bool agentFound = false;
//                 for (const auto& agent : agents) {
//                     if (agent->getPosition().x == x && agent->getPosition().y == y) {
//                         ConsoleUtils::setColor(agent->getColor());
//                         cout << agent->getDisplayChar() << " ";
//                         agentFound = true;
//                         break;
//                     }
//                 }
                
//                 if (!agentFound) {
//                     ConsoleUtils::setColor(grid[x][y].getColor());
//                     cout << grid[x][y].getChar() << " ";
//                 }
//             }
//             ConsoleUtils::setColor(WHITE);
//             cout << "│\n";
//         }
        
//         cout << "└";
//         for (int x = 0; x < config.citySize; x++) cout << "──";
//         cout << "┘\n";
        
//         // Statistics
//         ConsoleUtils::setColor(BRIGHT_WHITE);
//         cout << "\nSTATISTICS:\n";
        
//         int activeFires = 0, waitingVictims = 0;
//         for (int x = 0; x < config.citySize; x++) {
//             for (int y = 0; y < config.citySize; y++) {
//                 if (grid[x][y].type == CellType::FIRE) activeFires++;
//                 if (grid[x][y].hasVictim && !grid[x][y].victimRescued) waitingVictims++;
//             }
//         }
        
//         ConsoleUtils::setColor(BRIGHT_GREEN);
//         cout << "✅ Rescued: " << rescuedVictims << "/" << totalVictims;
//         ConsoleUtils::setColor(BRIGHT_RED);
//         cout << " | ❌ Deceased: " << deceasedVictims;
//         ConsoleUtils::setColor(BRIGHT_RED);
//         cout << " | 🔥 Fires: " << activeFires;
//         ConsoleUtils::setColor(BRIGHT_MAGENTA);
//         cout << " | ❤️ Waiting: " << waitingVictims << "\n";
        
//         double successRate = totalVictims > 0 ? (rescuedVictims * 100.0 / totalVictims) : 0;
//         ConsoleUtils::setColor(successRate > 50 ? BRIGHT_GREEN : BRIGHT_RED);
//         cout << "📈 Success Rate: " << fixed << setprecision(1) << successRate << "%";
//         ConsoleUtils::setColor(BRIGHT_CYAN);
//         cout << " | 📋 Tasks: " << tasksCompleted << "\n";
        
//         if (simulationOver) {
//             cout << "\n";
//             if (victory) {
//                 ConsoleUtils::setColor(BRIGHT_GREEN);
//                 cout << "🎉 VICTORY! All fires extinguished and victims saved! 🎉\n";
//             } else {
//                 ConsoleUtils::setColor(BRIGHT_RED);
//                 cout << "💀 Failed: " << (deceasedVictims > totalVictims * 0.4 ? "Too many casualties" : "Time ran out") << " 💀\n";
//             }
//             ConsoleUtils::setColor(BRIGHT_WHITE);
//             cout << "Press any key to exit...\n";
//             return;
//         }
        
//         // Agent Status
//         ConsoleUtils::setColor(BRIGHT_WHITE);
//         cout << "\nAGENTS:\n";
//         for (const auto& agent : agents) {
//             ConsoleUtils::setColor(agent->getColor());
//             cout << "  " << agent->getStatus() << "\n";
//         }
        
//         ConsoleUtils::setColor(BRIGHT_WHITE);
//         cout << "\nPress Ctrl+C to stop\n";
//     }
    
//     void run() {
//         ConsoleUtils::setWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
        
//         while (!simulationOver) {
//             display();
//             update();
//             ConsoleUtils::sleep(1000);
//         }
        
//         display();
//         cin.ignore();
//         cin.get();
//     }
// };

// // ==================== MAIN FUNCTION ====================
// int main() {
//     try {
//         ConsoleUtils::setWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
        
//         SimulationConfig config = UserInterface::getConfiguration();
        
//         ConsoleUtils::setColor(BRIGHT_WHITE);
//         cout << "\nStarting simulation...\n";
//         ConsoleUtils::sleep(2000);
        
//         DisasterSimulation simulation(config);
//         simulation.run();
        
//     } catch (const exception& e) {
//         ConsoleUtils::setColor(BRIGHT_RED);
//         cerr << "Error: " << e.what() << endl;
//         ConsoleUtils::setColor(WHITE);
//         cout << "Press Enter to exit..." << endl;
//         cin.get();
//         return 1;
//     }
    
//     return 0;
// }