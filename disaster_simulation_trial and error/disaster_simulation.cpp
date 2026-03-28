#include <iostream>
#include <vector>
#include <memory>
#include <queue>
#include <cmath>
#include <algorithm>
#include <random>
#include <string>
#include <windows.h>

using namespace std;

// Constants
const int GRID_SIZE = 20;

// Console Colors
enum ConsoleColor {
    BLACK = 0,
    BLUE = 1,
    GREEN = 2,
    CYAN = 3,
    RED = 4,
    MAGENTA = 5,
    YELLOW = 6,
    WHITE = 7,
    BRIGHT_BLUE = 9,
    BRIGHT_GREEN = 10,
    BRIGHT_CYAN = 11,
    BRIGHT_RED = 12,
    BRIGHT_MAGENTA = 13,
    BRIGHT_YELLOW = 14,
    BRIGHT_WHITE = 15
};

// Enum for cell types
enum class CellType {
    EMPTY,
    BUILDING_SAFE,
    BUILDING_FIRE,
    VICTIM,
    HOSPITAL,
    DEBRIS,
    FIRE_ZONE
};

// Enum for agent types
enum class AgentType {
    RESCUE_TEAM,
    FIRE_BRIGADE,
    AMBULANCE
};

// Enum for task types
enum class TaskType {
    EXTINGUISH_FIRE,
    RESCUE_VICTIM,
    TRANSPORT_TO_HOSPITAL,
    CLEAR_DEBRIS
};

// Forward declarations
class Agent;
class Task;
class Environment;
class Cell;
class Position;

// ==================== POSITION CLASS ====================
class Position {
public:
    int x, y;
    
    Position(int x = 0, int y = 0) : x(x), y(y) {}
    
    bool operator==(const Position& other) const {
        return x == other.x && y == other.y;
    }
    
    bool operator!=(const Position& other) const {
        return !(*this == other);
    }
    
    double distanceTo(const Position& other) const {
        return sqrt(pow(x - other.x, 2) + pow(y - other.y, 2));
    }
};

// ==================== CONSOLE UTILS ====================
class ConsoleUtils {
public:
    static void setColor(ConsoleColor color) {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleTextAttribute(hConsole, color);
    }
    
    static void resetColor() {
        setColor(WHITE);
    }
    
    static void clearScreen() {
        system("cls");
    }
    
    static void sleep(int milliseconds) {
        Sleep(milliseconds);
    }
};

// ==================== CELL CLASS ====================
class Cell {
public:
    int x, y;
    CellType type;
    bool hasVictim;
    bool victimRescued;
    int fireIntensity;
    int victimHealth;
    
    Cell() : x(0), y(0), type(CellType::EMPTY), hasVictim(false), victimRescued(false), 
             fireIntensity(0), victimHealth(100) {}
    
    Cell(int x, int y, CellType type = CellType::EMPTY) 
        : x(x), y(y), type(type), hasVictim(false), victimRescued(false), 
          fireIntensity(0), victimHealth(100) {}
    
    bool isTraversable() const {
        return type != CellType::DEBRIS && fireIntensity < 70;
    }
    
    void update() {
        if (type == CellType::BUILDING_FIRE || type == CellType::FIRE_ZONE) {
            if (fireIntensity < 100) {
                fireIntensity += 5;
            }
        }
        
        if (hasVictim && !victimRescued) {
            int healthLoss = (type == CellType::BUILDING_FIRE || type == CellType::FIRE_ZONE) ? 5 : 2;
            victimHealth = max(0, victimHealth - healthLoss);
        }
    }
    
    char getDisplayChar() const {
        if (hasVictim && !victimRescued) return 'V';
        
        switch(type) {
            case CellType::EMPTY: return ' ';
            case CellType::BUILDING_SAFE: return 'B';
            case CellType::BUILDING_FIRE: 
                return fireIntensity > 50 ? 'F' : 'f';
            case CellType::HOSPITAL: return 'H';
            case CellType::DEBRIS: return 'D';
            case CellType::FIRE_ZONE: 
                return fireIntensity > 50 ? 'F' : 'f';
            default: return '?';
        }
    }
    
