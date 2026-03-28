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

using namespace std;

// ==================== CONSTANTS AND ENUMS ====================
const int GRID_SIZE = 25;
const int WINDOW_WIDTH = 80;
const int WINDOW_HEIGHT = 40;

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
                efficiency = (type == TaskType::EXTINGUISH_FIRE) ? 0.3 : 1.8;
                break;
            case AgentType::RESCUE_TEAM:
                efficiency = (type == TaskType::RESCUE_VICTIM) ? 0.4 : 
                            (type == TaskType::CLEAR_DEBRIS) ? 0.5 : 1.6;
                break;
            case AgentType::AMBULANCE:
                efficiency = (type == TaskType::TRANSPORT_TO_HOSPITAL) ? 0.3 : 1.7;
                break;
        }
        
        return (distance * 0.7) + (urgencyFactor * 0.2) + (efficiency * 0.1);
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
        // Fire dynamics
        if (type == CellType::FIRE) {
            fireIntensity = min(100, fireIntensity + 2);
            smokeLevel = min(3, smokeLevel + 1);
            
            // Victims in fire take more damage
            if (hasVictim && !victimRescued) {
                victimHealth = max(0, victimHealth - 8);
            }
        } else if (hasVictim && !victimRescued) {
            // Normal health deterioration
            victimHealth = max(0, victimHealth - 1);
        }
        
        // Smoke dissipation
        if (smokeLevel > 0 && type != CellType::FIRE) {
            smokeLevel--;
        }
    }
    
    bool isTraversable() const {
        return type != CellType::DEBRIS && fireIntensity < 50;
    }
    
    bool isAccessible() const {
        return type != CellType::DEBRIS && fireIntensity < 70;
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
    
    string getTypeName() const {
        switch(type) {
            case CellType::EMPTY: return "Empty";
            case CellType::BUILDING: return "Building";
            case CellType::FIRE: return "Fire";
            case CellType::VICTIM: return "Victim";
            case CellType::HOSPITAL: return "Hospital";
            case CellType::DEBRIS: return "Debris";
            case CellType::ROAD: return "Road";
            default: return "Unknown";
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
                
                double g = current->g + getCost(current->pos, neighborPos, grid);
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
        // Manhattan distance for grid
        return abs(a.x - b.x) + abs(a.y - b.y);
    }
    
    static double getCost(const Position& from, const Position& to, 
                         const vector<vector<Cell>>& grid) {
        double baseCost = 1.0;
        
        // Higher cost for dangerous areas
        if (grid[to.x][to.y].type == CellType::FIRE) {
            baseCost += 2.0;
        }
        if (grid[to.x][to.y].smokeLevel > 0) {
            baseCost += 0.5;
        }
        
        return baseCost;
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
    
public:
    Agent(AgentType t, Position pos) 
        : id(nextId++), type(t), position(pos), basePosition(pos), state(AgentState::IDLE),
          fuel(100), water(100), medicalKits(5), efficiency(100), workProgress(0), workRequired(3) {}
    
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
            case AgentType::AMBULANCE: return "AMBULANCE";
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
            case AgentType::FIRE_BRIGADE: return 'F';
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
    
    // Resource management
    bool hasSufficientResources() const {
        if (fuel < 20) return false;
        switch(type) {
            case AgentType::FIRE_BRIGADE: return water > 10;
            case AgentType::AMBULANCE: return medicalKits > 0;
            case AgentType::RESCUE_TEAM: return true;
            default: return true;
        }
    }
    
    void consumeResources() {
        fuel = max(0, fuel - 1);
        if (type == AgentType::FIRE_BRIGADE && state == AgentState::WORKING) {
            water = max(0, water - 5);
        }
    }
    
    void refillResources() {
        fuel = 100;
        if (type == AgentType::FIRE_BRIGADE) water = 100;
        if (type == AgentType::AMBULANCE) medicalKits = min(5, medicalKits + 2);
    }
    
    // Task management
    virtual double calculateBid(shared_ptr<Task> task) = 0;
    
    void assignTask(shared_ptr<Task> task) {
        currentTask = task;
        task->assigned = true;
        task->assignedAgent = getTypeName() + to_string(id);
        state = AgentState::MOVING;
        
        // Calculate path to task
        path = AStar::findPath(position, task->position, getEnvironmentGrid());
        if (path.empty()) {
            state = AgentState::IDLE;
            currentTask = nullptr;
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
    
    virtual void performTask(vector<vector<Cell>>& grid) = 0;
    
    void update() {
        if (state == AgentState::MOVING) {
            move();
        } else if (state == AgentState::WORKING) {
            performTask(grid);
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
            path = AStar::findPath(position, basePosition, getEnvironmentGrid());
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
    virtual bool isValidMove(const Position& pos) = 0;
    virtual vector<vector<Cell>> getEnvironmentGrid() = 0;
};

int Agent::nextId = 1;

// ==================== SPECIFIC AGENT CLASSES ====================
class RescueTeam : public Agent {
private:
    vector<vector<Cell>>* gridRef;
    
public:
    RescueTeam(Position pos, vector<vector<Cell>>& grid) 
        : Agent(AgentType::RESCUE_TEAM, pos), gridRef(&grid) {}
    
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
    
    void performTask(vector<vector<Cell>>& grid) override {
        if (!currentTask) return;
        
        Cell& cell = grid[position.x][position.y];
        
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
                    fuel = max(0, fuel - 5); // Heavy work
                }
                break;
        }
    }

protected:
    bool isValidMove(const Position& pos) override {
        if (pos.x < 0 || pos.x >= gridRef->size() || pos.y < 0 || pos.y >= (*gridRef)[0].size()) {
            return false;
        }
        return (*gridRef)[pos.x][pos.y].isTraversable();
    }
    
    vector<vector<Cell>> getEnvironmentGrid() override {
        return *gridRef;
    }
};

class FireBrigade : public Agent {
private:
    vector<vector<Cell>>* gridRef;
    
public:
    FireBrigade(Position pos, vector<vector<Cell>>& grid) 
        : Agent(AgentType::FIRE_BRIGADE, pos), gridRef(&grid) {}
    
    double calculateBid(shared_ptr<Task> task) override {
        if (!hasSufficientResources()) return 9999.0;
        
        switch(task->type) {
            case TaskType::EXTINGUISH_FIRE:
                return task->calculateCost(position, type);
            default:
                return 9999.0;
        }
    }
    
    void performTask(vector<vector<Cell>>& grid) override {
        if (!currentTask) return;
        
        Cell& cell = grid[position.x][position.y];
        
        if (cell.type == CellType::FIRE) {
            cell.fireIntensity = max(0, cell.fireIntensity - 25);
            water = max(0, water - 10);
            
            if (cell.fireIntensity == 0) {
                cell.type = CellType::BUILDING;
                cell.smokeLevel = 2; // Residual smoke
            }
        }
    }

protected:
    bool isValidMove(const Position& pos) override {
        if (pos.x < 0 || pos.x >= gridRef->size() || pos.y < 0 || pos.y >= (*gridRef)[0].size()) {
            return false;
        }
        return (*gridRef)[pos.x][pos.y].isTraversable();
    }
    
    vector<vector<Cell>> getEnvironmentGrid() override {
        return *gridRef;
    }
};

class Ambulance : public Agent {
private:
    vector<vector<Cell>>* gridRef;
    bool transportingVictim;
    Position hospitalTarget;
    
public:
    Ambulance(Position pos, vector<vector<Cell>>& grid) 
        : Agent(AgentType::AMBULANCE, pos), gridRef(&grid), transportingVictim(false) {}
    
    double calculateBid(shared_ptr<Task> task) override {
        if (!hasSufficientResources() || transportingVictim) return 9999.0;
        
        switch(task->type) {
            case TaskType::TRANSPORT_TO_HOSPITAL:
                return task->calculateCost(position, type);
            default:
                return 9999.0;
        }
    }
    
    void performTask(vector<vector<Cell>>& grid) override {
        if (!currentTask) return;
        
        Cell& cell = grid[position.x][position.y];
        
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
                    path = AStar::findPath(position, hospitalTarget, getEnvironmentGrid());
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
    
    void completeTask() override {
        if (transportingVictim && position == hospitalTarget) {
            transportingVictim = false;
        }
        
        Agent::completeTask();
    }
    
    string getStatus() const override {
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
        
        for (int x = 0; x < gridRef->size(); x++) {
            for (int y = 0; y < (*gridRef)[0].size(); y++) {
                if ((*gridRef)[x][y].type == CellType::HOSPITAL) {
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

protected:
    bool isValidMove(const Position& pos) override {
        if (pos.x < 0 || pos.x >= gridRef->size() || pos.y < 0 || pos.y >= (*gridRef)[0].size()) {
            return false;
        }
        return (*gridRef)[pos.x][pos.y].isTraversable();
    }
    
    vector<vector<Cell>> getEnvironmentGrid() override {
        return *gridRef;
    }
};

// ==================== SIMULATION ENGINE ====================
class DisasterSimulation {
private:
    vector<vector<Cell>> grid;
    vector<shared_ptr<Agent>> agents;
    vector<shared_ptr<Task>> tasks;
    int stepCount;
    random_device rd;
    mt19937 gen;
    
    // Statistics
    int totalVictims;
    int rescuedVictims;
    int deceasedVictims;
    int extinguishedFires;
    int clearedDebris;
    int tasksCompleted;
    
public:
    DisasterSimulation(int size = GRID_SIZE) 
        : grid(size, vector<Cell>(size)), stepCount(0), gen(rd()),
          totalVictims(0), rescuedVictims(0), deceasedVictims(0), 
          extinguishedFires(0), clearedDebris(0), tasksCompleted(0) {
        
        initializeCity();
        initializeAgents();
        initializeDisaster();
    }
    
    void initializeCity() {
        // Initialize grid
        for (int x = 0; x < GRID_SIZE; x++) {
            for (int y = 0; y < GRID_SIZE; y++) {
                grid[x][y] = Cell(x, y);
            }
        }
        
        createCityLayout();
    }
    
    void createCityLayout() {
        // Create roads (every 3rd row and column)
        for (int i = 0; i < GRID_SIZE; i++) {
            for (int j = 0; j < GRID_SIZE; j++) {
                if (i % 3 == 0 || j % 3 == 0) {
                    grid[i][j].type = CellType::ROAD;
                }
            }
        }
        
        // Place hospitals at strategic locations
        vector<Position> hospitalPositions = {
            {2, 2}, {2, GRID_SIZE-3}, {GRID_SIZE-3, 2}, {GRID_SIZE-3, GRID_SIZE-3},
            {GRID_SIZE/2, GRID_SIZE/2}
        };
        
        for (const auto& pos : hospitalPositions) {
            if (pos.x < GRID_SIZE && pos.y < GRID_SIZE) {
                grid[pos.x][pos.y].type = CellType::HOSPITAL;
            }
        }
        
        // Create residential and commercial blocks
        for (int x = 1; x < GRID_SIZE-1; x += 4) {
            for (int y = 1; y < GRID_SIZE-1; y += 4) {
                // 2x2 building blocks
                for (int dx = 0; dx < 2; dx++) {
                    for (int dy = 0; dy < 2; dy++) {
                        int nx = x + dx, ny = y + dy;
                        if (nx < GRID_SIZE-1 && ny < GRID_SIZE-1 && 
                            grid[nx][ny].type != CellType::ROAD) {
                            grid[nx][ny].type = CellType::BUILDING;
                        }
                    }
                }
            }
        }
    }
    
    void initializeDisaster() {
        uniform_int_distribution<> dis(0, GRID_SIZE-1);
        
        // Set initial fires in random buildings
        for (int i = 0; i < 8; i++) {
            int x = dis(gen), y = dis(gen);
            if (grid[x][y].type == CellType::BUILDING) {
                grid[x][y].type = CellType::FIRE;
                grid[x][y].fireIntensity = 30 + dis(gen) % 50;
            }
        }
        
        // Place victims in buildings
        for (int i = 0; i < 15; i++) {
            int x = dis(gen), y = dis(gen);
            if (grid[x][y].type == CellType::BUILDING || grid[x][y].type == CellType::FIRE) {
                grid[x][y].hasVictim = true;
                grid[x][y].victimHealth = 30 + dis(gen) % 60;
                totalVictims++;
            }
        }
        
        // Add debris on some roads
        for (int i = 0; i < 10; i++) {
            int x = dis(gen), y = dis(gen);
            if (grid[x][y].type == CellType::ROAD) {
                grid[x][y].type = CellType::DEBRIS;
            }
        }
    }
    
    void initializeAgents() {
        // Strategic starting positions
        vector<Position> startPositions = {
            {1, 1}, {1, GRID_SIZE-2}, {GRID_SIZE-2, 1}, {GRID_SIZE-2, GRID_SIZE-2},
            {GRID_SIZE/2, 1}, {1, GRID_SIZE/2}, {GRID_SIZE-2, GRID_SIZE/2}, {GRID_SIZE/2, GRID_SIZE-2}
        };
        
        // Create diverse agent team
        agents.push_back(make_shared<RescueTeam>(startPositions[0], grid));
        agents.push_back(make_shared<RescueTeam>(startPositions[1], grid));
        agents.push_back(make_shared<RescueTeam>(startPositions[2], grid));
        
        agents.push_back(make_shared<FireBrigade>(startPositions[3], grid));
        agents.push_back(make_shared<FireBrigade>(startPositions[4], grid));
        agents.push_back(make_shared<FireBrigade>(startPositions[5], grid));
        
        agents.push_back(make_shared<Ambulance>(startPositions[6], grid));
        agents.push_back(make_shared<Ambulance>(startPositions[7], grid));
    }
    
    void spreadFire() {
        vector<Position> newFires;
        
        for (int x = 0; x < GRID_SIZE; x++) {
            for (int y = 0; y < GRID_SIZE; y++) {
                if (grid[x][y].type == CellType::FIRE && grid[x][y].fireIntensity > 40) {
                    // Chance to spread to adjacent cells
                    for (const auto& neighbor : grid[x][y].pos.getNeighbors()) {
                        if (neighbor.x >= 0 && neighbor.x < GRID_SIZE && 
                            neighbor.y >= 0 && neighbor.y < GRID_SIZE) {
                            
                            Cell& neighborCell = grid[neighbor.x][neighbor.y];
                            if ((neighborCell.type == CellType::BUILDING || 
                                 neighborCell.type == CellType::ROAD) &&
                                uniform_real_distribution<>(0, 1)(gen) < 0.15) {
                                
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
                grid[pos.x][pos.y].fireIntensity = 20;
            }
        }
    }
    
    void updateVictims() {
        for (int x = 0; x < GRID_SIZE; x++) {
            for (int y = 0; y < GRID_SIZE; y++) {
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
        for (int x = 0; x < GRID_SIZE; x++) {
            for (int y = 0; y < GRID_SIZE; y++) {
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
            
            if (bestAgent && bestBid < 100.0) { // Reasonable bid threshold
                bestAgent->assignTask(task);
                tasksCompleted++;
            }
        }
    }
    
    void updateAgents() {
        for (auto& agent : agents) {
            agent->update();
            
            // Update statistics based on agent actions
            if (agent->getCurrentTask()) {
                auto task = agent->getCurrentTask();
                if (task->type == TaskType::EXTINGUISH_FIRE && 
                    grid[task->position.x][task->position.y].fireIntensity == 0) {
                    extinguishedFires++;
                } else if (task->type == TaskType::RESCUE_VICTIM &&
                          grid[task->position.x][task->position.y].victimRescued) {
                    rescuedVictims++;
                } else if (task->type == TaskType::CLEAR_DEBRIS &&
                          grid[task->position.x][task->position.y].type != CellType::DEBRIS) {
                    clearedDebris++;
                }
            }
        }
    }
    
    void updateStatistics() {
        // Count current state
        int currentFires = 0, currentVictims = 0, currentRescued = 0;
        for (int x = 0; x < GRID_SIZE; x++) {
            for (int y = 0; y < GRID_SIZE; y++) {
                if (grid[x][y].type == CellType::FIRE) currentFires++;
                if (grid[x][y].hasVictim) {
                    if (grid[x][y].victimRescued) currentRescued++;
                    else currentVictims++;
                }
            }
        }
        
        // Update totals
        rescuedVictims = currentRescued;
    }
    
    void update() {
        stepCount++;
        
        // 1. Environment evolution
        for (int x = 0; x < GRID_SIZE; x++) {
            for (int y = 0; y < GRID_SIZE; y++) {
                grid[x][y].update();
            }
        }
        
        // 2. Disaster spread
        if (stepCount % 2 == 0) {
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
        
        // Header
        ConsoleUtils::setColor(BRIGHT_WHITE);
        cout << "🚨 DISASTER RESPONSE SIMULATION 🚨\n";
        cout << "==============================================\n";
        cout << "Step: " << stepCount << " | Time: " << stepCount << " minutes\n\n";
        
        // City Map
        cout << "CITY MAP:\n";
        cout << "┌";
        for (int x = 0; x < GRID_SIZE; x++) cout << "──";
        cout << "┐\n";
        
        for (int y = 0; y < GRID_SIZE; y++) {
            cout << "│";
            for (int x = 0; x < GRID_SIZE; x++) {
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
        for (int x = 0; x < GRID_SIZE; x++) cout << "──";
        cout << "┘\n";
        
        // Legend
        ConsoleUtils::setColor(BRIGHT_WHITE);
        cout << "\nLEGEND: ";
        ConsoleUtils::setColor(BRIGHT_YELLOW); cout << "R"; ConsoleUtils::setColor(WHITE); cout << "=Rescue ";
        ConsoleUtils::setColor(BRIGHT_RED); cout << "F"; ConsoleUtils::setColor(WHITE); cout << "=Fire ";
        ConsoleUtils::setColor(BRIGHT_CYAN); cout << "A"; ConsoleUtils::setColor(WHITE); cout << "=Ambulance ";
        ConsoleUtils::setColor(BRIGHT_MAGENTA); cout << "V"; ConsoleUtils::setColor(WHITE); cout << "=Victim ";
        ConsoleUtils::setColor(RED); cout << "F/f/w"; ConsoleUtils::setColor(WHITE); cout << "=Fire ";
        ConsoleUtils::setColor(BRIGHT_GREEN); cout << "H"; ConsoleUtils::setColor(WHITE); cout << "=Hospital ";
        ConsoleUtils::setColor(YELLOW); cout << "D"; ConsoleUtils::setColor(WHITE); cout << "=Debris\n";
        
        // Agent Status
        ConsoleUtils::setColor(BRIGHT_WHITE);
        cout << "\nAGENT STATUS:\n";
        for (const auto& agent : agents) {
            ConsoleUtils::setColor(agent->getColor());
            cout << "  " << agent->getStatus() << "\n";
        }
        
        // Tasks
        ConsoleUtils::setColor(BRIGHT_WHITE);
        cout << "\nACTIVE TASKS: " << tasks.size() << "\n";
        for (const auto& task : tasks) {
            if (!task->assigned) {
                ConsoleUtils::setColor(BRIGHT_RED);
                cout << "  " << task->getDisplayName() << " @(" << task->position.x 
                     << "," << task->position.y << ") Urgency: " << task->urgency << "\n";
            } else {
                ConsoleUtils::setColor(BRIGHT_YELLOW);
                cout << "  " << task->getDisplayName() << " @(" << task->position.x 
                     << "," << task->position.y << ") -> " << task->assignedAgent << "\n";
            }
        }
        
        // Statistics
        ConsoleUtils::setColor(BRIGHT_WHITE);
        cout << "\n📊 MISSION STATISTICS:\n";
        ConsoleUtils::setColor(BRIGHT_GREEN);
        cout << "  Victims Rescued: " << rescuedVictims << "/" << totalVictims << "\n";
        ConsoleUtils::setColor(BRIGHT_RED);
        cout << "  Victims Deceased: " << deceasedVictims << "\n";
        ConsoleUtils::setColor(BRIGHT_YELLOW);
        cout << "  Fires Extinguished: " << extinguishedFires << "\n";
        ConsoleUtils::setColor(BRIGHT_CYAN);
        cout << "  Debris Cleared: " << clearedDebris << "\n";
        ConsoleUtils::setColor(BRIGHT_MAGENTA);
        cout << "  Tasks Completed: " << tasksCompleted << "\n";
        
        int activeFires = 0, waitingVictims = 0;
        for (int x = 0; x < GRID_SIZE; x++) {
            for (int y = 0; y < GRID_SIZE; y++) {
                if (grid[x][y].type == CellType::FIRE) activeFires++;
                if (grid[x][y].hasVictim && !grid[x][y].victimRescued) waitingVictims++;
            }
        }
        
        ConsoleUtils::setColor(activeFires > 0 ? BRIGHT_RED : BRIGHT_GREEN);
        cout << "  Active Fires: " << activeFires << "\n";
        ConsoleUtils::setColor(waitingVictims > 0 ? BRIGHT_MAGENTA : BRIGHT_GREEN);
        cout << "  Waiting Victims: " << waitingVictims << "\n";
        
        // Performance
        double successRate = totalVictims > 0 ? (rescuedVictims * 100.0 / totalVictims) : 0;
        ConsoleUtils::setColor(successRate > 70 ? BRIGHT_GREEN : successRate > 40 ? BRIGHT_YELLOW : BRIGHT_RED);
        cout << "  Success Rate: " << fixed << setprecision(1) << successRate << "%\n";
        
        ConsoleUtils::setColor(BRIGHT_WHITE);
        cout << "\nPress Ctrl+C to stop | Next update in 1 second...\n";
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
        ConsoleUtils::setColor(BRIGHT_WHITE);
        cout << "Initializing Disaster Response Simulation...\n";
        cout << "City Size: " << GRID_SIZE << "x" << GRID_SIZE << "\n";
        cout << "Agents: 3 Rescue Teams, 3 Fire Brigades, 2 Ambulances\n";
        cout << "Features: A* Pathfinding, Contract Net Protocol, Real-time Simulation\n";
        cout << "Starting in 3 seconds...\n";
        
        ConsoleUtils::sleep(3000);
        
        DisasterSimulation simulation;
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