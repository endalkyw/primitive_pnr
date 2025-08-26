#include "ILPPlace.h"
#include "datatype.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <cassert>

#include <highs/Highs.h>  // HiGHS C++ API

int ILPPlace::widthOf (int i) const { return circuit_.blocks[i].master->shape.width; }
int ILPPlace::heightOf(int i) const { return circuit_.blocks[i].master->shape.height; }


// ===== 构造 / 基础 =====
ILPPlace::ILPPlace(CircuitData& data) : circuit_(data) {}

// 这里给 cfg_ 一些保守默认值；也可以放到构造里
void ILPPlace::run() {
  // 默认配置（可以根据电路规模自适应）
  if (cfg_.layoutW == 0 || cfg_.layoutH == 0) {
    // 粗略给个边界：用块尺寸和数量估个盒子
    int w_sum = 0, h_sum = 0;
    for (const auto& b : circuit_.blocks) {
      w_sum += b.master->shape.width;
      h_sum += b.master->shape.height;
    }
    cfg_.layoutW = std::max(64, w_sum);   // 简单点：横向铺开
    cfg_.layoutH = std::max(64, h_sum/2); // 高度给个保守预算
  }

  // 组模型
  Highs highs;
  HighsModel model;

  // 1) 变量
  addVariablesToILP(model);
  // 2) 约束
  addConstraintsToILP(model);
  // 3) 目标
  setObjectiveFunctionToILP(model);

  // 求解器选项
  highs.setOptionValue("output_flag", cfg_.verboseSolver);
  highs.setOptionValue("presolve", "on");
  highs.setOptionValue("mip_detect_symmetry", "on");

  // 传模型并求解
  highs.passModel(model);
  for (int c : bin_cols_) highs.changeColIntegrality(c, HighsVarType::kInteger);
  if (cfg_.use2D) {
  for (int i = 0; i < (int)vars_.y_col.size(); ++i)
    highs.changeColIntegrality((int)vars_.y_col[i], HighsVarType::kInteger);
 }
  HighsStatus st = highs.run();

  last_.status_code = st;
  last_.solved = (st == HighsStatus::kOk);

  if (!last_.solved) {
    std::cerr << "[ILP] HiGHS run failed\n";
    return;
  }

  const HighsInfo& info = highs.getInfo();
  last_.objective = info.objective_function_value;

  // 取解向量并回写
  const HighsSolution& sol = highs.getSolution();
  writeBackSolutionFrom(model, sol.col_value);

  if (cfg_.verboseSolver) {
    std::cout << "[ILP] objective = " << last_.objective << "\n";
  }
  printSolution();
  
}
void ILPPlace::printSolution(){
  std::cout << "Placement Solution:\n";
  for (const auto& b : circuit_.blocks) {
    std::cout << "  Block " << b.name
              << " at (" << b.position.x
              << ", "  << b.position.y << ")\n";
  }
}


// ===== 内部工具 =====
double ILPPlace::computeBigM() const {
  int M = std::max(cfg_.layoutW, cfg_.layoutH);
  return std::max(1, M);
}

// 这里假设 datatype.h 里有 PinInst/Block/Terminal 定义
ILPPlace::PinAffX ILPPlace::pinAffineX(const PinInst* p) const {
  PinAffX r{};
  if (p->isBlock()) {
    const Block* b = p->block;
    const int bi   = b->index;                 // 需要在 dummy/load 后给 blocks[i].index = i
    r.blockIndex   = bi;
    r.x_col        = vars_.x_col[bi];
    r.c            = p->tmpl ? p->tmpl->offset.x : 0.0;  // double 偏移
    r.isConst      = false;
  } else {
    const Terminal* t = p->term;
    r.blockIndex   = -1;
    r.x_col        = -1;
    // terminal 左下角 + pad 偏移（都是 double）
    const double term_x = t ? t->position.x : 0.0;
    const double off_x  = p->tmpl ? p->tmpl->offset.x : 0.0;
    r.c            = term_x + off_x;
    r.isConst      = true;
  }
  return r;
}