    ConsoleColor getColor() const {
        if (hasVictim && !victimRescued) {
            if (victimHealth > 50) return BRIGHT_MAGENTA;
            if (victimHealth > 20) return MAGENTA;
            return RED;
        }
        
        switch(type) {
            case CellType::EMPTY: return BLACK;
            case CellType::BUILDING_SAFE: return BLUE;
            case CellType::BUILDING_FIRE: 
                return fireIntensity > 50 ? BRIGHT_RED : RED;
            case CellType::HOSPITAL: return BRIGHT_GREEN;
            case CellType::DEBRIS: return YELLOW;
            case CellType::FIRE_ZONE: 
                return fireIntensity > 50 ? BRIGHT_RED : RED;
            default: return WHITE;
        }
    }
};

// ==================== TASK CLASS ====================
class Task {
public:
    TaskType type;
    Position position;
    int urgency;
    string description;
    bool assigned;
    string assignedTo;
    
    Task(TaskType t, Position pos, int urg, string desc = "")
        : type(t), position(pos), urgency(urg), description(desc), 
          assigned(false), assignedTo("") {}
    
    double calculateCost(const Position& agentPos, AgentType agentType) const {
        double distance = position.distanceTo(agentPos);
        double urgencyFactor = (100 - urgency) * 0.1;
        
        double efficiency = 1.0;
        switch(agentType) {
            case AgentType::FIRE_BRIGADE:
                efficiency = (type == TaskType::EXTINGUISH_FIRE) ? 0.5 : 2.0;
                break;
            case AgentType::RESCUE_TEAM:
                efficiency = (type == TaskType::RESCUE_VICTIM || 
                             type == TaskType::CLEAR_DEBRIS) ? 0.5 : 2.0;
                break;
            case AgentType::AMBULANCE:
                efficiency = (type == TaskType::TRANSPORT_TO_HOSPITAL) ? 0.5 : 2.0;
                break;
        }
        
        return distance + urgencyFactor + efficiency;
    }
    
    string getTypeName() const {
        switch(type) {
            case TaskType::EXTINGUISH_FIRE: return "Extinguish Fire";
            case TaskType::RESCUE_VICTIM: return "Rescue Victim";
            case TaskType::TRANSPORT_TO_HOSPITAL: return "Transport to Hospital";
            case TaskType::CLEAR_DEBRIS: return "Clear Debris";
            default: return "Unknown Task";
        }
    }
};

// ==================== ENVIRONMENT CLASS ====================
class Environment {
public:
    vector<vector<Cell>> grid;
    vector<shared_ptr<Task>> tasks;
    
    Environment(int size) {
        grid.resize(size, vector<Cell>(size));
        for (int i = 0; i < size; ++i) {
            for (int j = 0; j < size; ++j) {
                grid[i][j] = Cell(i, j);
            }
        }
    }
    
    Cell& getCell(int x, int y) {
        if (x >= 0 && x < grid.size() && y >= 0 && y < grid[0].size()) {
            return grid[x][y];
        }
        static Cell invalidCell(-1, -1);
        return invalidCell;
    }
    
    void addTask(shared_ptr<Task> task) {
        tasks.push_back(task);
    }
    
    void removeTask(shared_ptr<Task> task) {
        for (auto it = tasks.begin(); it != tasks.end(); ++it) {
            if (*it == task) {
                tasks.erase(it);
                break;
            }
        }
    }
    
    int getGridSize() const { return grid.size(); }
};

// ==================== AGENT BASE CLASS ====================
class Agent {
protected:
    static int nextId;
    int id;
    AgentType type;
    Position position;
    vector<Position> path;
    int fuel;
    int water;
    int medicalKits;
    bool busy;
    shared_ptr<Task> currentTask;
    
public:
    Agent(AgentType t, Position startPos) 
        : id(nextId++), type(t), position(startPos),
          fuel(100), water(100), medicalKits(5), busy(false) {}
    
    virtual ~Agent() = default;
    
    int getId() const { return id; }
    AgentType getType() const { return type; }
    Position getPosition() const { return position; }
    bool isBusy() const { return busy; }
    
    string getTypeName() const {
        switch(type) {
            case AgentType::RESCUE_TEAM: return "Rescue";
            case AgentType::FIRE_BRIGADE: return "Fire";
            case AgentType::AMBULANCE: return "Ambulance";
            default: return "Unknown";
        }
    }
    
