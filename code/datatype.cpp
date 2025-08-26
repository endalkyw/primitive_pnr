#include "datatype.h"
#include <cassert>
#include <iostream>

// ======================= 索引构建 =======================

void CircuitData::buildPrimitiveIndex() {
  primitive_by_name.clear();
  for (auto& m : primitives) {
    primitive_by_name[m.name] = &m;
    m.pin_index_by_name.clear();
    for (size_t i = 0; i < m.pins.size(); ++i) {
      m.pin_index_by_name[m.pins[i].name] = i;
    }
  }
}

void CircuitData::buildTerminalTemplateIndex() {
  term_tpl_by_name.clear();
  for (auto& t : term_templates) {
    term_tpl_by_name[t.name] = &t;
    t.pin_index_by_name.clear();
    for (size_t i = 0; i < t.pins.size(); ++i) {
      t.pin_index_by_name[t.pins[i].name] = i;
    }
  }
}

// ======================= Block =======================

DPoint Block::pinAbsXY(size_t i) const {
  assert(i < pins.size());
  const auto& off = pins[i].tmpl->offset; // double 偏移
  return { position.x + off.x, position.y + off.y };
}

DPoint Block::pinAbsXY(const std::string& pinName) const {
  auto it = master->pin_index_by_name.find(pinName);
  assert(it != master->pin_index_by_name.end());
  return pinAbsXY(it->second);
}

// ======================= Terminal =======================

DPoint Terminal::pinAbsXY(size_t i) const {
  assert(i < pins.size());
  const auto& off = pins[i].tmpl->offset; // double 偏移
  return { position.x + off.x, position.y + off.y };
}

DPoint Terminal::pinAbsXY(const std::string& pinName) const {
  auto it = tmpl->pin_index_by_name.find(pinName);
  assert(it != tmpl->pin_index_by_name.end());
  return pinAbsXY(it->second);
}

// ======================= 实例化 =======================

Block* CircuitData::addBlock(const std::string& instName,
                             const std::string& masterName,
                             IPoint pos) {
  auto it = primitive_by_name.find(masterName);
  if (it == primitive_by_name.end()) {
    std::cerr << "[addBlock] master not found: " << masterName << "\n";
    return nullptr;
  }
  const Primitive* M = it->second;

  blocks.emplace_back();
  Block& b = blocks.back();
  b.name    = instName;
  b.master  = M;
  b.position= pos;
  b.index   = static_cast<int>(blocks.size()) - 1;

  b.pins.resize(M->pins.size());
  for (size_t i = 0; i < M->pins.size(); ++i) {
    b.pins[i].tmpl  = &M->pins[i];
    b.pins[i].block = &b;
    b.pins[i].term  = nullptr;
  }
  return &b;
}

Terminal* CircuitData::addTerminal(const std::string& instName,
                                   const std::string& tplName,
                                   DPoint pos,
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
  t.name      = instName;
  t.tmpl      = T;
  t.position  = pos;
  t.fix       = fixOverride;
  t.edge      = T->edge;
  t.slide_min = (slideMin || slideMax) ? slideMin : T->default_slide_min;
  t.slide_max = (slideMin || slideMax) ? slideMax : T->default_slide_max;

  t.pins.resize(T->pins.size());
  for (size_t i = 0; i < T->pins.size(); ++i) {
    t.pins[i].tmpl  = &T->pins[i];
    t.pins[i].block = nullptr;
    t.pins[i].term  = &t;
  }
  return &t;
}

// ======================= I/O =======================

void CircuitData::loadFromFile(const std::string& filename) {
  std::cout << "Loading circuit from " << filename << " ... (TODO)\n";
  // TODO: parse your netlist/placement file and fill primitives / blocks / nets / terminals
}

