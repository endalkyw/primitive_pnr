#ifndef DATATYPE_H
#define DATATYPE_H

#include <string>
#include <vector>
#include <deque>
#include <unordered_map>
#include <cmath>
#include <cassert>

// ---------- 通用 ----------
struct Point { int x{0}, y{0}; };
struct Shape { int width{0}, height{0}; };

struct Primitive;   // master（器件库）
struct Block;       // 实例
struct PinTemplate; // master 里的 pin 模板
struct PinInst;     // 实例化 pin

// Block/Terminal 固定模式 & 边信息
enum class FixMode { Free, FixedXY, EdgeSlide };
enum class Edge { None, Top, Bottom, Left, Right };

// ---------- master：器件库 ----------
struct PinTemplate {
    std::string name;
    Point offset; // 相对左下角
};

struct Primitive {
    std::string name;
    Shape shape;
    std::vector<PinTemplate> pins;
    std::unordered_map<std::string, size_t> pin_index_by_name;
};

// ---------- 实例：Block ----------
struct PinInst {
    const PinTemplate* tmpl{nullptr};
    const Block* parent{nullptr};
};

struct Block {
    std::string name;
    const Primitive* master{nullptr};
    Point position;
    std::vector<PinInst> pins;

    FixMode fix{FixMode::Free};
    Edge edge{Edge::None};
    int slide_min{0}, slide_max{0}; // EdgeSlide 时的一维滑动区间

    Point pinAbsXY(size_t i) const;
    Point pinAbsXY(const std::string& pinName) const;
};

// ---------- Terminal 模板 & 实例 ----------
struct TerminalTemplate {
    std::string name;          // e.g. "VDD_TOP", "GND_BOTTOM"
    Shape shape{1,1};          // IO pad 尺寸
    std::vector<PinTemplate> pins; // 通常 1 个 pin
    Edge edge{Edge::Top};      // 预期边
    FixMode default_fix{FixMode::FixedXY};
    int default_slide_min{0}, default_slide_max{0};
    std::unordered_map<std::string, size_t> pin_index_by_name;
};

struct Terminal {
    std::string name;                 // 实例名，如 "T_VDD"
    const TerminalTemplate* tmpl{nullptr};
    Point position;                   // 绝对坐标
    std::vector<PinInst> pins;        // 实例化的 pins
    FixMode fix{FixMode::FixedXY};
    Edge edge{Edge::Top};
    int slide_min{0}, slide_max{0};

    Point pinAbsXY(size_t i) const;
    Point pinAbsXY(const std::string& pinName) const;
};

// ---------- Net ----------
struct Net {
    std::string name;
    std::vector<const PinInst*> pins; // Block/Terminal 的 PinInst*
};

// ---------- 顶层容器 ----------
struct CircuitData {
    // masters
    std::deque<Primitive> primitives;
    std::deque<TerminalTemplate> term_templates;

    // instances
    std::deque<Block> blocks;
    std::deque<Terminal> terminals;

    std::vector<Net> nets;

    // 索引
    std::unordered_map<std::string, Primitive*>        primitive_by_name;
    std::unordered_map<std::string, TerminalTemplate*> term_tpl_by_name;

    // I/O
    void loadFromFile(const std::string& filename);
    void dummyData();

    // 索引构建
    void buildPrimitiveIndex();
    void buildTerminalTemplateIndex();

    // 实例化
    Block*    addBlock(const std::string& instName, const std::string& masterName, Point pos);
    Terminal* addTerminal(const std::string& instName, const std::string& tplName, Point pos,
                          FixMode fixOverride = FixMode::FixedXY,
                          int slideMin = 0, int slideMax = 0);
};

#endif // DATATYPE_H
