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

using namespace std;

// Constants
const int GRID_SIZE = 20;

// Console Colors
const int COLORS[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};

enum class CellType { EMPTY, BUILDING, FIRE, VICTIM, HOSPITAL, DEBRIS };
enum class AgentType { RESCUE_TEAM, FIRE_BRIGADE, AMBULANCE };

// ==================== POSITION CLASS ====================
class Position {
public:
    int x, y;
    Position(int x = 0, int y = 0) : x(x), y(y) {}
    bool operator==(const Position& other) const { return x == other.x && y == other.y; }
    double distanceTo(const Position& other) const { 
        return sqrt(pow(x - other.x, 2) + pow(y - other.y, 2)); 
    }
};

// ==================== CELL CLASS ====================
class Cell {
public:
    int x, y;
    CellType type;
    int fireIntensity;
    int victimHealth;
    bool hasVictim;
    bool victimRescued;
    
    // Default constructor - FIXED!
    Cell() : x(0), y(0), type(CellType::EMPTY), fireIntensity(0), 
             victimHealth(100), hasVictim(false), victimRescued(false) {}
    
    Cell(int x, int y) : x(x), y(y), type(CellType::EMPTY), fireIntensity(0),
                        victimHealth(100), hasVictim(false), victimRescued(false) {}
    
    void update() {
        if (type == CellType::FIRE && fireIntensity < 100) {
            fireIntensity = min(100, fireIntensity + 10);
        }
        
        if (hasVictim && !victimRescued) {
            victimHealth = max(0, victimHealth - (type == CellType::FIRE ? 10 : 2));
        }
    }
    
    char getChar() const {
        if (hasVictim && !victimRescued) return 'V';
        switch(type) {
            case CellType::EMPTY: return '.';
            case CellType::BUILDING: return 'B';
            case CellType::FIRE: return fireIntensity > 50 ? 'F' : 'f';
            case CellType::HOSPITAL: return 'H';
            case CellType::DEBRIS: return 'D';
            default: return '?';
        }
    }
    
    int getColor() const {
        if (hasVictim && !victimRescued) {
            if (victimHealth > 60) return COLORS[13];
            if (victimHealth > 30) return COLORS[5];
            return COLORS[4];
        }
        
        switch(type) {
            case CellType::EMPTY: return COLORS[8];
            case CellType::BUILDING: return COLORS[1];
            case CellType::FIRE: return fireIntensity > 50 ? COLORS[12] : COLORS[4];
            case CellType::HOSPITAL: return COLORS[10];
            case CellType::DEBRIS: return COLORS[6];
            default: return COLORS[7];
        }
    }
    
    bool isTraversable() const {
        return type != CellType::DEBRIS && fireIntensity < 60;
    }
};

// ==================== AGENT CLASS ====================
class Agent {
public:
    int id;
    AgentType type;
    Position pos;
    bool busy;
    int resources;
    
    Agent(int id, AgentType type, Position pos) 
        : id(id), type(type), pos(pos), busy(false), resources(100) {}
    
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
            case AgentType::RESCUE_TEAM: return COLORS[14];
            case AgentType::FIRE_BRIGADE: return COLORS[12];
            case AgentType::AMBULANCE: return COLORS[11];
            default: return COLORS[15];
        }
    }
    
    string getStatus() const {
        string typeStr;
        switch(type) {
            case AgentType::RESCUE_TEAM: typeStr = "RESCUE"; break;
            case AgentType::FIRE_BRIGADE: typeStr = "FIRE"; break;
            case AgentType::AMBULANCE: typeStr = "AMBUL"; break;
        }
        return typeStr + to_string(id) + "(" + to_string(pos.x) + "," + to_string(pos.y) + ")";
    }
};

// ==================== SIMULATION CLASS ====================
class Simulation {
private:
    vector<vector<Cell>> grid;
    vector<Agent> agents;
    int stepCount;
    
public:
    Simulation() : stepCount(0) {
        initializeGrid();
        initializeAgents();
    }
    
    void initializeGrid() {
        // Initialize grid with proper dimensions
        grid.resize(GRID_SIZE, vector<Cell>(GRID_SIZE));
        
        for (int x = 0; x < GRID_SIZE; x++) {
            for (int y = 0; y < GRID_SIZE; y++) {
                grid[x][y] = Cell(x, y);
            }
        }
        
        // Create interesting layout
        createCityLayout();
    }
    