    void setPath(const vector<Position>& newPath) { path = newPath; }
    bool hasPath() const { return !path.empty(); }
    
    void assignTask(shared_ptr<Task> task) { 
        currentTask = task; 
        busy = true;
    }
    
    void completeTask() { 
        currentTask = nullptr; 
        busy = false;
    }
    
    virtual void move() {
        if (!path.empty()) {
            position = path.front();
            path.erase(path.begin());
            fuel = max(0, fuel - 1);
        }
    }
    
    virtual double calculateBid(shared_ptr<Task> task) = 0;
    virtual void performTask(Environment& env) = 0;
    
    bool hasSufficientResources() const {
        return fuel > 10 && 
               ((type == AgentType::FIRE_BRIGADE && water > 0) ||
                (type == AgentType::AMBULANCE && medicalKits > 0) ||
                (type == AgentType::RESCUE_TEAM));
    }
    
    void refillResources() {
        fuel = 100;
        if (type == AgentType::FIRE_BRIGADE) water = 100;
        if (type == AgentType::AMBULANCE) medicalKits = 5;
    }
    
    virtual char getDisplayChar() const = 0;
    virtual ConsoleColor getColor() const = 0;
    
    string getStatus() const {
        string status = getTypeName() + to_string(id) + " @(" + 
                       to_string(position.x) + "," + to_string(position.y) + ")";
        if (busy && currentTask) {
            status += " -> " + currentTask->getTypeName();
        } else if (!busy) {
            status += " [IDLE]";
        }
        status += " Fuel:" + to_string(fuel);
        if (type == AgentType::FIRE_BRIGADE) status += " Water:" + to_string(water);
        if (type == AgentType::AMBULANCE) status += " MedKits:" + to_string(medicalKits);
        return status;
    }
};

int Agent::nextId = 1;

// ==================== SPECIFIC AGENT CLASSES ====================
class RescueTeam : public Agent {
public:
    RescueTeam(Position startPos) : Agent(AgentType::RESCUE_TEAM, startPos) {}
    
    double calculateBid(shared_ptr<Task> task) override {
        if (!hasSufficientResources()) return 1000.0;
        
        switch(task->type) {
            case TaskType::RESCUE_VICTIM:
            case TaskType::CLEAR_DEBRIS:
                return task->calculateCost(position, type);
            default:
                return 1000.0;
        }
    }
    
    void performTask(Environment& env) override {
        if (!currentTask || !hasSufficientResources()) return;
        
        if (position == currentTask->position) {
            Cell& cell = env.getCell(position.x, position.y);
            
            switch(currentTask->type) {
                case TaskType::RESCUE_VICTIM:
                    if (cell.hasVictim && !cell.victimRescued) {
                        cell.victimRescued = true;
                        cout << "  RescueTeam " << id << " rescued victim at (" 
                             << position.x << "," << position.y << ")\n";
                    }
                    break;
                    
                case TaskType::CLEAR_DEBRIS:
                    if (cell.type == CellType::DEBRIS) {
                        cell.type = CellType::EMPTY;
                        cout << "  RescueTeam " << id << " cleared debris at (" 
                             << position.x << "," << position.y << ")\n";
                    }
                    break;
                    
                default:
                    break;
            }
            
            completeTask();
            env.removeTask(currentTask);
        }
    }
    
    char getDisplayChar() const override { return 'R'; }
    ConsoleColor getColor() const override { return BRIGHT_YELLOW; }
};

class FireBrigade : public Agent {
public:
    FireBrigade(Position startPos) : Agent(AgentType::FIRE_BRIGADE, startPos) {}
    
    double calculateBid(shared_ptr<Task> task) override {
        if (!hasSufficientResources()) return 1000.0;
        
        switch(task->type) {
            case TaskType::EXTINGUISH_FIRE:
                return task->calculateCost(position, type);
            default:
                return 1000.0;
        }
    }
    
