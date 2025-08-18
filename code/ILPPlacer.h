#ifndef ILPPLACE_H
#define ILPPLACE_H

#include "datatype.h"

class ILPPlace {
public:
    ILPPlace(CircuitData& data);
    void run();  // execute ILP-based placement

private:
    CircuitData& circuit;
};

#endif