// ======================= Dummy Data =======================
//
//  - Primitive: 1x1 宏，四个 pin 在四条边的中点：
//      P_LEFT(0,0.5), P_RIGHT(1,0.5), P_TOP(0.5,1), P_BOTTOM(0.5,0)
//  - Terminal 模板：pad 中心 (0.5,0.5)
//  - 生成 3×3 个 blocks，左下角在整数格 (c,r)
//  - VDD/GND 终端：放置使得 PAD 中心分别在 (1.5,3.0) / (1.5,-1.0)
//  - 连接：首行 block 的 P_TOP → VDD，末行 block 的 P_BOTTOM → GND
//
void CircuitData::dummyData() {
  primitives.clear(); term_templates.clear();
  blocks.clear(); terminals.clear(); nets.clear();
  primitive_by_name.clear(); term_tpl_by_name.clear();

  // 1) primitive：1x1，四边中点
  Primitive cell;
  cell.name  = "PRIM_X1";
  cell.shape = {1,1};
  cell.pins  = {
      {"P_LEFT",   {0.0, 0.5}},
      {"P_RIGHT",  {1.0, 0.5}},
      {"P_TOP",    {0.5, 1.0}},
      {"P_BOTTOM", {0.5, 0.0}}
  };
  primitives.push_back(cell);
  buildPrimitiveIndex();

  // 2) terminal templates（pad 中心）
  TerminalTemplate vdd, gnd;
  vdd.name = "VDD_TOP"; vdd.edge = Edge::Top;
  vdd.shape= {1,1};
  vdd.pins = { {"PAD",{0.5,0.5}} };
  vdd.default_fix = FixMode::FixedXY;

  gnd.name = "GND_BOTTOM"; gnd.edge = Edge::Bottom;
  gnd.shape= {1,1};
  gnd.pins = { {"PAD",{0.5,0.5}} };
  gnd.default_fix = FixMode::FixedXY;

  term_templates.push_back(vdd);
  term_templates.push_back(gnd);
  buildTerminalTemplateIndex();

  // 3) 3×3 blocks（左下角整数栅格）
  const int ROWS = 3, COLS = 3;
  for (int r = 0; r < ROWS; ++r) {
    for (int c = 0; c < COLS; ++c) {
      std::string inst = "U" + std::to_string(r * COLS + c + 1);
      addBlock(inst, "PRIM_X1", {c, r});
    }
  }
  for (int i = 0; i < (int)blocks.size(); ++i) blocks[i].index = i;

  // 4) VDD/GND terminals：让 PAD 中心在 (1.5,3.0)/(1.5,-1.0)
  auto* T_VDD = addTerminal("T_VDD", "VDD_TOP",    {1.0,  4.5}, FixMode::FixedXY);
  auto* T_GND = addTerminal("T_GND", "GND_BOTTOM", {1.0, 0.5}, FixMode::FixedXY);

  constexpr int P_LEFT   = 0;
  constexpr int P_RIGHT  = 1;
  constexpr int P_TOP    = 2;
  constexpr int P_BOTTOM = 3;

  // 5) 终端连接：最上行连 VDD（3 条），最下行连 GND（3 条） → 6
  for (int c = 0; c < COLS; ++c) {
    Block& top = blocks[(ROWS-1) * COLS + c]; // r=2 → U7..U9
    Net nV; nV.name = "N_VDD_" + top.name;
    nV.pins.push_back(&T_VDD->pins[0]);   // VDD pad
    nV.pins.push_back(&top.pins[P_TOP]);  // 顶部块的 TOP
    nets.push_back(std::move(nV));
  }
  for (int c = 0; c < COLS; ++c) {
    Block& bot = blocks[0 * COLS + c];    // r=0 → U1..U3
    Net nG; nG.name = "N_GND_" + bot.name;
    nG.pins.push_back(&T_GND->pins[0]);   // GND pad
    nG.pins.push_back(&bot.pins[P_BOTTOM]);
    nets.push_back(std::move(nG));
  }

  // 6) 横向相邻：A(right) ↔ B(left)（每行 2 条 × 3 行 = 6）
  for (int r = 0; r < ROWS; ++r) {
    for (int c = 0; c < COLS - 1; ++c) {
      Block& A = blocks[r * COLS + c];
      Block& B = blocks[r * COLS + (c + 1)];
      Net nH; nH.name = "N_H_" + A.name + "_" + B.name;
      nH.pins.push_back(&A.pins[P_RIGHT]);
      nH.pins.push_back(&B.pins[P_LEFT]);
      nets.push_back(std::move(nH));
    }
  }

  // 7) 纵向相邻：上块 BOTTOM ↔ 下块 TOP（每列 2 条 × 3 列 = 6）
  for (int r = 0; r < ROWS - 1; ++r) {
    for (int c = 0; c < COLS; ++c) {
      Block& upper = blocks[(r + 1) * COLS + c]; // 上
      Block& lower = blocks[r * COLS + c];       // 下
      Net nV; nV.name = "N_V_" + upper.name + "_" + lower.name;
      nV.pins.push_back(&upper.pins[P_BOTTOM]);
      nV.pins.push_back(&lower.pins[P_TOP]);
      nets.push_back(std::move(nV));
    }
  }

  std::cout << "DummyData: masters=" << primitives.size()
            << " blocks="   << blocks.size()
            << " terms="    << terminals.size()
            << " nets="     << nets.size() << "\n";

  // 期望 18 条 net
  assert((int)nets.size() == 18 && "Expected 18 nets: 6 terminals + 12 adjacency");
}

