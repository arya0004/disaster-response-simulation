#pragma once
#include "main.h"

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
        // Use Windows Sleep instead of C++11 threads for better compatibility
        Sleep(milliseconds);
    }
    
    static void setCursorPosition(int x, int y) {
        COORD coord;
        coord.X = x;
        coord.Y = y;
        SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
    }
};