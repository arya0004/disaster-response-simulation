#pragma once

#include <iostream>
#include <vector>
#include <memory>
#include <queue>
#include <cmath>
#include <algorithm>
#include <random>
#include <string>
#include <thread>
#include <chrono>
#include <windows.h>  // For console colors

using namespace std;

// Constants
const int GRID_SIZE = 20;
const int DISPLAY_WIDTH = 60;
const int DISPLAY_HEIGHT = 30;

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