ILPPlace::PinAffY ILPPlace::pinAffineY(const PinInst* p) const {
  PinAffY r{};
  if (p->isBlock()) {
    const Block* b = p->block;
    const int bi   = b->index;
    r.blockIndex   = bi;
    r.y_col        = vars_.y_col[bi];
    r.c            = p->tmpl ? p->tmpl->offset.y : 0.0;  // double 偏移
    r.isConst      = false;
  } else {
    const Terminal* t = p->term;
    r.blockIndex   = -1;
    r.y_col        = -1;
    const double term_y = t ? t->position.y : 0.0;
    const double off_y  = p->tmpl ? p->tmpl->offset.y : 0.0;
    r.c            = term_y + off_y;
    r.isConst      = true;
  }
  return r;
}



int ILPPlace::makeSepPairIndex(int i, int j) const {
  // 仅当 sep_pairs 是严格 i<j 顺序推入时，此映射才有意义。
  // 若你改成 push_back 动态收集，请改为存储 (i,j)->idx 的表。
  const int N = (int)circuit_.blocks.size();
  assert(0 <= i && i < j && j < N);
  // 块和：sum_{t=0}^{i-1} (N-1 - t) = i*(2*N - i - 1)/2
  int prefix = i * (2 * N - i - 1) / 2;
  return prefix + (j - (i + 1));
}

// ===== 建模三步 =====
void ILPPlace::addVariablesToILP(HighsModel& model) {
  auto& lp = model.lp_;
  const int N = (int)circuit_.blocks.size();

  // --- 1) x / y 列（保持你原来的写法） ---
  const bool use2D = cfg_.use2D;
  vars_.x_col.resize(N, -1);
  if (use2D) vars_.y_col.resize(N, -1);
  for (int i = 0; i < N; ++i) {
    vars_.x_col[i] = lp.num_col_++;
    lp.col_cost_.push_back(0.0);
    lp.col_lower_.push_back(0.0);
    lp.col_upper_.push_back(cfg_.layoutW);

    if (use2D) {
      vars_.y_col[i] = lp.num_col_++;
      lp.col_cost_.push_back(0.0);
      lp.col_lower_.push_back(0.0);
      lp.col_upper_.push_back(cfg_.layoutH);
    }
  }

  // --- 2) 四向二进制 L/R/B/T（保持你原来的写法；仅示意） ---
  vars_.sep_pairs.clear();
  bin_cols_.clear();
  for (int i = 0; i < N; ++i) {
    for (int j = i + 1; j < N; ++j) {
      ILPVars::Sep4 s{};
      s.i = i; s.j = j;
      // L
      s.L = lp.num_col_++;
      lp.col_cost_.push_back(0.0); lp.col_lower_.push_back(0.0); lp.col_upper_.push_back(1.0);
      bin_cols_.push_back((int)s.L);
      // R
      s.R = lp.num_col_++;
      lp.col_cost_.push_back(0.0); lp.col_lower_.push_back(0.0); lp.col_upper_.push_back(1.0);
      bin_cols_.push_back((int)s.R);
      // B
      s.B = lp.num_col_++;
      lp.col_cost_.push_back(0.0); lp.col_lower_.push_back(0.0); lp.col_upper_.push_back(1.0);
      bin_cols_.push_back((int)s.B);
      // T
      s.T = lp.num_col_++;
      lp.col_cost_.push_back(0.0); lp.col_lower_.push_back(0.0); lp.col_upper_.push_back(1.0);
      bin_cols_.push_back((int)s.T);

      vars_.sep_pairs.push_back(s);
    }
  }

  // --- 3) HPWL 线性化变量：对每个 net 建 xmin/xmax/(ymin/ymax) ---
  const int M = (int)circuit_.nets.size();
  vars_.net_span_cols.clear();
  vars_.net_span_cols.resize(M);
  for (int n = 0; n < M; ++n) {
    auto& span = vars_.net_span_cols[n];

    // xmin_n
    span.xmin = lp.num_col_++;
    lp.col_cost_.push_back(0.0);
    lp.col_lower_.push_back(0.0);            // 可更松：-inf，但 0 足够（坐标非负）
    lp.col_upper_.push_back(kHighsInf);

    // xmax_n
    span.xmax = lp.num_col_++;
    lp.col_cost_.push_back(0.0);
    lp.col_lower_.push_back(0.0);
    lp.col_upper_.push_back(kHighsInf);

    if (use2D) {
      // ymin_n
      span.ymin = lp.num_col_++;
      lp.col_cost_.push_back(0.0);
      lp.col_lower_.push_back(0.0);
      lp.col_upper_.push_back(kHighsInf);

      // ymax_n
      span.ymax = lp.num_col_++;
      lp.col_cost_.push_back(0.0);
      lp.col_lower_.push_back(0.0);
      lp.col_upper_.push_back(kHighsInf);
    }
  }

  vars_.num_cols_created = lp.num_col_;
}

