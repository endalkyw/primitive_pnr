#include "datatype.h"
#include <fstream>
#include <iostream>

void CircuitData::loadFromFile(const std::string& filename) {
    // TODO: simple parser for testcases
    std::cout << "Loading circuit from " << filename << " ..." << std::endl;

    // Example dummy data
    primitives.push_back({"M1", 2, 1, 0, 0});
    primitives.push_back({"M2", 3, 1, 0, 0});
}
