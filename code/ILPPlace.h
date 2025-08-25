#ifndef ILPPLACE_H
#define ILPPLACE_H

#include "datatype.h"


class ILPPlace {
public:
    ILPPlace(CircuitData& data);
    void run();  // execute ILP-based placement
    void printSolution(); // print placement solution
    

private:

    CircuitData& circuit;
    void dummySolve(); // placeholder for ILP solver

};

#endif
