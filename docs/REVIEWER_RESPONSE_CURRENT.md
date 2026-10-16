# Response to reviewer

## Reviewer comment

The previous major mathematical concerns can indeed be considered resolved.
The main obstacle: the published experimental package does not match what was claimed.
The status of the BL method contradicts the description of the completed experiments.
In their response to the second review, the authors state that:

- the experiments were re-run after the algorithms and the dwell-time rule were finalized;
- all three methods processed the same set of pre-recorded queries;
- the main study comprises 3,600 runs;
- the code, fixed queries, raw measurements, and verification results are available in the repository.

However, the file STATUS.md in the repository states essentially the opposite: BL currently uses its own execution mechanism and query generator; it is not yet integrated with reviewer_queries.csv and /run_experiments.py; and BL's results on urban data cannot be combined with the results from A/AP until a common fixed-query interface and a unified output format are implemented.
This discrepancy must be resolved.

## Response

Thank you for confirming the mathematical revisions and for identifying this inconsistency. We agree that the earlier package and our previous response did not give a consistent account of BL's integration status. We have corrected the status documentation and revised the experimental claims to match the available evidence.

In the revised replication package accompanying this response, scripts/run_experiments.py invokes the BL executable built from baseline_bl/fixed_query_runner.cpp. It reads the same processed network and the same fixed query row as A* and AP, using the --queries and --query-id arguments. The current entry point does not generate queries. The three methods use elapsed seconds, the query-level dwell_seconds threshold, and a common output schema. The root STATUS.md and baseline_bl/STATUS.md now describe this interface consistently.

The authoritative query file in this release is queries/final_queries_1260.csv. The current main-study configuration selects its 1,200 sensitivity queries and excludes the 60 additional default-setting queries. Thus, the common protocol specifies 1,200 queries per method, or 3,600 execution attempts across BL, A* and AP. The former reviewer_queries.csv filename is no longer the input identified by the revised protocol.

We have also corrected our earlier statement about the completed rerun. The supplied workbook contains 3,600 archived runtime records, with 1,200 records per method, including timeout outcomes. The available files do not establish that these measurements were produced by the corrected implementation and identical fixed inputs. Accordingly, the revised manuscript retains them as historical descriptive records, labels Figures 2-5 on that basis, and does not use them to claim a verified runtime advantage for the current release. We have removed the claim that the archive constitutes a verified rerun of all three finalized methods.

The package separately provides the current source, fixed inputs, original runtime workbook and CSV records, A*/AP verification outputs under results/rechecked_aap/, and exhaustive-instance verification under validation/seconds_instances/. Table 3 distinguishes the 3,600 historical runtime records from the 2,520 current A*/AP execution attempts and the 20-instance exhaustive comparison. The corresponding changes appear in Sections 5.1.2-5.1.4, 5.2 and 5.6, with matching qualifications in the abstract and conclusion.

These revisions correct both the stated BL integration status and the scope of the experimental claims. The current package no longer describes BL as awaiting the common interface, and the manuscript no longer presents the archived timings as verified executions of that corrected interface.
