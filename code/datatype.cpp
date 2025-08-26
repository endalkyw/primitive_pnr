#include "datatype.h"
#include <cassert>
#include <iostream>

// ---- 索引 ----
void CircuitData::buildPrimitiveIndex() {
    primitive_by_name.clear();
    for (auto& m : primitives) {
        primitive_by_name[m.name] = &m;
        m.pin_index_by_name.clear();
        for (size_t i = 0; i < m.pins.size(); ++i)
            m.pin_index_by_name[m.pins[i].name] = i;
    }
}

void CircuitData::buildTerminalTemplateIndex() {
    term_tpl_by_name.clear();
    for (auto& t : term_templates) {
        term_tpl_by_name[t.name] = &t;
        t.pin_index_by_name.clear();
        for (size_t i = 0; i < t.pins.size(); ++i)
            t.pin_index_by_name[t.pins[i].name] = i;
    }
}

// ---- Block ----
Point Block::pinAbsXY(size_t i) const {
    const auto& off = pins[i].tmpl->offset;
    return { position.x + off.x, position.y + off.y };
}
Point Block::pinAbsXY(const std::string& pinName) const {
    auto it = master->pin_index_by_name.find(pinName);
    assert(it != master->pin_index_by_name.end());
    return pinAbsXY(it->second);
}

// ---- Terminal ----
Point Terminal::pinAbsXY(size_t i) const {
    const auto& off = pins[i].tmpl->offset;
    return { position.x + off.x, position.y + off.y };
}
Point Terminal::pinAbsXY(const std::string& pinName) const {
    auto it = tmpl->pin_index_by_name.find(pinName);
    assert(it != tmpl->pin_index_by_name.end());
    return pinAbsXY(it->second);
}

// ---- 实例化 ----
Block* CircuitData::addBlock(const std::string& instName,
                             const std::string& masterName,
                             Point pos) {
    auto it = primitive_by_name.find(masterName);
    if (it == primitive_by_name.end()) {
        std::cerr << "[addBlock] master not found: " << masterName << "\n";
        return nullptr;
    }
    const Primitive* M = it->second;

    blocks.emplace_back();
    Block& b = blocks.back();
    b.name = instName;
    b.master = M;
    b.position = pos;
    b.pins.resize(M->pins.size());
    for (size_t i = 0; i < M->pins.size(); ++i) {
        b.pins[i].tmpl   = &M->pins[i];
        b.pins[i].parent = &b;
    }
    return &b;
}

Terminal* CircuitData::addTerminal(const std::string& instName,
                                   const std::string& tplName,
                                   Point pos,
                                   FixMode fixOverride,
                                   int slideMin, int slideMax) {
    auto it = term_tpl_by_name.find(tplName);
    if (it == term_tpl_by_name.end()) {
        std::cerr << "[addTerminal] template not found: " << tplName << "\n";
        return nullptr;
    }
    const TerminalTemplate* T = it->second;

    terminals.emplace_back();
    Terminal& t = terminals.back();
    t.name = instName;
    t.tmpl = T;
    t.position = pos;
    t.fix = fixOverride;
    t.edge = T->edge;
    t.slide_min = (slideMin || slideMax) ? slideMin : T->default_slide_min;
    t.slide_max = (slideMin || slideMax) ? slideMax : T->default_slide_max;

    t.pins.resize(T->pins.size());
    for (size_t i = 0; i < T->pins.size(); ++i) {
        t.pins[i].tmpl   = &T->pins[i];
        t.pins[i].parent = nullptr; // Terminal 没有 Block parent
    }
    return &t;
}

// ---- I/O ----
void CircuitData::loadFromFile(const std::string& filename) {
    std::cout << "Loading circuit from " << filename << " ...\n";
    // TODO: parse netlist
}

// ---- Dummy Data: 单一 primitive + VDD/GND terminal ----
void CircuitData::dummyData() {
    primitives.clear(); blocks.clear(); nets.clear();
    terminals.clear(); term_templates.clear();
    primitive_by_name.clear(); term_tpl_by_name.clear();

    // 1) 定义唯一 primitive
    Primitive cell;
    cell.name = "PRIM_X1";
    cell.shape = {1,1};
    cell.pins  = {
        {"P_LEFT",{0,0}},
        {"P_RIGHT",{1,0}},
        {"P_TOP",{0,1}},
        {"P_BOTTOM",{0,0}}
    };
    primitives.push_back(cell);
    buildPrimitiveIndex();

    // 2) 定义 terminal templates: VDD / GND
    TerminalTemplate vdd, gnd;
    vdd.name = "VDD_TOP"; vdd.edge = Edge::Top;
    vdd.shape = {1,1};
    vdd.pins  = { {"PAD",{0,0}} };
    vdd.default_fix = FixMode::FixedXY;

    gnd.name = "GND_BOTTOM"; gnd.edge = Edge::Bottom;
    gnd.shape = {1,1};
    gnd.pins  = { {"PAD",{0,0}} };
    gnd.default_fix = FixMode::FixedXY;

    term_templates.push_back(vdd);
    term_templates.push_back(gnd);
    buildTerminalTemplateIndex();

    // 3) 实例化 3×3 block grid
    const int ROWS=3, COLS=3;
    for (int r=0;r<ROWS;r++){
        for (int c=0;c<COLS;c++){
            std::string inst="U"+std::to_string(r*COLS+c+1);
            addBlock(inst,"PRIM_X1",{c,r});
        }
    }

    // 4) 实例化 VDD/GND terminals
    auto* T_VDD = addTerminal("T_VDD","VDD_TOP",{COLS/2,ROWS},FixMode::FixedXY);
    auto* T_GND = addTerminal("T_GND","GND_BOTTOM",{COLS/2,-1},FixMode::FixedXY);

    // 5) 构造简单网表：比如第一行所有 block 的 TOP -> VDD
    for (int c=0;c<COLS;c++){
        Block& b=blocks[c]; // 第0行
        Net net;
        net.name="N_VDD_"+b.name;
        net.pins.push_back(&T_VDD->pins[0]);
        net.pins.push_back(&b.pins[2]); // P_TOP
        nets.push_back(net);
    }
    // 最后一行所有 block 的 BOTTOM -> GND
    for (int c=0;c<COLS;c++){
        Block& b=blocks[(ROWS-1)*COLS+c];
        Net net;
        net.name="N_GND_"+b.name;
        net.pins.push_back(&T_GND->pins[0]);
        net.pins.push_back(&b.pins[3]); // P_BOTTOM
        nets.push_back(net);
    }

    std::cout << "DummyData: masters="<<primitives.size()
              << " blocks="<<blocks.size()
              << " terms="<<terminals.size()
              << " nets="<<nets.size()<<"\n";
}
