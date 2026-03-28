#pragma once
#include "cell.h"

class Task {
public:
    TaskType type;
    Position position;
    int urgency; // 1-100, higher = more urgent
    string description;
    bool assigned;
    string assignedTo;
    
    Task(TaskType t, Position pos, int urg, string desc = "")
        : type(t), position(pos), urgency(urg), description(desc), 
          assigned(false), assignedTo("") {}
    
    // Calculate cost for an agent at given position
    double calculateCost(const Position& agentPos, AgentType agentType) const {
        double distance = position.distanceTo(agentPos);
        double urgencyFactor = (100 - urgency) * 0.1;
        
        // Different agent types have different efficiencies for different tasks
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