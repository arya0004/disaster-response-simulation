#pragma once
#include "task.h"

class Environment;

class Agent {
protected:
    static int nextId;
    int id;
    AgentType type;
    Position position;
    Position target;
    vector<Position> path;
    int fuel;
    int water;
    int medicalKits;
    bool busy;
    shared_ptr<Task> currentTask;
    
public:
    Agent(AgentType t, Position startPos) 
        : id(nextId++), type(t), position(startPos), target(startPos),
          fuel(100), water(100), medicalKits(5), busy(false) {}
    
    virtual ~Agent() = default;
    
    // Getters
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
    
    // Pathfinding
    void setPath(const vector<Position>& newPath) { path = newPath; }
    bool hasPath() const { return !path.empty(); }
    const vector<Position>& getPath() const { return path; }
    
    // Task management
    void assignTask(shared_ptr<Task> task) { 
        currentTask = task; 
        busy = true;
    }
    void completeTask() { 
        currentTask = nullptr; 
        busy = false;
    }
    shared_ptr<Task> getCurrentTask() const { return currentTask; }
    
    // Movement
    virtual void move() {
        if (!path.empty()) {
            position = path.front();
            path.erase(path.begin());
            fuel = max(0, fuel - 1);
        }
    }
    
    // Decision making
    virtual double calculateBid(shared_ptr<Task> task) = 0;
    virtual void performTask(Environment& env) = 0;
    
    // Resource management
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
    
    // Display
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

// Initialize static member
int Agent::nextId = 1;