    void performTask(Environment& env) override {
        if (!currentTask || !hasSufficientResources()) return;
        
        if (position == currentTask->position) {
            Cell& cell = env.getCell(position.x, position.y);
            
            if (cell.type == CellType::BUILDING_FIRE || cell.type == CellType::FIRE_ZONE) {
                cell.fireIntensity = max(0, cell.fireIntensity - 30);
                water = max(0, water - 10);
                
                cout << "  FireBrigade " << id << " fighting fire at (" 
                     << position.x << "," << position.y << ") - Intensity: " 
                     << cell.fireIntensity << "\n";
                
                if (cell.fireIntensity == 0) {
                    if (cell.type == CellType::BUILDING_FIRE) {
                        cell.type = CellType::BUILDING_SAFE;
                    } else {
                        cell.type = CellType::EMPTY;
                    }
                    cout << "  FireBrigade " << id << " extinguished fire at (" 
                         << position.x << "," << position.y << ")\n";
                    completeTask();
                    env.removeTask(currentTask);
                }
            }
        }
    }
    
    char getDisplayChar() const override { return 'F'; }
    ConsoleColor getColor() const override { return BRIGHT_RED; }
};

class Ambulance : public Agent {
private:
    bool transportingVictim;
    
public:
    Ambulance(Position startPos) : Agent(AgentType::AMBULANCE, startPos), 
                                  transportingVictim(false) {}
    
    double calculateBid(shared_ptr<Task> task) override {
        if (!hasSufficientResources()) return 1000.0;
        
        switch(task->type) {
            case TaskType::TRANSPORT_TO_HOSPITAL:
                return task->calculateCost(position, type);
            default:
                return 1000.0;
        }
    }
    
    void performTask(Environment& env) override {
        if (!currentTask || !hasSufficientResources()) return;
        
        Cell& cell = env.getCell(position.x, position.y);
        
        if (!transportingVictim && position == currentTask->position) {
            if (cell.hasVictim && cell.victimRescued && medicalKits > 0) {
                cell.hasVictim = false;
                cell.victimRescued = false;
                transportingVictim = true;
                medicalKits--;
                cout << "  Ambulance " << id << " picked up victim at (" 
                     << position.x << "," << position.y << ")\n";
            }
        } else if (transportingVictim) {
            // Find any hospital to deliver
            for (int x = 0; x < GRID_SIZE; x++) {
                for (int y = 0; y < GRID_SIZE; y++) {
                    if (env.getCell(x, y).type == CellType::HOSPITAL && 
                        position.x == x && position.y == y) {
                        transportingVictim = false;
                        cout << "  Ambulance " << id << " delivered victim to hospital at (" 
                             << position.x << "," << position.y << ")\n";
                        completeTask();
                        env.removeTask(currentTask);
                        return;
                    }
                }
            }
        }
    }
    
    char getDisplayChar() const override { return 'A'; }
    ConsoleColor getColor() const override { return BRIGHT_CYAN; }
};

// ==================== PATHFINDING ====================
class Pathfinding {
public:
    static vector<Position> aStar(const Position& start, const Position& goal, 
                                 const vector<vector<Cell>>& grid) {
        vector<Position> path;
        
        if (goal.x < 0 || goal.x >= grid.size() || 
            goal.y < 0 || goal.y >= grid[0].size() ||
            !grid[goal.x][goal.y].isTraversable()) {
            return path;
        }
        
        // Simple direct path for demo (replace with full A* if needed)
        // This is a simplified version for the demo
        int dx = goal.x - start.x;
        int dy = goal.y - start.y;
        
        int steps = max(abs(dx), abs(dy));
        if (steps > 0) {
            for (int i = 1; i <= steps; i++) {
                int x = start.x + dx * i / steps;
                int y = start.y + dy * i / steps;
                if (x >= 0 && x < grid.size() && y >= 0 && y < grid[0].size() && 
                    grid[x][y].isTraversable()) {
                    path.push_back(Position(x, y));
                }
            }
        }
        
        return path;
    }
};

// ==================== SIMULATION CONTROLLER ====================
class Simulation {
private:
    Environment env;
    vector<shared_ptr<Agent>> agents;
    int stepCount;
    
public:
    Simulation(int gridSize = GRID_SIZE) : env(gridSize), stepCount(0) {
        initializeEnvironment();
        initializeAgents();
    }
    
