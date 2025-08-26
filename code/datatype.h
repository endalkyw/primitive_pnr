#ifndef DATATYPE_H
#define DATATYPE_H

#include <string>
#include <vector>
#include <deque>
#include <unordered_map>
#include <cmath>
#include <cassert>
#include <iostream>

// ---- 通用点类型 ----
template <typename T>
struct PointT { T x{0}, y{0}; };

using IPoint = PointT<int>;      // 用于 block 的栅格坐标
using DPoint = PointT<double>;   // 用于 pin offset / terminal 位置

struct Shape { int width{0}, height{0}; };

struct Primitive;
struct Block;
struct PinTemplate;
struct Terminal;
struct PinInst;

// Block/Terminal 固定模式 & 边信息
enum class FixMode { Free, FixedXY, EdgeSlide };
enum class Edge { None, Top, Bottom, Left, Right };

// ---------- master：器件库 ----------
struct PinTemplate {
    std::string name;
    DPoint offset; // 相对左下角，允许小数
};

struct Primitive {
    std::string name;
    Shape shape;   // 尺寸仍用整数
    std::vector<PinTemplate> pins;
    std::unordered_map<std::string, size_t> pin_index_by_name;
};

// ---------- 实例：Block ----------
struct PinInst {
    const PinTemplate* tmpl{nullptr};
    const Block*       block{nullptr}; // 若 pin 属于 Block
    const Terminal*    term{nullptr};  // 若 pin 属于 Terminal
    bool isBlock()    const { return block != nullptr; }
    bool isTerminal() const { return term  != nullptr; }
};

struct Block {
    std::string name;
    const Primitive* master{nullptr};
    IPoint position;                // 左下角：整数栅格
    std::vector<PinInst> pins;

    FixMode fix{FixMode::Free};
    Edge edge{Edge::None};
    int slide_min{0}, slide_max{0};

    int index{-1};

    DPoint pinAbsXY(size_t i) const;
    DPoint pinAbsXY(const std::string& pinName) const;
};

// ---------- Terminal 模板 & 实例 ----------
struct TerminalTemplate {
    std::string name;
    Shape shape{1,1};
    std::vector<PinTemplate> pins; // 通常 1 个 pin，offset 为 double
    Edge edge{Edge::Top};
    FixMode default_fix{FixMode::FixedXY};
    int default_slide_min{0}, default_slide_max{0};
    std::unordered_map<std::string, size_t> pin_index_by_name;
};

struct Terminal {
    std::string name;
    const TerminalTemplate* tmpl{nullptr};
    DPoint position;                // 左下角：允许小数，便于“中心对齐”
    std::vector<PinInst> pins;
    FixMode fix{FixMode::FixedXY};
    Edge edge{Edge::Top};
    int slide_min{0}, slide_max{0};

    DPoint pinAbsXY(size_t i) const;
    DPoint pinAbsXY(const std::string& pinName) const;
};

// ---------- Net ----------
struct Net {
    std::string name;
    std::vector<const PinInst*> pins; // 指向 Block/Terminal 的 PinInst*
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
    Block*    addBlock(const std::string& instName, const std::string& masterName, IPoint pos);
    Terminal* addTerminal(const std::string& instName, const std::string& tplName, DPoint pos,
                          FixMode fixOverride = FixMode::FixedXY,
                          int slideMin = 0, int slideMax = 0);
};

#endif // DATATYPE_H
