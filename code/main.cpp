#include <iostream>
#include "datatype.h"
#include "ILPPlace.h"

int main(int argc, char* argv[]) {
    std::cout << "Analog ILP Placer started..." << std::endl;

    // Example: initialize data
    CircuitData circuit;
    circuit.dummyData();
    //print absolute address example.cir in system out
   

    // Run placer
    ILPPlace placer(circuit);
    placer.run();

    std::cout << "Placement finished." << std::endl;
    return 0;
}