    void initializeEnvironment() {
        // Simple deterministic initialization for reproducibility
        // Hospitals
        env.getCell(2, 2).type = CellType::HOSPITAL;
        env.getCell(GRID_SIZE-3, GRID_SIZE-3).type = CellType::HOSPITAL;
        
        // Buildings (some on fire)
        for (int i = 5; i < 15; i += 3) {
            for (int j = 5; j < 15; j += 3) {
                if ((i + j) % 6 == 0) {
                    env.getCell(i, j).type = CellType::BUILDING_FIRE;
                    env.getCell(i, j).fireIntensity = 50;
                } else {
                    env.getCell(i, j).type = CellType::BUILDING_SAFE;
                }
            }
        }
        
        // Victims
        env.getCell(4, 4).hasVictim = true;
        env.getCell(4, 4).victimHealth = 70;
        env.getCell(8, 8).hasVictim = true;
        env.getCell(8, 8).victimHealth = 40;
        env.getCell(12, 12).hasVictim = true;
        env.getCell(12, 12).victimHealth = 20;
        
        // Debris
        env.getCell(6, 10).type = CellType::DEBRIS;
        env.getCell(10, 6).type = CellType::DEBRIS;
    }
    
    void initializeAgents() {
        agents.push_back(make_shared<RescueTeam>(Position(0, 0)));
        agents.push_back(make_shared<RescueTeam>(Position(GRID_SIZE-1, 0)));
        agents.push_back(make_shared<FireBrigade>(Position(0, GRID_SIZE-1)));
        agents.push_back(make_shared<FireBrigade>(Position(GRID_SIZE-1, GRID_SIZE-1)));
        agents.push_back(make_shared<Ambulance>(Position(GRID_SIZE/2, 0)));
    }
    
    void updateTasks() {
        // Scan for new tasks
        for (int x = 0; x < GRID_SIZE; x++) {
            for (int y = 0; y < GRID_SIZE; y++) {
                Cell& cell = env.getCell(x, y);
                Position pos(x, y);
                
                if ((cell.type == CellType::BUILDING_FIRE || cell.type == CellType::FIRE_ZONE) 
                    && cell.fireIntensity > 0) {
                    bool taskExists = false;
                    for (auto& task : env.tasks) {
                        if (task->position == pos && task->type == TaskType::EXTINGUISH_FIRE) {
                            taskExists = true;
                            break;
                        }
                    }
                    if (!taskExists) {
                        env.addTask(make_shared<Task>(TaskType::EXTINGUISH_FIRE, pos, cell.fireIntensity));
                    }
                }
                
                if (cell.hasVictim && !cell.victimRescued) {
                    bool taskExists = false;
                    for (auto& task : env.tasks) {
                        if (task->position == pos && task->type == TaskType::RESCUE_VICTIM) {
                            taskExists = true;
                            break;
                        }
                    }
                    if (!taskExists) {
                        env.addTask(make_shared<Task>(TaskType::RESCUE_VICTIM, pos, 100 - cell.victimHealth));
                    }
                }
                
                if (cell.type == CellType::DEBRIS) {
                    bool taskExists = false;
                    for (auto& task : env.tasks) {
                        if (task->position == pos && task->type == TaskType::CLEAR_DEBRIS) {
                            taskExists = true;
                            break;
                        }
                    }
                    if (!taskExists) {
                        env.addTask(make_shared<Task>(TaskType::CLEAR_DEBRIS, pos, 50));
                    }
                }
            }
        }
    }
    
    void assignTasks() {
        for (auto& task : env.tasks) {
            if (task->assigned) continue;
            
            shared_ptr<Agent> bestAgent = nullptr;
            double bestBid = 1000.0;
            
            for (auto& agent : agents) {
                if (!agent->isBusy() && agent->hasSufficientResources()) {
                    double bid = agent->calculateBid(task);
                    if (bid < bestBid) {
                        bestBid = bid;
                        bestAgent = agent;
                    }
                }
            }
            
            if (bestAgent) {
                bestAgent->assignTask(task);
                task->assigned = true;
                task->assignedTo = bestAgent->getTypeName() + to_string(bestAgent->getId());
                
                auto path = Pathfinding::aStar(bestAgent->getPosition(), task->position, env.grid);
                if (!path.empty()) {
                    bestAgent->setPath(path);
                }
                
                cout << "  Assigned " << task->getTypeName() << " to " 
                     << bestAgent->getTypeName() << bestAgent->getId() 
                     << " (bid: " << bestBid << ")\n";
            }
        }
    }
    
