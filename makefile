# ====== Config ======
CXX ?= g++
CXXFLAGS ?= -std=gnu++17 -O2 -Wall -Wextra -Wpedantic -Icode
LDFLAGS ?=
LDLIBS ?=

# Debug: make DEBUG=1
ifeq ($(DEBUG),1)
  CXXFLAGS := -std=gnu++17 -O0 -g -Wall -Wextra -Wpedantic -Icode
endif

# ====== Sources / Outputs ======
SRC := code/main.cpp code/datatype.cpp code/ILPPlace.cpp
OBJ := $(SRC:code/%.cpp=build/%.o)
BIN := bin/placer

# 确保目录存在
$(shell mkdir -p bin build >/dev/null)

# ====== 可选：ILP 求解器链接（按需开启） ======
# lpsolve（conda-forge: lpsolve55）
# CXXFLAGS += -I$(CONDA_PREFIX)/include
# LDFLAGS  += -L$(CONDA_PREFIX)/lib
# LDLIBS   += -llpsolve55
#
# GUROBI（需设 GUROBI_HOME）
# CXXFLAGS += -I$(GUROBI_HOME)/include
# LDFLAGS  += -L$(GUROBI_HOME)/lib
# LDLIBS   += -lgurobi_c++ -lgurobi

# ====== Rules ======
.PHONY: all build clean fmt run

all: $(BIN)

$(BIN): $(OBJ)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# 通用：code/xxx.cpp -> build/xxx.o
build/%.o: code/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 可选：运行（不加任何参数，由你在 main 里决定是否需要 argv）
run: $(BIN)
	./$(BIN)

clean:
	rm -rf build bin

fmt:
	clang-format -i $(SRC) code/*.h || true