void ILPPlace::addConstraintsToILP(HighsModel& model) {
  auto& lp = model.lp_;
  const int N  = (int)circuit_.blocks.size();
  const int M  = (int)circuit_.nets.size();
  const double Mbig = (cfg_.bigM > 0 ? cfg_.bigM : computeBigM());

  // 行式
  lp.a_matrix_.format_ = MatrixFormat::kRowwise;
  lp.num_row_ = 0;
  lp.a_matrix_.start_.clear();
  lp.a_matrix_.index_.clear();
  lp.a_matrix_.value_.clear();
  lp.row_lower_.clear();
  lp.row_upper_.clear();
  lp.a_matrix_.start_.push_back(0);

  // ========= 边界约束（保持你原来的实现） =========
  for (int i = 0; i < N; ++i) {
    // x_i <= layoutW - w_i
    lp.num_row_++;
    lp.row_lower_.push_back(-kHighsInf);
    lp.row_upper_.push_back(cfg_.layoutW - widthOf(i));
    lp.a_matrix_.index_.push_back(vars_.x_col[i]);
    lp.a_matrix_.value_.push_back(1.0);
    lp.a_matrix_.start_.push_back((int)lp.a_matrix_.index_.size());

    // -x_i <= 0  (x_i >= 0)
    lp.num_row_++;
    lp.row_lower_.push_back(-kHighsInf);
    lp.row_upper_.push_back(0.0);
    lp.a_matrix_.index_.push_back(vars_.x_col[i]);
    lp.a_matrix_.value_.push_back(-1.0);
    lp.a_matrix_.start_.push_back((int)lp.a_matrix_.index_.size());

    if (cfg_.use2D) {
      // y_i <= layoutH - h_i
      lp.num_row_++;
      lp.row_lower_.push_back(-kHighsInf);
      lp.row_upper_.push_back(cfg_.layoutH - heightOf(i));
      lp.a_matrix_.index_.push_back(vars_.y_col[i]);
      lp.a_matrix_.value_.push_back(1.0);
      lp.a_matrix_.start_.push_back((int)lp.a_matrix_.index_.size());

      // -y_i <= 0  (y_i >= 0)
      lp.num_row_++;
      lp.row_lower_.push_back(-kHighsInf);
      lp.row_upper_.push_back(0.0);
      lp.a_matrix_.index_.push_back(vars_.y_col[i]);
      lp.a_matrix_.value_.push_back(-1.0);
      lp.a_matrix_.start_.push_back((int)lp.a_matrix_.index_.size());
    }
  }

  // ========= 四向非重叠（修正后的 +M 版本；如用 assignment 可删） =========
  if (cfg_.fourWayNoOverlap) {
    for (const auto& s : vars_.sep_pairs) {
      const int i = s.i, j = s.j;
      const int wi = widthOf(i),  hi = heightOf(i);
      const int wj = widthOf(j),  hj = heightOf(j);

      // (1) -L -R -B -T <= -1
      lp.num_row_++;
      lp.row_lower_.push_back(-kHighsInf);
      lp.row_upper_.push_back(-1.0);
      lp.a_matrix_.index_.push_back(s.L); lp.a_matrix_.value_.push_back(-1.0);
      lp.a_matrix_.index_.push_back(s.R); lp.a_matrix_.value_.push_back(-1.0);
      lp.a_matrix_.index_.push_back(s.B); lp.a_matrix_.value_.push_back(-1.0);
      lp.a_matrix_.index_.push_back(s.T); lp.a_matrix_.value_.push_back(-1.0);
      lp.a_matrix_.start_.push_back((int)lp.a_matrix_.index_.size());

      // (2) x_i - x_j + M*L_ij <= -w_i + M
      lp.num_row_++;
      lp.row_lower_.push_back(-kHighsInf);
      lp.row_upper_.push_back(-wi + Mbig);
      lp.a_matrix_.index_.push_back(vars_.x_col[i]); lp.a_matrix_.value_.push_back( 1.0);
      lp.a_matrix_.index_.push_back(vars_.x_col[j]); lp.a_matrix_.value_.push_back(-1.0);
      lp.a_matrix_.index_.push_back(s.L);            lp.a_matrix_.value_.push_back( Mbig);
      lp.a_matrix_.start_.push_back((int)lp.a_matrix_.index_.size());

      // (3) x_j - x_i + M*R_ij <= -w_j + M
      lp.num_row_++;
      lp.row_lower_.push_back(-kHighsInf);
      lp.row_upper_.push_back(-wj + Mbig);
      lp.a_matrix_.index_.push_back(vars_.x_col[j]); lp.a_matrix_.value_.push_back( 1.0);
      lp.a_matrix_.index_.push_back(vars_.x_col[i]); lp.a_matrix_.value_.push_back(-1.0);
      lp.a_matrix_.index_.push_back(s.R);            lp.a_matrix_.value_.push_back( Mbig);
      lp.a_matrix_.start_.push_back((int)lp.a_matrix_.index_.size());

      if (cfg_.use2D) {
        // (4) y_i - y_j + M*B_ij <= -h_i + M
        lp.num_row_++;
        lp.row_lower_.push_back(-kHighsInf);
        lp.row_upper_.push_back(-hi + Mbig);
        lp.a_matrix_.index_.push_back(vars_.y_col[i]); lp.a_matrix_.value_.push_back( 1.0);
        lp.a_matrix_.index_.push_back(vars_.y_col[j]); lp.a_matrix_.value_.push_back(-1.0);
        lp.a_matrix_.index_.push_back(s.B);            lp.a_matrix_.value_.push_back( Mbig);
        lp.a_matrix_.start_.push_back((int)lp.a_matrix_.index_.size());

        // (5) y_j - y_i + M*T_ij <= -h_j + M
        lp.num_row_++;
        lp.row_lower_.push_back(-kHighsInf);
        lp.row_upper_.push_back(-hj + Mbig);
        lp.a_matrix_.index_.push_back(vars_.y_col[j]); lp.a_matrix_.value_.push_back( 1.0);
        lp.a_matrix_.index_.push_back(vars_.y_col[i]); lp.a_matrix_.value_.push_back(-1.0);
        lp.a_matrix_.index_.push_back(s.T);            lp.a_matrix_.value_.push_back( Mbig);
        lp.a_matrix_.start_.push_back((int)lp.a_matrix_.index_.size());
      }
    }
  }

  // ========= HPWL 线性化：对每个 net、每个 pin 约束 xmin/xmax/(ymin/ymax) =========
  for (int n = 0; n < M; ++n) {
    const auto& span = vars_.net_span_cols[n];
    // 保证 xmin <= xmax，ymin <= ymax
    {
      // xmin - xmax <= 0
      lp.num_row_++;
      lp.row_lower_.push_back(-kHighsInf);
      lp.row_upper_.push_back(0.0);
      lp.a_matrix_.index_.push_back(span.xmin); lp.a_matrix_.value_.push_back( 1.0);
      lp.a_matrix_.index_.push_back(span.xmax); lp.a_matrix_.value_.push_back(-1.0);
      lp.a_matrix_.start_.push_back((int)lp.a_matrix_.index_.size());
    }
    if (cfg_.use2D) {
      // ymin - ymax <= 0
      lp.num_row_++;
      lp.row_lower_.push_back(-kHighsInf);
      lp.row_upper_.push_back(0.0);
      lp.a_matrix_.index_.push_back(span.ymin); lp.a_matrix_.value_.push_back( 1.0);
      lp.a_matrix_.index_.push_back(span.ymax); lp.a_matrix_.value_.push_back(-1.0);
      lp.a_matrix_.start_.push_back((int)lp.a_matrix_.index_.size());
    }

    // 对 net n 的每个 pin，令 pinX = (var? x_b : 0) + c_x（double），同理 pinY
    for (const PinInst* p : circuit_.nets[n].pins) {
      // ------ X 方向：xmin <= pinX <= xmax ------
      {
        const PinAffX ax = pinAffineX(p);
        // xmin <= (var + c)  <=>  xmin - var <= c   （var 缺省为 0）
        lp.num_row_++;
        lp.row_lower_.push_back(-kHighsInf);
        lp.row_upper_.push_back(ax.c); // 右端是 double 常数
        // +1 * xmin
        lp.a_matrix_.index_.push_back(span.xmin);
        lp.a_matrix_.value_.push_back(1.0);
        // -1 * var
        if (!ax.isConst) {
          lp.a_matrix_.index_.push_back(ax.x_col);
          lp.a_matrix_.value_.push_back(-1.0);
        }
        lp.a_matrix_.start_.push_back((int)lp.a_matrix_.index_.size());

        // (var + c) <= xmax  <=>  var - xmax <= -c
        lp.num_row_++;
        lp.row_lower_.push_back(-kHighsInf);
        lp.row_upper_.push_back(-ax.c); // 右端是 -c
        if (!ax.isConst) {
          lp.a_matrix_.index_.push_back(ax.x_col);
          lp.a_matrix_.value_.push_back(1.0);
        }
        lp.a_matrix_.index_.push_back(span.xmax);
        lp.a_matrix_.value_.push_back(-1.0);
        lp.a_matrix_.start_.push_back((int)lp.a_matrix_.index_.size());
      }

      if (cfg_.use2D) {
        // ------ Y 方向：ymin <= pinY <= ymax ------
        const PinAffY ay = pinAffineY(p);
        // ymin <= (var + c)  <=>  ymin - var <= c
        lp.num_row_++;
        lp.row_lower_.push_back(-kHighsInf);
        lp.row_upper_.push_back(ay.c);
        lp.a_matrix_.index_.push_back(span.ymin);
        lp.a_matrix_.value_.push_back(1.0);
        if (!ay.isConst) {
          lp.a_matrix_.index_.push_back(ay.y_col);
          lp.a_matrix_.value_.push_back(-1.0);
        }
        lp.a_matrix_.start_.push_back((int)lp.a_matrix_.index_.size());

        // (var + c) <= ymax  <=>  var - ymax <= -c
        lp.num_row_++;
        lp.row_lower_.push_back(-kHighsInf);
        lp.row_upper_.push_back(-ay.c);
        if (!ay.isConst) {
          lp.a_matrix_.index_.push_back(ay.y_col);
          lp.a_matrix_.value_.push_back(1.0);
        }
        lp.a_matrix_.index_.push_back(span.ymax);
        lp.a_matrix_.value_.push_back(-1.0);
        lp.a_matrix_.start_.push_back((int)lp.a_matrix_.index_.size());
      }
    }
  }

  vars_.num_rows_created = lp.num_row_;
}