    void update() {
        stepCount++;
        
        // Update environment
        for (int x = 0; x < GRID_SIZE; x++) {
            for (int y = 0; y < GRID_SIZE; y++) {
                env.getCell(x, y).update();
            }
        }
        
        // Update tasks
        updateTasks();
        
        // Assign tasks
        assignTasks();
        
        // Update agents
        for (auto& agent : agents) {
            if (agent->isBusy()) {
                if (agent->hasPath()) {
                    agent->move();
                }
                agent->performTask(env);
            }
        }
    }
    
    void display() {
        ConsoleUtils::clearScreen();
        
        // Display header
        ConsoleUtils::setColor(BRIGHT_WHITE);
        cout << "=== DISASTER RESPONSE SIMULATION ===" << endl;
        cout << "Step: " << stepCount << " | Active Tasks: " << env.tasks.size() << endl;
        cout << "=====================================" << endl;
        
        // Display grid
        cout << "\nCITY MAP:\n";
        cout << "Legend: R=Rescue F=Fire A=Ambulance V=Victim B=Building F/f=Fire H=Hospital D=Debris\n";
        cout << string(GRID_SIZE * 2 + 2, '-') << endl;
        
        for (int y = 0; y < GRID_SIZE; y++) {
            cout << "|";
            for (int x = 0; x < GRID_SIZE; x++) {
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
                    const Cell& cell = env.getCell(x, y);
                    ConsoleUtils::setColor(cell.getColor());
                    cout << cell.getDisplayChar() << " ";
                }
            }
            ConsoleUtils::setColor(WHITE);
            cout << "|" << endl;
        }
        cout << string(GRID_SIZE * 2 + 2, '-') << endl;
        
        // Display agent status
        ConsoleUtils::setColor(BRIGHT_WHITE);
        cout << "\nAGENT STATUS:\n";
        for (const auto& agent : agents) {
            ConsoleUtils::setColor(agent->getColor());
            cout << "  " << agent->getStatus() << endl;
        }
        
        // Display statistics
        ConsoleUtils::setColor(BRIGHT_WHITE);
        cout << "\nSTATISTICS:\n";
        int victimsWaiting = 0;
        int victimsRescued = 0;
        int activeFires = 0;
        
        for (int x = 0; x < GRID_SIZE; x++) {
            for (int y = 0; y < GRID_SIZE; y++) {
                const Cell& cell = env.getCell(x, y);
                if (cell.hasVictim) {
                    if (cell.victimRescued) victimsRescued++;
                    else victimsWaiting++;
                }
                if (cell.fireIntensity > 0) activeFires++;
            }
        }
        
        ConsoleUtils::setColor(victimsWaiting == 0 ? GREEN : RED);
        cout << "  Victims waiting: " << victimsWaiting << endl;
        ConsoleUtils::setColor(victimsRescued > 0 ? GREEN : YELLOW);
        cout << "  Victims rescued: " << victimsRescued << endl;
        ConsoleUtils::setColor(activeFires == 0 ? GREEN : RED);
        cout << "  Active fires: " << activeFires << endl;
        
        ConsoleUtils::resetColor();
        cout << "\nPress Ctrl+C to stop simulation..." << endl;
    }
    
    void run() {
        while (true) {
            display();
            update();
            ConsoleUtils::sleep(2000); // 2 seconds between steps
        }
    }
};

// ==================== MAIN FUNCTION ====================
int main() {
    cout << "Starting Disaster Response Simulation..." << endl;
    cout << "Simulation will run continuously. Press Ctrl+C to stop." << endl;
    cout << "Initializing environment and agents..." << endl;
    
    ConsoleUtils::sleep(2000);
    
    try {
        Simulation simulation;
        simulation.run();
    } catch (const exception& e) {
        ConsoleUtils::setColor(BRIGHT_RED);
        cerr << "Error: " << e.what() << endl;
        ConsoleUtils::resetColor();
        return 1;
    }
    
    return 0;
}