#include "simulation.h"

int main() {
    cout << "Starting Disaster Response Simulation..." << endl;
    cout << "Simulation will run continuously. Press Ctrl+C to stop." << endl;
    cout << "Initializing environment and agents..." << endl;
    
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