    void createCityLayout() {
        // Hospitals at corners
        grid[1][1].type = CellType::HOSPITAL;
        grid[GRID_SIZE-2][GRID_SIZE-2].type = CellType::HOSPITAL;
        grid[1][GRID_SIZE-2].type = CellType::HOSPITAL;
        grid[GRID_SIZE-2][1].type = CellType::HOSPITAL;
        
        // Create building blocks with fires and victims
        for (int x = 3; x < GRID_SIZE-3; x += 4) {
            for (int y = 3; y < GRID_SIZE-3; y += 4) {
                // 2x2 building block
                for (int dx = 0; dx < 2; dx++) {
                    for (int dy = 0; dy < 2; dy++) {
                        int nx = x + dx, ny = y + dy;
                        if (nx < GRID_SIZE && ny < GRID_SIZE) {
                            grid[nx][ny].type = CellType::BUILDING;
                            
                            // Random fires
                            if ((x + y) % 5 == 0) {
                                grid[nx][ny].type = CellType::FIRE;
                                grid[nx][ny].fireIntensity = 30 + (rand() % 50);
                            }
                            
                            // Random victims
                            if ((x * y) % 7 == 0) {
                                grid[nx][ny].hasVictim = true;
                                grid[nx][ny].victimHealth = 20 + (rand() % 70);
                            }
                        }
                    }
                }
            }
        }
        
        // Add some debris
        for (int i = 0; i < 10; i++) {
            int x = rand() % GRID_SIZE;
            int y = rand() % GRID_SIZE;
            if (grid[x][y].type == CellType::EMPTY) {
                grid[x][y].type = CellType::DEBRIS;
            }
        }
    }
    
    void initializeAgents() {
        agents.push_back(Agent(1, AgentType::RESCUE_TEAM, Position(0, 0)));
        agents.push_back(Agent(2, AgentType::RESCUE_TEAM, Position(GRID_SIZE-1, 0)));
        agents.push_back(Agent(3, AgentType::FIRE_BRIGADE, Position(0, GRID_SIZE-1)));
        agents.push_back(Agent(4, AgentType::FIRE_BRIGADE, Position(GRID_SIZE-1, GRID_SIZE-1)));
        agents.push_back(Agent(5, AgentType::AMBULANCE, Position(GRID_SIZE/2, 0)));
    }
    
    void spreadFire() {
        vector<Position> newFires;
        
        for (int x = 0; x < GRID_SIZE; x++) {
            for (int y = 0; y < GRID_SIZE; y++) {
                if (grid[x][y].type == CellType::FIRE && grid[x][y].fireIntensity > 40) {
                    // Chance to spread
                    for (int dx = -1; dx <= 1; dx++) {
                        for (int dy = -1; dy <= 1; dy++) {
                            if (dx == 0 && dy == 0) continue;
                            int nx = x + dx, ny = y + dy;
                            if (nx >= 0 && nx < GRID_SIZE && ny >= 0 && ny < GRID_SIZE) {
                                if ((grid[nx][ny].type == CellType::BUILDING || 
                                     grid[nx][ny].type == CellType::EMPTY) &&
                                    rand() % 4 == 0) {
                                    newFires.push_back(Position(nx, ny));
                                }
                            }
                        }
                    }
                }
            }
        }
        
        for (const auto& pos : newFires) {
            if (grid[pos.x][pos.y].type != CellType::HOSPITAL) {
                grid[pos.x][pos.y].type = CellType::FIRE;
                grid[pos.x][pos.y].fireIntensity = 20;
            }
        }
    }
    
    void moveAgents() {
        for (auto& agent : agents) {
            if (agent.resources <= 0) {
                agent.resources = 100; // Refill
                continue;
            }
            
            // Simple random movement with purpose
            int dx = 0, dy = 0;
            
            // Different behavior based on agent type
            switch(agent.type) {
                case AgentType::FIRE_BRIGADE:
                    // Move towards fires
                    for (int x = 0; x < GRID_SIZE; x++) {
                        for (int y = 0; y < GRID_SIZE; y++) {
                            if (grid[x][y].type == CellType::FIRE) {
                                if (x > agent.pos.x) dx = 1;
                                else if (x < agent.pos.x) dx = -1;
                                if (y > agent.pos.y) dy = 1;
                                else if (y < agent.pos.y) dy = -1;
                            }
                        }
                    }
                    break;
                    
                case AgentType::RESCUE_TEAM:
                    // Move towards victims
                    for (int x = 0; x < GRID_SIZE; x++) {
                        for (int y = 0; y < GRID_SIZE; y++) {
                            if (grid[x][y].hasVictim && !grid[x][y].victimRescued) {
                                if (x > agent.pos.x) dx = 1;
                                else if (x < agent.pos.x) dx = -1;
                                if (y > agent.pos.y) dy = 1;
                                else if (y < agent.pos.y) dy = -1;
                            }
                        }
                    }
                    break;
                    
                case AgentType::AMBULANCE:
                    // Move towards rescued victims or hospitals
                    for (int x = 0; x < GRID_SIZE; x++) {
                        for (int y = 0; y < GRID_SIZE; y++) {
                            if (grid[x][y].hasVictim && grid[x][y].victimRescued) {
                                if (x > agent.pos.x) dx = 1;
                                else if (x < agent.pos.x) dx = -1;
                                if (y > agent.pos.y) dy = 1;
                                else if (y < agent.pos.y) dy = -1;
                            }
                        }
                    }
                    break;
            }
            
            // Apply movement with bounds checking
            int newX = agent.pos.x + dx;
            int newY = agent.pos.y + dy;
            
            if (newX >= 0 && newX < GRID_SIZE && newY >= 0 && newY < GRID_SIZE &&
                grid[newX][newY].isTraversable()) {
                agent.pos.x = newX;
                agent.pos.y = newY;
                agent.resources--;
                
                // Perform actions
                performAgentAction(agent);
            }
        }
    }
    
