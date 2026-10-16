# Verification evidence

See `tests/` for BL unit, fixed-input, seconds-boundary and fractional-budget checks. `validation/seconds_instances/` records 20 processed-second instances and 200 ranks on which BL and A* both match independent exhaustive enumeration. This is bounded implementation validation, not a proof of historical runtime provenance or a performance comparison with an upstream KOR implementation.

Archived recovery uses original raw integer clocks and raw row-index IDs. Its 5,567 sequences are retained under `results/final/bl_supplementary/`, with actual source ZIPs and original adapter. Current processed IDs/times cannot be inferred by merely renaming those old clocks.
