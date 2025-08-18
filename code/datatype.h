#ifndef DATATYPE_H
#define DATATYPE_H

#include <string>
#include <vector>

// Primitive cell
struct Primitive {
    std::string name;
    int width;
    int height;   // unified height
    int x, y;     // placement coordinates
};

// Circuit container
struct CircuitData {
    std::vector<Primitive> primitives;

    void loadFromFile(const std::string& filename);
};

#endif
