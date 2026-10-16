CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic

.PHONY: all test clean

all: build/route_search build/kcrn_bl build/test_bl

build:
	mkdir -p build

build/route_search: src/main.cpp | build
	$(CXX) $(CXXFLAGS) $< -o $@

build/kcrn_bl: baseline_bl/fixed_query_runner.cpp baseline_bl/bl_time_expanded.hpp baseline_bl/transit_router.hpp | build
	$(CXX) $(CXXFLAGS) $< -o $@

build/test_bl: baseline_bl/test_bl.cpp baseline_bl/bl_time_expanded.hpp baseline_bl/transit_router.hpp | build
	$(CXX) $(CXXFLAGS) $< -o $@

test: all
	python3 tests/test_small_oracle.py build/route_search
	build/test_bl
	python3 tests/test_fixed_query_interface.py build/route_search build/kcrn_bl
	python3 tests/test_seconds_boundary.py build/route_search build/kcrn_bl
	python3 tests/test_numeric_boundaries.py build/route_search build/kcrn_bl

clean:
	$(RM) -r build

