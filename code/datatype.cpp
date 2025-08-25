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

void CircuitData::dummyData(){
    layoutSize = {5, 5}; // 5x5 layout
    // create 9 primitives in a
    // 3x3 grid
    primitives.push_back({"M1", 0, 0, 1, 1});
    primitives.push_back({"M2", 1, 0, 1, 1});
    primitives.push_back({"M3", 2, 0, 1, 1});
    primitives.push_back({"M4", 0, 1, 1, 1  });
    primitives.push_back({"M5", 1, 1, 1, 1  });
    primitives.push_back({"M6", 2, 1, 1, 1  });  
    primitives.push_back({"M7", 0, 2, 1, 1  });
    primitives.push_back({"M8", 1, 2, 1, 1  });
    primitives.push_back({"M9", 2, 2, 1, 1  });
    // for each primitive, add 4 pins at the center of each side
    for(auto& prim : primitives){
        prim.pins.push_back({"P1", {prim.shape.width/2, 0}, &prim}); // bottom
        prim.pins.push_back({"P2", {prim.shape.width, prim.shape.height/2}, &prim}); // right
        prim.pins.push_back({"P3", {prim.shape.width/2, prim.shape.height}, &prim}); // top
        prim.pins.push_back({"P4", {0, prim.shape.height/2}, &prim}); // left
    }
    //相邻的primitive之间连线，形成一个网表，每个primitive的右侧pin和下侧pin分别和相邻primitive的左侧pin和上侧pin相连
    for(size_t i = 0; i < primitives.size(); ++i){  
        size_t row = i / 3;
        size_t col = i % 3;
        // right pin to left pin of right primitive
        if(col < 2){
            Net net;
            net.name = "N" + std::to_string(nets.size()+1);
            net.pins.push_back(&primitives[i].pins[1]); // right pin of current primitive
            net.pins.push_back(&primitives[i+1].pins[3]); // left pin of right primitive
            nets.push_back(net);
        }
        // bottom pin to top pin of bottom primitive
        if(row < 2){
            Net net;
            net.name = "N" + std::to_string(nets.size()+1);
            net.pins.push_back(&primitives[i].pins[0]); // bottom pin of current primitive
            net.pins.push_back(&primitives[i+3].pins[2]); // top pin of bottom primitive
            nets.push_back(net);
        }
    }
    //在layout 上下各加一个terminal，上面的terminal连接第一行的primitive，下面的terminal连接最后一行的primitive
    for(size_t col = 0; col < 3; ++col){
        Pin topTerminal = {"T_top_" + std::to_string(col+1), {col, 3}, nullptr};
        terminals.push_back(&topTerminal);
        Net netTop;
        netTop.name = "N" + std::to_string(nets.size()+1);
        netTop.pins.push_back(&topTerminal);
        netTop.pins.push_back(&primitives[col].pins[2]); // top pin of first row primitive
        nets.push_back(netTop);

        Pin bottomTerminal = {"T_bottom_" + std::to_string(col+1), {col, -1}, nullptr};
        terminals.push_back(&bottomTerminal);
        Net netBottom;
        netBottom.name = "N" + std::to_string(nets.size()+1);
        netBottom.pins.push_back(&bottomTerminal);
        netBottom.pins.push_back(&primitives[6 + col].pins[0]); // bottom pin of last row primitive
        nets.push_back(netBottom);
    }



}
