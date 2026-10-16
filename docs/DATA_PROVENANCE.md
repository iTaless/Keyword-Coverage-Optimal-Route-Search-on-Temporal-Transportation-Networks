# Data provenance

The exact Austin and Houston experimental snapshots are distributed in this repository under `data/raw/` and `data/processed/`. Publishing the prepared data alongside the fixed queries ensures that the inputs referenced by the result tables remain available even when an upstream feed changes.

| Snapshot | Stations | Scheduled edges |
| --- | ---: | ---: |
| Austin | 2,683 | 535,542 |
| Houston | 9,094 | 1,446,762 |

The processed tables are deterministic conversions of the checked-in raw edge and keyword files. `scripts/convert_austin_raw.py` assigns stable edge IDs from raw line numbers and converts `HHMMSS` values to integer seconds.

The MobilityData GTFS Validator (<https://github.com/MobilityData/gtfs-validator>) is software used to check GTFS-format data. It is not itself the experimental dataset. The prepared experimental inputs are available in this repository:

<https://github.com/iTaless/Keyword-Coverage-Optimal-Route-Search-on-Temporal-Transportation-Networks>

Before archival publication, the authors should add the original feed acquisition dates/versions and the applicable source/data licenses if those records are available. This package does not invent missing provenance metadata.

## Verified counts and namespace mapping

Austin mean scheduled out-degree is 535542/2683 = 199.6056653. Houston mean is 1446762/9094 = 159.0897295. Raw ID 0 corresponds to processed ID austin_e000001 or houston_e000001. Every raw-to-processed endpoint, cost and HHMMSS time conversion has been checked. This verifies the conversion, not the correctness of archived raw-clock duration arithmetic. See KNOWN_ISSUES.md.
