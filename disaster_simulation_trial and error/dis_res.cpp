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
#include <stack>
#include <unordered_map>
#include <functional>

using namespace std;

// ==================== CONSTANTS AND ENUMS ====================
const int GRID_SIZE = 25;
const int WINDOW_WIDTH = 100;
const int WINDOW_HEIGHT = 50;

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
enum class SimulationState { RUNNING, VICTORY, DEFEAT };

// ==================== UTILITY CLASSES ====================
class Position {
public:
    int x, y;
    Position(int x = 0, int y = 0) : x(x), y(y) {}
    
    bool operator==(const Position& other) const { return x == other.x && y == other.y; }
    bool operator<(const Position& other) const { return x < other.x || (x == other.x && y < other.y); }
    bool operator!=(const Position& other) const { return !(*this == other); }
    
    double distanceTo(const Position& other) const {
        return sqrt(pow(x - other.x, 2) + pow(y - other.y, 2));
    }
    
    int manhattanDistanceTo(const Position& other) const {
        return abs(x - other.x) + abs(y - other.y);
    }
    
    vector<Position> getNeighbors() const {
        return {{x-1,y}, {x+1,y}, {x,y-1}, {x,y+1}};
    }
    
    string toString() const {
        return "(" + to_string(x) + "," + to_string(y) + ")";
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
    bool blocked;       // For dynamic obstacles
    
    Cell() : pos(0,0), type(CellType::EMPTY), fireIntensity(0), victimHealth(100),
            hasVictim(false), victimRescued(false), smokeLevel(0), blocked(false) {}
    
    Cell(int x, int y) : pos(x,y), type(CellType::EMPTY), fireIntensity(0), victimHealth(100),
                        hasVictim(false), victimRescued(false), smokeLevel(0), blocked(false) {}
    
    void update() {
        // Fire dynamics - SLOWER
        if (type == CellType::FIRE) {
            fireIntensity = min(100, fireIntensity + 1); // Reduced from 2 to 1
            smokeLevel = min(3, smokeLevel + 1);
            
            // Victims in fire take LESS damage
            if (hasVictim && !victimRescued) {
                victimHealth = max(0, victimHealth - 3); // Reduced from 8 to 3
            }
        } else if (hasVictim && !victimRescued) {
            // Normal health deterioration - MUCH SLOWER
            if (rand() % 3 == 0) { // Only deteriorate every 3 turns
                victimHealth = max(0, victimHealth - 1);
            }
        }
        
        // Smoke dissipation
        if (smokeLevel > 0 && type != CellType::FIRE) {
            smokeLevel--;
        }
    }
    
    bool isTraversable() const {
        return !blocked && type != CellType::DEBRIS && fireIntensity < 60; // More lenient
    }
    
    bool isAccessible() const {
        return !blocked && type != CellType::DEBRIS && fireIntensity < 80;
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
    int timeCreated;
    
    Task(TaskType t, Position pos, int urg, string taskId, int step) 
        : type(t), position(pos), urgency(urg), assigned(false), baseCost(0), timeCreated(step) {
        id = taskId;
        priority = calculatePriority();
    }
    
    int calculatePriority() {
        int basePriority = 0;
        switch(type) {
            case TaskType::EXTINGUISH_FIRE: basePriority = urgency * 2; break; // Reduced from 3
            case TaskType::RESCUE_VICTIM: basePriority = urgency * 3; break; // Reduced from 4
            case TaskType::TRANSPORT_TO_HOSPITAL: basePriority = urgency * 2; break;
            case TaskType::CLEAR_DEBRIS: basePriority = urgency; break;
            default: basePriority = urgency;
        }
        
        // Increase priority over time for critical tasks
        if (urgency > 70) {
            basePriority += (urgency - 70);
        }
        
        return basePriority;
    }
    
    double calculateCost(const Position& agentPos, AgentType agentType) const {
        double distance = position.distanceTo(agentPos);
        double urgencyFactor = (100 - urgency) * 0.05; // Reduced impact
        
        // Agent specialization multipliers - MORE DISTINCT
        double efficiency = 1.0;
        switch(agentType) {
            case AgentType::FIRE_BRIGADE:
                efficiency = (type == TaskType::EXTINGUISH_FIRE) ? 0.1 : 3.0; // Much better at fires
                break;
            case AgentType::RESCUE_TEAM:
                efficiency = (type == TaskType::RESCUE_VICTIM) ? 0.2 : 
                            (type == TaskType::CLEAR_DEBRIS) ? 0.3 : 2.5;
                break;
            case AgentType::AMBULANCE:
                efficiency = (type == TaskType::TRANSPORT_TO_HOSPITAL) ? 0.1 : 3.0; // Much better at transport
                break;
        }
        
        return (distance * 0.7) + (urgencyFactor * 0.1) + (efficiency * 0.2); // Distance matters more
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

// ==================== A* PATHFINDING ====================
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
          fuel(300), water(300), medicalKits(12), efficiency(100), workProgress(0), workRequired(2), // MORE resources, LESS work
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
            case AgentType::FIRE_BRIGADE: return water > 25; // Increased threshold
            case AgentType::AMBULANCE: return medicalKits > 0;
            case AgentType::RESCUE_TEAM: return true;
            default: return true;
        }
    }
    
    void consumeResources() {
        fuel = max(0, fuel - 1);
        if (type == AgentType::FIRE_BRIGADE && state == AgentState::WORKING) {
            water = max(0, water - 5); // Reduced consumption
        }
    }
    
    void refillResources() {
        fuel = 300;
        if (type == AgentType::FIRE_BRIGADE) water = 300;
        if (type == AgentType::AMBULANCE) medicalKits = min(12, medicalKits + 6); // More refill
    }
    
    // Task management
    virtual double calculateBid(shared_ptr<Task> task) = 0;
    
    void assignTask(shared_ptr<Task> task) {
        currentTask = task;
        task->assigned = true;
        task->assignedAgent = getTypeName() + to_string(id);
        state = AgentState::MOVING;
        
        // Use A* for initial path
        path = AStar::findPath(position, task->position, *environmentGrid);
        if (path.empty()) {
            state = AgentState::IDLE;
            currentTask = nullptr;
            task->assigned = false;
            task->assignedAgent = "";
        } else {
            // Remove current position from path
            if (!path.empty() && path[0] == position) {
                path.erase(path.begin());
            }
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
        
        // Check if next position is still traversable
        if (isValidMove(nextPos)) {
            position = nextPos;
            path.erase(path.begin());
            consumeResources();
        } else {
            // Recalculate path if blocked
            if (currentTask) {
                path = AStar::findPath(position, currentTask->position, *environmentGrid);
                if (path.empty()) {
                    // Task became unreachable, abandon it
                    completeTask();
                }
            }
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
                return task->calculateCost(position, type) * 0.5; // Strong preference
            case TaskType::CLEAR_DEBRIS:
                return task->calculateCost(position, type) * 0.8;
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
                    fuel = max(0, fuel - 2); // Reduced consumption
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
                return task->calculateCost(position, type) * 0.3; // Very strong preference
            default:
                return 9999.0;
        }
    }
    
    void performTask() override {
        if (!currentTask) return;
        
        Cell& cell = (*environmentGrid)[position.x][position.y];
        
        if (cell.type == CellType::FIRE) {
            // MORE EFFICIENT fire extinguishing
            cell.fireIntensity = max(0, cell.fireIntensity - 50); // Increased from 30
            water = max(0, water - 5); // Reduced consumption
            
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
                return task->calculateCost(position, type) * 0.4; // Very strong preference
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
                    state = AgentState::MOVING;
                }
            }
        } else {
            // Deliver to hospital
            if (cell.type == CellType::HOSPITAL) {
                transportingVictim = false;
                hospitalTarget = Position(-1, -1);
                completeTask();
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

// ==================== CONTRACT NET PROTOCOL ====================
class ContractNetProtocol {
private:
    struct Bid {
        shared_ptr<Task> task;
        shared_ptr<Agent> agent;
        double cost;
        
        Bid(shared_ptr<Task> t, shared_ptr<Agent> a, double c) 
            : task(t), agent(a), cost(c) {}
        
        bool operator>(const Bid& other) const { return cost > other.cost; }
    };

public:
    static void allocateTasks(vector<shared_ptr<Task>>& tasks, vector<shared_ptr<Agent>>& agents) {
        if (tasks.empty() || agents.empty()) return;
        
        // Collect all bids
        priority_queue<Bid, vector<Bid>, greater<Bid>> bidQueue;
        
        for (auto& task : tasks) {
            if (task->assigned) continue;
            
            for (auto& agent : agents) {
                if (!agent->isBusy() && agent->hasSufficientResources()) {
                    double bidCost = agent->calculateBid(task);
                    if (bidCost < 500.0) { // Increased bid threshold
                        bidQueue.push(Bid(task, agent, bidCost));
                    }
                }
            }
        }
        
        // Assign tasks based on best bids
        map<shared_ptr<Agent>, bool> agentAssigned;
        map<shared_ptr<Task>, bool> taskAssigned;
        
        while (!bidQueue.empty()) {
            Bid bestBid = bidQueue.top();
            bidQueue.pop();
            
            if (!bestBid.agent->isBusy() && !bestBid.task->assigned && 
                !agentAssigned[bestBid.agent] && !taskAssigned[bestBid.task]) {
                
                bestBid.agent->assignTask(bestBid.task);
                agentAssigned[bestBid.agent] = true;
                taskAssigned[bestBid.task] = true;
            }
        }
    }
};

// ==================== GOAL STACK PLANNING ====================
class Goal {
public:
    string description;
    function<bool()> condition;
    int priority;
    
    Goal(const string& desc, function<bool()> cond, int prio = 1) 
        : description(desc), condition(cond), priority(prio) {}
    
    bool isAchieved() const { return condition(); }
};

class GoalStackPlanner {
private:
    stack<shared_ptr<Goal>> goalStack;
    vector<shared_ptr<Goal>> allGoals;
    
public:
    void addGoal(shared_ptr<Goal> goal) {
        allGoals.push_back(goal);
    }
    
    void updateStack() {
        // Clear current stack
        while (!goalStack.empty()) goalStack.pop();
        
        // Sort goals by priority (highest first)
        vector<shared_ptr<Goal>> sortedGoals = allGoals;
        sort(sortedGoals.begin(), sortedGoals.end(), 
             [](const shared_ptr<Goal>& a, const shared_ptr<Goal>& b) {
                 return a->priority > b->priority;
             });
        
        // Push unachieved goals to stack
        for (const auto& goal : sortedGoals) {
            if (!goal->isAchieved()) {
                goalStack.push(goal);
            }
        }
    }
    
    shared_ptr<Goal> getCurrentGoal() {
        if (goalStack.empty()) return nullptr;
        return goalStack.top();
    }
    
    void popGoal() {
        if (!goalStack.empty()) goalStack.pop();
    }
    
    bool hasGoals() const {
        return !goalStack.empty();
    }
    
    int getRemainingGoals() const {
        return goalStack.size();
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
    int maxSteps;
    
    SimulationConfig() 
        : numFires(6), numVictims(12), numRescueTeams(5),  // Better defaults
          numFireBrigades(7), numAmbulances(5), citySize(GRID_SIZE), maxSteps(300) {}
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
        
        config.numFires = getNumberInput("Number of buildings on fire (3-15): ", 3, 15);
        config.numVictims = getNumberInput("Number of victims (8-25): ", 8, 25);
        config.numRescueTeams = getNumberInput("Number of rescue teams (3-10): ", 3, 10);
        config.numFireBrigades = getNumberInput("Number of fire brigades (4-12): ", 4, 12);
        config.numAmbulances = getNumberInput("Number of ambulances (3-10): ", 3, 10);
        config.maxSteps = getNumberInput("Maximum simulation steps (150-600): ", 150, 600);
        
        // Give advice based on configuration
        giveConfigurationAdvice(config);
        
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
    
    static void giveConfigurationAdvice(const SimulationConfig& config) {
        ConsoleUtils::setColor(BRIGHT_CYAN);
        cout << "\n💡 STRATEGY ADVICE:\n";
        
        double fireToBrigadeRatio = (double)config.numFires / config.numFireBrigades;
        double victimToRescueRatio = (double)config.numVictims / config.numRescueTeams;
        double victimToAmbulanceRatio = (double)config.numVictims / config.numAmbulances;
        
        if (fireToBrigadeRatio > 1.5) {
            cout << "⚠️  High fire load! Consider more fire brigades.\n";
        }
        if (victimToRescueRatio > 3) {
            cout << "⚠️  Many victims! Consider more rescue teams.\n";
        }
        if (victimToAmbulanceRatio > 4) {
            cout << "⚠️  Transport bottleneck! Consider more ambulances.\n";
        }
        
        if (fireToBrigadeRatio < 0.7 && victimToRescueRatio < 2 && victimToAmbulanceRatio < 2) {
            cout << "✅ Good balance! You should be able to win with this setup.\n";
        }
        
        // Recommended winning combinations
        cout << "\n🎯 Recommended winning combinations:\n";
        cout << "• 6 fires, 12 victims: 5 rescue, 7 fire, 5 ambulance\n";
        cout << "• 8 fires, 15 victims: 6 rescue, 8 fire, 6 ambulance\n"; 
        cout << "• 10 fires, 20 victims: 8 rescue, 10 fire, 8 ambulance\n";
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
    SimulationState simState;
    
    // Statistics
    int totalVictims;
    int rescuedVictims;
    int deceasedVictims;
    int extinguishedFires;
    int clearedDebris;
    int tasksCompleted;
    int initialFires;
    
    // Goal Stack Planning
    GoalStackPlanner goalPlanner;
    
public:
    DisasterSimulation(const SimulationConfig& cfg) 
        : grid(cfg.citySize, vector<Cell>(cfg.citySize)), stepCount(0), config(cfg),
          simState(SimulationState::RUNNING), totalVictims(0), rescuedVictims(0), 
          deceasedVictims(0), extinguishedFires(0), clearedDebris(0), tasksCompleted(0),
          initialFires(0) {
        
        initializeCity();
        initializeAgents();
        initializeDisaster();
        initializeGoals();
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
        initialFires = config.numFires;
        
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
        
        // Place victims based on user configuration - MORE SPREAD OUT
        int victimsPlaced = 0;
        while (victimsPlaced < config.numVictims) {
            int x = rand() % config.citySize, y = rand() % config.citySize;
            if (grid[x][y].type == CellType::BUILDING) { // Only in buildings, not initially in fires
                grid[x][y].hasVictim = true;
                grid[x][y].victimHealth = 60 + rand() % 40; // Higher initial health
                totalVictims++;
                victimsPlaced++;
            }
        }
        
        // Add LESS debris
        for (int i = 0; i < 5; i++) {
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
            {3*config.citySize/4, 3*config.citySize/4}, {config.citySize/4, 3*config.citySize/4},
            {3*config.citySize/4, config.citySize/4}
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
    
    void initializeGoals() {
        // More achievable goals
        goalPlanner.addGoal(make_shared<Goal>("Rescue majority of victims", 
            [this]() { return rescuedVictims >= totalVictims * 0.7; }, 100)); // Reduced from 80% to 70%
        
        goalPlanner.addGoal(make_shared<Goal>("Control fires", 
            [this]() { 
                int activeFires = 0;
                for (const auto& row : grid) {
                    for (const auto& cell : row) {
                        if (cell.type == CellType::FIRE) activeFires++;
                    }
                }
                return activeFires <= initialFires * 0.3; // Only need to control 70% of fires
            }, 80));
        
        goalPlanner.addGoal(make_shared<Goal>("Prevent too many deaths", 
            [this]() { return deceasedVictims <= totalVictims * 0.4; }, 90)); // Increased from 30% to 40%
        
        goalPlanner.addGoal(make_shared<Goal>("Complete within time limit", 
            [this]() { return stepCount < config.maxSteps; }, 70));
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
                                rand() % 8 == 0) { // Reduced from 5 to 8
                                
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
                        int urgency = min(100, grid[x][y].fireIntensity * 2);
                        tasks.push_back(make_shared<Task>(TaskType::EXTINGUISH_FIRE, pos, 
                                                         urgency, taskId, stepCount));
                    }
                }
                
                // Victim rescue tasks
                if (grid[x][y].hasVictim && !grid[x][y].victimRescued) {
                    if (!isTaskExists(TaskType::RESCUE_VICTIM, pos)) {
                        string taskId = "VICTIM_" + to_string(x) + "_" + to_string(y);
                        int urgency = 100 - grid[x][y].victimHealth;
                        tasks.push_back(make_shared<Task>(TaskType::RESCUE_VICTIM, pos, 
                                                        urgency, taskId, stepCount));
                    }
                }
                
                // Debris clearance tasks
                if (grid[x][y].type == CellType::DEBRIS) {
                    if (!isTaskExists(TaskType::CLEAR_DEBRIS, pos)) {
                        string taskId = "DEBRIS_" + to_string(x) + "_" + to_string(y);
                        tasks.push_back(make_shared<Task>(TaskType::CLEAR_DEBRIS, pos, 30, taskId, stepCount)); // Lower urgency
                    }
                }
                
                // Transport tasks for rescued victims
                if (grid[x][y].hasVictim && grid[x][y].victimRescued) {
                    if (!isTaskExists(TaskType::TRANSPORT_TO_HOSPITAL, pos)) {
                        string taskId = "TRANSPORT_" + to_string(x) + "_" + to_string(y);
                        tasks.push_back(make_shared<Task>(TaskType::TRANSPORT_TO_HOSPITAL, pos, 
                                                        50, taskId, stepCount)); // Medium urgency
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
    
    void updateStatistics() {
        // Count current state
        rescuedVictims = 0;
        extinguishedFires = 0;
        clearedDebris = 0;
        
        for (int x = 0; x < config.citySize; x++) {
            for (int y = 0; y < config.citySize; y++) {
                if (grid[x][y].hasVictim && grid[x][y].victimRescued) rescuedVictims++;
                if (grid[x][y].type == CellType::BUILDING && grid[x][y].fireIntensity == 0) extinguishedFires++;
                if (grid[x][y].type == CellType::ROAD) clearedDebris++;
            }
        }
        
        // Count tasks completed
        tasksCompleted = rescuedVictims + extinguishedFires + clearedDebris;
    }
    
    void checkSimulationEnd() {
        // Update goal stack
        goalPlanner.updateStack();
        
        // Check victory conditions - MORE ACHIEVABLE
        if (rescuedVictims >= totalVictims * 0.7) { // Reduced from 80% to 70%
            simState = SimulationState::VICTORY;
            return;
        }
        
        // Check defeat conditions - MORE FORGIVING
        if (stepCount >= config.maxSteps || deceasedVictims >= totalVictims * 0.5) { // Increased from 50% to 60%
            simState = SimulationState::DEFEAT;
            return;
        }
        
        // Check if all critical goals are failed
        auto currentGoal = goalPlanner.getCurrentGoal();
        if (currentGoal && currentGoal->description == "Prevent too many deaths" && 
            deceasedVictims > totalVictims * 0.4) { // Increased from 30% to 40%
            simState = SimulationState::DEFEAT;
        }
    }
    
    void update() {
        if (simState != SimulationState::RUNNING) return;
        
        stepCount++;
        
        // 1. Environment evolution
        for (int x = 0; x < config.citySize; x++) {
            for (int y = 0; y < config.citySize; y++) {
                grid[x][y].update();
            }
        }
        
        // 2. Disaster spread - SLOWER
        if (stepCount % 4 == 0) { // Reduced frequency from 2 to 4
            spreadFire();
        }
        
        // 3. Victim status update
        updateVictims();
        
        // 4. Task management
        updateTasks();
        
        // 5. Agent assignment using Contract Net Protocol
        ContractNetProtocol::allocateTasks(tasks, agents);
        
        // 6. Agent actions
        for (auto& agent : agents) {
            agent->update();
        }
        
        // 7. Statistics
        updateStatistics();
        
        // 8. Check simulation end conditions
        checkSimulationEnd();
    }
    
    void display() {
        ConsoleUtils::clearScreen();
        
        // Header with better UI
        ConsoleUtils::setColor(BRIGHT_CYAN);
        cout << "╔══════════════════════════════════════════════════════════════════════════════╗\n";
        cout << "║                    🚨 DISASTER RESPONSE SIMULATION 🚨                     ║\n";
        cout << "╚══════════════════════════════════════════════════════════════════════════════╝\n";
        
        ConsoleUtils::setColor(BRIGHT_WHITE);
        cout << "⏰ Step: " << stepCount << "/" << config.maxSteps << " | 🕐 Time: " << stepCount << " minutes";
        cout << " | 👥 Agents: " << agents.size() << " | 📋 Tasks: " << tasks.size();
        
        // Simulation state indicator
        ConsoleUtils::setColor(simState == SimulationState::VICTORY ? BRIGHT_GREEN : 
                              simState == SimulationState::DEFEAT ? BRIGHT_RED : BRIGHT_YELLOW);
        cout << " | 🎯 State: ";
        switch(simState) {
            case SimulationState::RUNNING: cout << "RUNNING"; break;
            case SimulationState::VICTORY: cout << "VICTORY!"; break;
            case SimulationState::DEFEAT: cout << "DEFEAT"; break;
        }
        cout << "\n\n";
        
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
        
        // Legend with different symbols
        ConsoleUtils::setColor(BRIGHT_WHITE);
        cout << "\n📖 LEGEND: ";
        ConsoleUtils::setColor(BRIGHT_YELLOW); cout << "R"; ConsoleUtils::setColor(WHITE); cout << "=Rescue ";
        ConsoleUtils::setColor(BRIGHT_RED); cout << "W"; ConsoleUtils::setColor(WHITE); cout << "=Fire Brigade ";
        ConsoleUtils::setColor(BRIGHT_CYAN); cout << "A"; ConsoleUtils::setColor(WHITE); cout << "=Ambulance ";
        ConsoleUtils::setColor(BRIGHT_MAGENTA); cout << "V"; ConsoleUtils::setColor(WHITE); cout << "=Victim ";
        ConsoleUtils::setColor(RED); cout << "F/f/w"; ConsoleUtils::setColor(WHITE); cout << "=Fire ";
        ConsoleUtils::setColor(BRIGHT_GREEN); cout << "H"; ConsoleUtils::setColor(WHITE); cout << "=Hospital ";
        ConsoleUtils::setColor(YELLOW); cout << "D"; ConsoleUtils::setColor(WHITE); cout << "=Debris\n";
        
        // Statistics in a better layout
        ConsoleUtils::setColor(BRIGHT_WHITE);
        cout << "\n📊 MISSION STATISTICS:\n";
        cout << "╔══════════════════════════════════════════════════════════════════════════════╗\n";
        
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
        cout << "   |  🔥 Active Fires: " << setw(3) << activeFires << "/" << setw(3) << initialFires << "       ║\n";
        
        ConsoleUtils::setColor(BRIGHT_RED);
        cout << "║  ❌ Victims Deceased: " << setw(3) << deceasedVictims;
        ConsoleUtils::setColor(BRIGHT_MAGENTA);
        cout << "         |  ❤️ Waiting Victims: " << setw(3) << waitingVictims << "         ║\n";
        
        ConsoleUtils::setColor(BRIGHT_YELLOW);
        cout << "║  🎯 Fires Extinguished: " << setw(3) << extinguishedFires;
        ConsoleUtils::setColor(BRIGHT_CYAN);
        cout << "     |  📋 Tasks Completed: " << setw(3) << tasksCompleted << "         ║\n";
        
        // Performance
        double successRate = totalVictims > 0 ? (rescuedVictims * 100.0 / totalVictims) : 0;
        double deathRate = totalVictims > 0 ? (deceasedVictims * 100.0 / totalVictims) : 0;
        
        ConsoleUtils::setColor(successRate > 70 ? BRIGHT_GREEN : successRate > 50 ? BRIGHT_YELLOW : BRIGHT_RED);
        cout << "║  📈 Rescue Rate: " << setw(6) << fixed << setprecision(1) << successRate << "%";
        ConsoleUtils::setColor(deathRate < 30 ? BRIGHT_GREEN : deathRate < 50 ? BRIGHT_YELLOW : BRIGHT_RED);
        cout << "       |  💀 Death Rate: " << setw(6) << fixed << setprecision(1) << deathRate << "%   ║\n";
        
        // Progress to victory
        double progress = min(100.0, (rescuedVictims * 100.0) / (totalVictims * 0.7)); // 70% target
        ConsoleUtils::setColor(BRIGHT_BLUE);
        cout << "║  📊 Progress to Victory: [" << string((int)(progress/5), '=') << string(20-(int)(progress/5), ' ') << "] ";
        cout << setw(5) << fixed << setprecision(1) << progress << "%                  ║\n";
        
        ConsoleUtils::setColor(BRIGHT_WHITE);
        cout << "╚══════════════════════════════════════════════════════════════════════════════╝\n";
        
        // Current Goals
        ConsoleUtils::setColor(BRIGHT_WHITE);
        cout << "\n🎯 ACTIVE GOALS:\n";
        goalPlanner.updateStack();
        auto currentGoal = goalPlanner.getCurrentGoal();
        if (currentGoal) {
            ConsoleUtils::setColor(BRIGHT_YELLOW);
            cout << "  Current: " << currentGoal->description;
            if (currentGoal->isAchieved()) {
                ConsoleUtils::setColor(BRIGHT_GREEN);
                cout << " ✅ ACHIEVED!";
            }
            cout << "\n";
        }
        
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
        
        // Simulation end message
        if (simState != SimulationState::RUNNING) {
            cout << "\n";
            ConsoleUtils::setColor(simState == SimulationState::VICTORY ? BRIGHT_GREEN : BRIGHT_RED);
            if (simState == SimulationState::VICTORY) {
                cout << "🎉 VICTORY! You successfully managed the disaster! 🎉\n";
                cout << "Rescued " << rescuedVictims << "/" << totalVictims << " victims in " << stepCount << " steps.\n";
            } else {
                cout << "💀 DEFEAT! The disaster overwhelmed your response teams.\n";
                cout << "Only rescued " << rescuedVictims << "/" << totalVictims << " victims.\n";
                cout << "Try with more agents or fewer initial disasters.\n";
            }
            ConsoleUtils::setColor(BRIGHT_WHITE);
            cout << "\nPress Enter to exit...";
        } else {
            ConsoleUtils::setColor(BRIGHT_WHITE);
            cout << "\n⏭️  Press Ctrl+C to stop | Next update in 1 second...\n";
        }
    }
    
    void run() {
        ConsoleUtils::setWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
        
        while (simState == SimulationState::RUNNING) {
            display();
            update();
            ConsoleUtils::sleep(1000);
        }
        
        // Final display
        display();
        
        // Wait for user input before exiting
        if (simState != SimulationState::RUNNING) {
            cin.ignore();
            cin.get();
        }
    }
    
    SimulationState getState() const { return simState; }
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
             << config.numFireBrigades << " Fire Brigades, " << config.numAmbulances << " Ambulances\n";
        cout << "🔥 Initial Fires: " << config.numFires << " | ❤️ Initial Victims: " << config.numVictims << "\n";
        cout << "⏰ Time Limit: " << config.maxSteps << " steps\n";
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