void ILPPlace::setObjectiveFunctionToILP(HighsModel& model) {
  auto& lp = model.lp_;

  // 清空旧的列目标（如果你之前给 x/y 赋过值，先归零）
  for (auto c : vars_.x_col) lp.col_cost_[c] = 0.0;
  if (cfg_.use2D) for (auto c : vars_.y_col) lp.col_cost_[c] = 0.0;

  // 只给 net span 列赋权：+1 * xmax，-1 * xmin；（y 同理）
  const int M = (int)vars_.net_span_cols.size();
  for (int n = 0; n < M; ++n) {
    const auto& s = vars_.net_span_cols[n];
    lp.col_cost_[s.xmax] += 1.0;
    lp.col_cost_[s.xmin] -= 1.0;
    if (cfg_.use2D) {
      lp.col_cost_[s.ymax] += 1.0;
      lp.col_cost_[s.ymin] -= 1.0;
    }
  }
}

void ILPPlace::writeBackSolutionFrom(const HighsModel& /*model*/,
                                     const std::vector<double>& col_value) {
  const int N = (int)circuit_.blocks.size();
  for (int i = 0; i < N; ++i) {
    const int xi = (int)std::llround(col_value[vars_.x_col[i]]);
    int yi = 0;
    if (cfg_.use2D) yi = (int)std::llround(col_value[vars_.y_col[i]]);
    circuit_.blocks[i].position.x = xi;
    circuit_.blocks[i].position.y = yi;
  }
}
