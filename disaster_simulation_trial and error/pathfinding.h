#pragma once
#include "agents.cpp"

struct Node {
    Position pos;
    double f, g, h;
    shared_ptr<Node> parent;
    
    Node(Position p, double g, double h, shared_ptr<Node> par = nullptr)
        : pos(p), f(g + h), g(g), h(h), parent(par) {}
};

class Pathfinding {
public:
    static vector<Position> aStar(const Position& start, const Position& goal, 
                                 const vector<vector<Cell>>& grid) {
        vector<Position> path;
        
        // Check if goal is valid
        if (goal.x < 0 || goal.x >= grid.size() || 
            goal.y < 0 || goal.y >= grid[0].size() ||
            !grid[goal.x][goal.y].isTraversable()) {
            return path;
        }
        
        auto compare = [](const shared_ptr<Node>& a, const shared_ptr<Node>& b) {
            return a->f > b->f;
        };
        
        priority_queue<shared_ptr<Node>, vector<shared_ptr<Node>>, 
                      decltype(compare)> openList(compare);
        vector<vector<bool>> closedList(grid.size(), 
                                      vector<bool>(grid[0].size(), false));
        
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
            
            closedList[current->pos.x][current->pos.y] = true;
            
            // Check neighbors (4-directional movement)
            int dx[] = {-1, 1, 0, 0};
            int dy[] = {0, 0, -1, 1};
            
            for (int i = 0; i < 4; i++) {
                Position neighbor(current->pos.x + dx[i], current->pos.y + dy[i]);
                
                if (neighbor.x < 0 || neighbor.x >= grid.size() ||
                    neighbor.y < 0 || neighbor.y >= grid[0].size() ||
                    closedList[neighbor.x][neighbor.y] ||
                    !grid[neighbor.x][neighbor.y].isTraversable()) {
                    continue;
                }
                
                double g = current->g + 1.0;
                double h = heuristic(neighbor, goal);
                auto neighborNode = make_shared<Node>(neighbor, g, h, current);
                
                openList.push(neighborNode);
            }
        }
        
        return path;
    }
    
private:
    static double heuristic(const Position& a, const Position& b) {
        return abs(a.x - b.x) + abs(a.y - b.y); // Manhattan distance
    }
};