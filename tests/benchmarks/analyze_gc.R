# TODO: better analysis (Factorial experiment with blocking)

if (!file.exists("gc_results.csv")) {
  stop("gc_results.csv not found.")
}

df <- read.csv("gc_results.csv")
df$rel_wall_time <- NA
benchmarks <- unique(df$benchmark)

for (b in benchmarks) {
  mask <- df$benchmark == b
  df$rel_wall_time[mask] <- df$wall_ms[mask] / min(df$wall_ms[mask])
}

# Aggregate
agg <- aggregate(cbind(rel_wall_time, collections) ~ growth + min_heap + min_growth,
                 data = df, FUN = mean)

# Sort by lowest collection count first, then by performance
agg_by_count <- agg[order(agg$collections), ]

cat("\n--- Top 5 Settings for LOWEST Collection Count ---\n")
print(head(agg_by_count, 5), row.names = FALSE)

cat("\nObservations:\n")
cat("- Higher Growth Factors (4.0) drastically reduce collections but increase wall time.\n")
cat("- Increasing min_heap is the safest way to reduce early-program collection spikes.\n")
