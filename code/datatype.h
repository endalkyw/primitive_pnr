#ifndef DATATYPE_H
#define DATATYPE_H

#include <string>
#include <vector>
#include <deque>   // 使用 deque 保证元素地址稳定
#include <cmath>

// 前向声明
struct Primitive;
struct Pin;

struct Point {
    int x{0};
    int y{0};
};

struct Shape {
    int width{0};
    int height{0};
};

struct Pin {
    std::string name;
    Point offset;              // 相对 cell 左下角
    Primitive* parent{nullptr}; // 反向指向所属 cell（便于求绝对坐标）
};

// Primitive cell
struct Primitive {
    std::string name;
    Point position;           // 左下角坐标
    Shape shape;              // 宽高（高度统一的 row-base 模式）
    std::deque<Pin> pins;     // 用 deque 保证 Pin* 稳定

    // 工具：获取 pin 的绝对坐标（不做越界检查，调用方保证 index 合法）
    Point pinAbsXY(size_t pinIdx) const {
        const Pin& p = pins[pinIdx];
        return { position.x + p.offset.x, position.y + p.offset.y };
    }
};

struct Net {
    std::string name;
    std::vector<Pin*> pins;   // 直接存 Pin*，后续 HPWL/ILP 取坐标更高效
};

// 顶层电路
struct CircuitData {
    std::deque<Primitive> primitives; // 用 deque 保证 Primitive* 稳定
    std::vector<Net> nets;
    std::vector<Pin*> terminals;      // 顶层端口（可挂在虚拟 primitive 上，或独立 pin 池）
    Shape layoutSize;                 // 布局区域大小

    // I/O
    void loadFromFile(const std::string& filename);
    void dummyData(); // 快速造数用

    // 解析完文本后，建立 pins 的 parent 指针和 net 的 Pin* 链接
    // （比如先读到 "CellName.PinName" 暂存，随后 resolve）
    void resolveConnectivity();
};

#endif // DATATYPE_H
