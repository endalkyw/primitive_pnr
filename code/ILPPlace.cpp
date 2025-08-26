#define _ISOC11_SOURCE 1
// 兜底，防止 libstdc++ 误探测：
#define _GLIBCXX_USE_TIMESPEC_GET 0
#define _GLIBCXX_HAVE_TIMESPEC_GET 0
#include "ILPPlace.h"
#include <ctime>         // 先让 glibc 正确声明 timespec_get
#include <highs/Highs.h> // 然后再包含 HiGHS
#include <iostream>
#include <vector>

ILPPlace::ILPPlace(CircuitData& data) : circuit(data) {}
static void demo_solve_with_highs_cpp() {
  Highs highs;

  // 构建一个极简 MIP：min x + y,  x>=1, y>=2, x,y 为整数
  HighsModel model;
  auto& lp = model.lp_;
  lp.num_col_   = 2;
  lp.num_row_   = 2;
  lp.col_cost_  = std::vector<double>{1.0, 1.0};
  lp.col_lower_ = std::vector<double>{0.0, 0.0};
  lp.col_upper_ = std::vector<double>{kHighsInf, kHighsInf};   // 注意：kHighsInf
  lp.row_lower_ = std::vector<double>{1.0, 2.0};
  lp.row_upper_ = std::vector<double>{kHighsInf, kHighsInf};

  // A 矩阵（按行存）：r0: 1*x，r1: 1*y
  lp.a_matrix_.format_ = MatrixFormat::kRowwise;
  lp.a_matrix_.start_  = {0, 1, 2};  // 每行起始 nnz 下标（2 行 -> 3 个值）
  lp.a_matrix_.index_  = {0, 1};     // 列索引
  lp.a_matrix_.value_  = {1.0, 1.0};

  // 交给求解器
  highs.passModel(model);

  // 把两列设置为整数（当前 API 逐列设置）
  highs.changeColIntegrality(0, HighsVarType::kInteger);
  highs.changeColIntegrality(1, HighsVarType::kInteger);

  highs.setOptionValue("output_flag", true);   // 需要安静就设 false
  HighsStatus st = highs.run();
  if (st != HighsStatus::kOk) {
    std::cerr << "HiGHS run failed\n";
    return;
  }

  const HighsInfo& info = highs.getInfo();
  const HighsSolution& sol = highs.getSolution();

  std::cout << "[HiGHS] obj=" << info.objective_function_value
            << " x=" << sol.col_value[0]
            << " y=" << sol.col_value[1] << "\n";
}


void ILPPlace::run() {
    std::cout << "Running ILP placer on " 
              << circuit.blocks.size() 
              << " blocks..." << std::endl;
    std::cout << "Running ILP placer (HiGHS demo) ...\n";
    demo_solve_with_highs_cpp();

    // TODO: formulate ILP problem
    // - unify height
    
    // - row-based placement constraints
    // - objective: e.g., HPWL minimization
}

void ILPPlace::printSolution() {
    std::cout << "Placement Solution:" << std::endl;
    for (const auto& block : circuit.blocks) {
        std::cout << "Block " << block.name 
                  << " at (" << block.position.x 
                  << ", " << block.position.y << ")\n";
    }
}

void ILPPlace::dummySolve() {  
}

