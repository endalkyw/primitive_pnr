#include "ILPPlace.h"
#include <iostream>

ILPPlace::ILPPlace(CircuitData& data) : circuit(data) {}

void ILPPlace::run() {
    std::cout << "Running ILP placer on " 
              << circuit.blocks.size() 
              << " blocks..." << std::endl;

    // TODO: formulate ILP problem
    // - unify height
    
    // - row-based placement constraints
    // - objective: e.g., HPWL minimization
}

void ILPPlace::printSolution() {
    std::cout << "Placement Solution:" << std::endl;
    for (const auto& block : circuit.blocks) {
        std::cout << "Block " << block.name 
                  << " at (" << block.position.x 
                  << ", " << block.position.y << ")\n";
    }
}

void ILPPlace::dummySolve() {
    
}
