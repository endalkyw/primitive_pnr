#include <iostream>
#include "datatype.h"
#include "ILPPlace.h"

int main() {
  std::cout << "[ILP] Analog ILP Placer started...\n";

  // 1) 准备数据（仅 dummy）
  CircuitData circuit;
  circuit.dummyData();

  // 2) 构造 placer 并运行（所有 setting 在 ILPPlace 内部完成）
  ILPPlace placer(circuit);
  placer.run();

  std::cout << "[ILP] Placement finished.\n";
  return 0;
}
