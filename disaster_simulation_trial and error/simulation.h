#pragma once
#include "pathfinding.h"

class Simulation {
private:
    Environment env;
    vector<shared_ptr<Agent>> agents;
    int stepCount;
    random_device rd;
    mt19937 gen;
    
public:
    Simulation(int gridSize = GRID_SIZE) : env(gridSize), stepCount(0), gen(rd()) {
        initializeEnvironment();
        initializeAgents();
    }
    
    void initializeEnvironment() {
        uniform_int_distribution<> dis(0, GRID_SIZE - 1);
        
        // Add some hospitals
        for (int i = 0; i < 2; i++) {
            int x, y;
            do {
                x = dis(gen);
                y = dis(gen);
            } while (env.getCell(x, y).type != CellType::EMPTY);
            env.getCell(x, y).type = CellType::HOSPITAL;
        }
        
        // Add some buildings (safe and on fire)
        for (int i = 0; i < 15; i++) {
            int x = dis(gen);
            int y = dis(gen);
            if (env.getCell(x, y).type == CellType::EMPTY) {
                env.getCell(x, y).type = (i < 4) ? CellType::BUILDING_FIRE : CellType::BUILDING_SAFE;
                if (env.getCell(x, y).type == CellType::BUILDING_FIRE) {
                    env.getCell(x, y).fireIntensity = 50;
                }
            }
        }
        
        // Add some victims
        for (int i = 0; i < 12; i++) {
            int x = dis(gen);
            int y = dis(gen);
            Cell& cell = env.getCell(x, y);
            if (cell.type == CellType::EMPTY || cell.type == CellType::BUILDING_SAFE) {
                cell.hasVictim = true;
                cell.victimHealth = 30 + (dis(gen) % 70); // 30-100 health
            }
        }
        
        // Add some debris
        for (int i = 0; i < 8; i++) {
            int x = dis(gen);
            int y = dis(gen);
            if (env.getCell(x, y).type == CellType::EMPTY) {
                env.getCell(x, y).type = CellType::DEBRIS;
            }
        }
    }
    
    void initializeAgents() {
        // Create agents at random positions
        uniform_int_distribution<> dis(0, GRID_SIZE - 1);
        
        agents.push_back(make_shared<RescueTeam>(Position(0, 0)));
        agents.push_back(make_shared<RescueTeam>(Position(GRID_SIZE-1, 0)));
        agents.push_back(make_shared<RescueTeam>(Position(0, GRID_SIZE-1)));
        
        agents.push_back(make_shared<FireBrigade>(Position(GRID_SIZE-1, GRID_SIZE-1)));
        agents.push_back(make_shared<FireBrigade>(Position(GRID_SIZE/2, 0)));
        
        agents.push_back(make_shared<Ambulance>(Position(0, GRID_SIZE/2)));
        agents.push_back(make_shared<Ambulance>(Position(GRID_SIZE/2, GRID_SIZE-1)));
    }
    
    void updateTasks() {
        // Scan environment for new tasks
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
                            task->urgency = cell.fireIntensity; // Update urgency
                            break;
                        }
                    }
                    if (!taskExists) {
                        env.addTask(make_shared<Task>(TaskType::EXTINGUISH_FIRE, 
                                                    pos, cell.fireIntensity));
                    }
                }
                
                if (cell.hasVictim && !cell.victimRescued) {
                    bool taskExists = false;
                    for (auto& task : env.tasks) {
                        if (task->position == pos && task->type == TaskType::RESCUE_VICTIM) {
                            taskExists = true;
                            task->urgency = 100 - cell.victimHealth; // Update urgency
                            break;
                        }
                    }
                    if (!taskExists) {
                        env.addTask(make_shared<Task>(TaskType::RESCUE_VICTIM, 
                                                    pos, 100 - cell.victimHealth));
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
                
                // Transport tasks for rescued victims
                if (cell.hasVictim && cell.victimRescued) {
                    bool taskExists = false;
                    for (auto& task : env.tasks) {
                        if (task->position == pos && task->type == TaskType::TRANSPORT_TO_HOSPITAL) {
                            taskExists = true;
                            break;
                        }
                    }
                    if (!taskExists) {
                        env.addTask(make_shared<Task>(TaskType::TRANSPORT_TO_HOSPITAL, 
                                                    pos, 100 - cell.victimHealth));
                    }
                }
            }
        }
    }
    
    void assignTasks() {
        // Contract Net Protocol for task allocation
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
                
                // Calculate path to task
                auto path = Pathfinding::aStar(bestAgent->getPosition(), 
                                             task->position, env.grid);
                if (!path.empty()) {
                    bestAgent->setPath(path);
                }
                
                cout << "  Assigned " << task->getTypeName() << " at (" 
                     << task->position.x << "," << task->position.y 
                     << ") to " << bestAgent->getTypeName() 
                     << bestAgent->getId() << " (bid: " << bestBid << ")\n";
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
        
        // Assign tasks to agents
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
        
        // Remove completed tasks
        env.tasks.erase(
            remove_if(env.tasks.begin(), env.tasks.end(),
                     [](const shared_ptr<Task>& task) { 
                         return task->assigned && !task->assignedTo.empty(); 
                     }),
            env.tasks.end()
        );
        
        // Refill resources occasionally
        static random_device rd;
        static mt19937 gen(rd());
        uniform_real_distribution<> dis(0.0, 1.0);
        if (dis(gen) < 0.1) { // 10% chance each step
            for (auto& agent : agents) {
                if (!agent->isBusy() && dis(gen) < 0.3) {
                    agent->refillResources();
                }
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
                // Check if any agent is at this position
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
        
        // Display active tasks
        ConsoleUtils::setColor(BRIGHT_WHITE);
        cout << "\nACTIVE TASKS:\n";
        if (env.tasks.empty()) {
            ConsoleUtils::setColor(GREEN);
            cout << "  No active tasks - situation under control!" << endl;
        } else {
            for (const auto& task : env.tasks) {
                ConsoleUtils::setColor(task->assigned ? YELLOW : RED);
                cout << "  " << task->getTypeName() << " at (" 
                     << task->position.x << "," << task->position.y 
                     << ") - Urgency: " << task->urgency;
                if (task->assigned) {
                    cout << " [Assigned to: " << task->assignedTo << "]";
                }
                cout << endl;
            }
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
            ConsoleUtils::sleep(1000); // 1 second between steps
        }
    }
    
    const Environment& getEnvironment() const { return env; }
    const vector<shared_ptr<Agent>>& getAgents() const { return agents; }
    int getStepCount() const { return stepCount; }
};