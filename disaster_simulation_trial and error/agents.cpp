#pragma once
#include "agent.h"

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
        tasks.erase(remove(tasks.begin(), tasks.end(), task), tasks.end());
    }
    
    int getGridSize() const { return grid.size(); }
};

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
                return 1000.0; // High cost for unsuitable tasks
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
                        // Victim is now ready for transport
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
            // Pick up victim
            if (cell.hasVictim && cell.victimRescued && medicalKits > 0) {
                cell.hasVictim = false;
                cell.victimRescued = false;
                transportingVictim = true;
                medicalKits--;
                cout << "  Ambulance " << id << " picked up victim at (" 
                     << position.x << "," << position.y << ")\n";
                
                // Find nearest hospital
                Position nearestHospital = findNearestHospital(env);
                if (nearestHospital.x != -1) {
                    // In full implementation, would set path to hospital
                    cout << "  Ambulance " << id << " heading to hospital at (" 
                         << nearestHospital.x << "," << nearestHospital.y << ")\n";
                }
            }
        } else if (transportingVictim && cell.type == CellType::HOSPITAL) {
            // Deliver to hospital
            transportingVictim = false;
            cout << "  Ambulance " << id << " delivered victim to hospital at (" 
                 << position.x << "," << position.y << ")\n";
            completeTask();
            env.removeTask(currentTask);
        }
    }
    
    Position findNearestHospital(Environment& env) {
        Position nearest(-1, -1);
        double minDist = numeric_limits<double>::max();
        
        for (int x = 0; x < env.getGridSize(); x++) {
            for (int y = 0; y < env.getGridSize(); y++) {
                if (env.getCell(x, y).type == CellType::HOSPITAL) {
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
    
    char getDisplayChar() const override { return 'A'; }
    ConsoleColor getColor() const override { return BRIGHT_CYAN; }
};