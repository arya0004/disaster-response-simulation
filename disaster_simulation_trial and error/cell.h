#pragma once
#include "utils.h"

class Cell {
public:
    int x, y;
    CellType type;
    bool hasVictim;
    bool victimRescued;
    int fireIntensity; // 0-100
    int victimHealth; // 0-100, 0 = deceased
    
    // Default constructor for vector compatibility
    Cell() : x(0), y(0), type(CellType::EMPTY), hasVictim(false), victimRescued(false), 
             fireIntensity(0), victimHealth(100) {}
    
    Cell(int x, int y, CellType type = CellType::EMPTY) 
        : x(x), y(y), type(type), hasVictim(false), victimRescued(false), 
          fireIntensity(0), victimHealth(100) {}
    
    // Check if cell is traversable
    bool isTraversable() const {
        return type != CellType::DEBRIS && fireIntensity < 70;
    }
    
    // Update cell state
    void update() {
        if (type == CellType::BUILDING_FIRE || type == CellType::FIRE_ZONE) {
            // Fire spreads and intensifies
            if (fireIntensity < 100) {
                fireIntensity += 5;
            }
            
            // Chance to spread fire to adjacent cells
            static random_device rd;
            static mt19937 gen(rd());
            uniform_real_distribution<> dis(0.0, 1.0);
            
            if (dis(gen) < 0.1) { // 10% chance to spread each update
                // In a full implementation, this would spread to adjacent cells
            }
        }
        
        if (hasVictim && !victimRescued) {
            // Victim health deteriorates faster in fire
            int healthLoss = (type == CellType::BUILDING_FIRE || type == CellType::FIRE_ZONE) ? 5 : 2;
            victimHealth = max(0, victimHealth - healthLoss);
        }
    }
    
    // Get display character
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
    
    // Get color for display
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
    
    string getTypeName() const {
        switch(type) {
            case CellType::EMPTY: return "Empty";
            case CellType::BUILDING_SAFE: return "Safe Building";
            case CellType::BUILDING_FIRE: return "Burning Building";
            case CellType::VICTIM: return "Victim";
            case CellType::HOSPITAL: return "Hospital";
            case CellType::DEBRIS: return "Debris";
            case CellType::FIRE_ZONE: return "Fire Zone";
            default: return "Unknown";
        }
    }
};