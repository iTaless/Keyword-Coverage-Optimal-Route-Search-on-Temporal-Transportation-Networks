# Verification record

The supplied source was compiled with:

```text
g++ -std=c++17 -O3 -Wall -Wextra -Wpedantic main_with_bl.cpp -o kcrn_bl
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic test_bl.cpp -o test_bl
```

Checks completed:

1. An impossible transfer (arrival after the next departure) produces no route.
2. A POI contributes only when its dwell threshold is met.
3. Budget pruning and target dwell scoring are independently checked.
4. Top-k output contains distinct original scheduled-edge sequences.
5. On 100 seeded random temporal graphs, transformed BL matched exhaustive
   enumeration in route ordering and keyword-coverage score.
6. On the supplied 9,094-node / 1,446,762-trip dataset, an end-to-end smoke
   query completed. BL and A* returned the same original edge sequence, AP
   returned a feasible route, and every logged route passed independent
   validation.
7. On a 100-node induced subgraph of the supplied dataset, three validation
   queries produced matching optimum scores for A*, transformed BL, and
   exhaustive enumeration.

These checks establish implementation consistency; they do not replace the
full 30-query-per-setting experiment required for the manuscript.