    void performAgentAction(Agent& agent) {
        Cell& cell = grid[agent.pos.x][agent.pos.y];
        
        switch(agent.type) {
            case AgentType::FIRE_BRIGADE:
                if (cell.type == CellType::FIRE) {
                    cell.fireIntensity = max(0, cell.fireIntensity - 40);
                    if (cell.fireIntensity == 0) {
                        cell.type = CellType::BUILDING;
                    }
                }
                break;
                
            case AgentType::RESCUE_TEAM:
                if (cell.hasVictim && !cell.victimRescued) {
                    cell.victimRescued = true;
                } else if (cell.type == CellType::DEBRIS) {
                    cell.type = CellType::EMPTY;
                }
                break;
                
            case AgentType::AMBULANCE:
                if (cell.hasVictim && cell.victimRescued) {
                    cell.hasVictim = false;
                    cell.victimRescued = false;
                }
                break;
        }
    }
    
    void update() {
        stepCount++;
        
        // Update cells
        for (int x = 0; x < GRID_SIZE; x++) {
            for (int y = 0; y < GRID_SIZE; y++) {
                grid[x][y].update();
            }
        }
        
        // Spread fire occasionally
        if (stepCount % 3 == 0) {
            spreadFire();
        }
        
        // Move agents
        moveAgents();
    }
    
    void display() {
        system("cls");
        
        // Header
        setColor(COLORS[15]);
        cout << "🚨 DISASTER RESPONSE SIMULATION - Step " << stepCount << " 🚨\n";
        cout << "==============================================\n\n";
        
        // Display grid
        cout << "CITY MAP:\n";
        for (int y = 0; y < GRID_SIZE; y++) {
            for (int x = 0; x < GRID_SIZE; x++) {
                // Check if agent at this position
                bool agentHere = false;
                for (const auto& agent : agents) {
                    if (agent.pos.x == x && agent.pos.y == y) {
                        setColor(agent.getColor());
                        cout << agent.getChar() << " ";
                        agentHere = true;
                        break;
                    }
                }
                
                if (!agentHere) {
                    setColor(grid[x][y].getColor());
                    cout << grid[x][y].getChar() << " ";
                }
            }
            setColor(COLORS[15]);
            cout << endl;
        }
        
        // Legend
        cout << "\nLEGEND: ";
        setColor(COLORS[14]); cout << "R"; setColor(COLORS[15]); cout << "=Rescue ";
        setColor(COLORS[12]); cout << "F"; setColor(COLORS[15]); cout << "=Fire ";
        setColor(COLORS[11]); cout << "A"; setColor(COLORS[15]); cout << "=Ambulance ";
        setColor(COLORS[13]); cout << "V"; setColor(COLORS[15]); cout << "=Victim ";
        setColor(COLORS[4]); cout << "F/f"; setColor(COLORS[15]); cout << "=Fire ";
        setColor(COLORS[10]); cout << "H"; setColor(COLORS[15]); cout << "=Hospital\n";
        
        // Agent status
        cout << "\nAGENTS:\n";
        for (const auto& agent : agents) {
            setColor(agent.getColor());
            cout << "  " << agent.getStatus() << " R:" << agent.resources << endl;
        }
        
        // Statistics
        int fires = 0, victims = 0, saved = 0;
        for (int x = 0; x < GRID_SIZE; x++) {
            for (int y = 0; y < GRID_SIZE; y++) {
                if (grid[x][y].type == CellType::FIRE) fires++;
                if (grid[x][y].hasVictim) {
                    if (grid[x][y].victimRescued) saved++;
                    else victims++;
                }
            }
        }
        
        cout << "\n📊 STATISTICS:\n";
        setColor(fires > 0 ? COLORS[12] : COLORS[10]);
        cout << "  Active Fires: " << fires << endl;
        setColor(victims > 0 ? COLORS[13] : COLORS[10]);
        cout << "  Victims Waiting: " << victims << endl;
        setColor(COLORS[10]);
        cout << "  Victims Saved: " << saved << endl;
        
        setColor(COLORS[15]);
        cout << "\nPress Ctrl+C to stop | Next step in 0.5 seconds...\n";
    }
    
    void setColor(int color) {
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
    }
    
    void run() {
        while (true) {
            display();
            update();
            Sleep(500); // Fast updates!
        }
    }
};

// ==================== MAIN ====================
int main() {
    srand(time(0));
    system("mode con: cols=60 lines=40");
    
    Simulation sim;
    sim.run();
    return 0;
}