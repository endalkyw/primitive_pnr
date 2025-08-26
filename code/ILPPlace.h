#ifndef ILPPLACE_H
#define ILPPLACE_H


#include <utility>
#include <vector>
struct CircuitData;
struct PinInst;
struct HighsModel;    // 来自 HiGHS
enum class HighsStatus : int; // 可选：仅作返回码占位

// 放置问题配置
struct ILPConfig {
  int layoutW = 0;            // 版图宽度（栅格单位）
  int layoutH = 0;            // 版图高度（栅格单位）
  bool use2D = true;          // 是否做 2D 放置（x,y 全整数变量）
  bool fourWayNoOverlap = true; // 非重叠采用四向析取（L/R/B/T）
  double bigM = -1.0;         // Big-M（<=0 则自动推导）
  bool verboseSolver = true;  // HiGHS 日志
};

struct ILPVars {
  // block 位置
  std::vector<long long> x_col;    // size = N
  std::vector<long long> y_col;    // size = N（use2D==true 时有效）

  // 非重叠：四向二进制，按 (i,j) 存一条记录（仅 i<j）
  struct Sep4 { long long L, R, B, T; int i, j; };
  std::vector<Sep4> sep_pairs;     // size = N*(N-1)/2

  // HPWL 线性化
  struct NetSpan { long long xmin, xmax, ymin, ymax; };
  std::vector<NetSpan> net_span_cols; // size = |nets|

  // 统计
  int num_cols_created = 0;  // 总列数（变量个数）
  int num_rows_created = 0;  // 约束行数（建模后填）
};
struct ILPResult {
  bool solved = false;      // 是否成功求解
  double objective = 0.0;   // 目标值（总 HPWL）
  HighsStatus status_code = (HighsStatus)0; // 可选：原始状态码
};
class ILPPlace {
public:
    explicit ILPPlace(CircuitData& data);
    void setConfig(const ILPConfig& cfg);
    bool place_blocks_with_ilp();
    void run();
    void printSolution() const;
    ILPResult getLastResult() const { return last_; }
    void printSolution(); // print placement solution
    

private:

    CircuitData& circuit_;
    ILPConfig    cfg_;
    ILPVars      vars_;
    ILPResult    last_;
    std::vector<int> bin_cols_;

    void addVariablesToILP(HighsModel& model);
    void addConstraintsToILP(HighsModel& model);
    void setObjectiveFunctionToILP(HighsModel& model);
    void writeBackSolutionFrom(const HighsModel& model,
                                const std::vector<double>& col_value);

    double computeBigM() const;

    int widthOf (int i) const;   // <--- 只声明
    int heightOf(int i) const;   // <--- 只声明

    struct PinAffX { int blockIndex; long long x_col; double c; bool isConst; };
    struct PinAffY { int blockIndex; long long y_col; double c; bool isConst; };
    PinAffX pinAffineX(const PinInst* p) const;  // <--- 会用 double
    PinAffY pinAffineY(const PinInst* p) const;  // <--- 会用 double

    int makeSepPairIndex(int i, int j) const;

    

};

#endif
