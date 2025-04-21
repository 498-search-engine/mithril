#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "Usage: $0 <shard_docs_dir> <shard_index_dir>"
  exit 1
fi

DOCS_DIR="$1"
INDEX_DIR="$2"
SHARD_NAME=$(basename "$INDEX_DIR")

# metrics directory outside the index so it won't get clobbered
METRICS_DIR="/mnt/idx1/metrics"
mkdir -p "$METRICS_DIR"

# shard-specific filenames
VMSTAT_LOG="${METRICS_DIR}/vmstat_${SHARD_NAME}.log"
IOSTAT_LOG="${METRICS_DIR}/iostat_${SHARD_NAME}.log"
TIME_LOG="${METRICS_DIR}/time_${SHARD_NAME}.log"
SUMMARY_LOG="${METRICS_DIR}/summary_${SHARD_NAME}.log"

# Clear existing logs
> "$VMSTAT_LOG"
> "$IOSTAT_LOG"
> "$TIME_LOG"
> "$SUMMARY_LOG"

# Function to clean up background processes
cleanup() {
  # Kill background processes if they're still running
  kill "$VMSTAT_PID" "$IOSTAT_PID" 2>/dev/null || true
  # Wait for them to actually terminate
  wait "$VMSTAT_PID" "$IOSTAT_PID" 2>/dev/null || true
}

# Register cleanup function to run on script exit
trap cleanup EXIT

# Capture start time
START_TIME=$(date +%s)
echo "Starting build metrics collection at $(date)" >> "$SUMMARY_LOG"

# Start monitoring in background processes
# vmstat: Memory and CPU activity every second
vmstat 1 > "$VMSTAT_LOG" &
VMSTAT_PID=$!

# iostat: Disk I/O activity every second
iostat -dx 1 > "$IOSTAT_LOG" &
IOSTAT_PID=$!

# Give monitors a moment to start
sleep 1

# Run the builder under GNU time, capturing stderr to time log
echo "Starting index build for $SHARD_NAME at $(date)" >> "$SUMMARY_LOG"
set +e  # Temporarily disable exit on error for time command
/usr/bin/time -v ./barebones_builder.sh "$DOCS_DIR" "$INDEX_DIR" 2> "$TIME_LOG"
BUILD_STATUS=$?
set -e  # Re-enable exit on error

# Capture end time and calculate duration
END_TIME=$(date +%s)
DURATION=$((END_TIME - START_TIME))
echo "Build duration: ${DURATION} seconds" >> "$SUMMARY_LOG"
echo "Build exit status: ${BUILD_STATUS}" >> "$SUMMARY_LOG"

# Extract essential time metrics from GNU time output
echo "----- PERFORMANCE METRICS -----" >> "$SUMMARY_LOG"
{
  grep -E "User time|System time|Elapsed|Maximum resident|Page faults|File system" "$TIME_LOG" || 
  echo "Warning: Some time metrics not found" >> "$SUMMARY_LOG"
} >> "$SUMMARY_LOG"

# Create a simple analysis summary
echo "----- SYSTEM RESOURCE USAGE -----" >> "$SUMMARY_LOG"

# CPU metrics - with error handling
if [[ -s "$VMSTAT_LOG" ]]; then
  echo "CPU usage (user): $(awk 'NR>2 && $13~/^[0-9]+$/ {sum+=$13; count++} END {if(count>0) printf "%.1f%%", sum/count; else print "N/A"}' "$VMSTAT_LOG")" >> "$SUMMARY_LOG"
  echo "CPU usage (system): $(awk 'NR>2 && $14~/^[0-9]+$/ {sum+=$14; count++} END {if(count>0) printf "%.1f%%", sum/count; else print "N/A"}' "$VMSTAT_LOG")" >> "$SUMMARY_LOG"
  echo "CPU idle: $(awk 'NR>2 && $15~/^[0-9]+$/ {sum+=$15; count++} END {if(count>0) printf "%.1f%%", sum/count; else print "N/A"}' "$VMSTAT_LOG")" >> "$SUMMARY_LOG"
else
  echo "CPU metrics: No data available (vmstat log empty)" >> "$SUMMARY_LOG"
fi

# Memory metrics
if [[ -s "$VMSTAT_LOG" ]]; then
  echo "Average free memory: $(awk 'NR>2 && $4~/^[0-9]+$/ {sum+=$4; count++} END {if(count>0) printf "%.1f MB", sum/count/1024; else print "N/A"}' "$VMSTAT_LOG")" >> "$SUMMARY_LOG"
else
  echo "Memory metrics: No data available (vmstat log empty)" >> "$SUMMARY_LOG"
fi

# Disk I/O metrics
if [[ -s "$IOSTAT_LOG" ]]; then
  DISK_DEVICE=$(awk '/sd[a-z]/{print $1; exit}' "$IOSTAT_LOG" 2>/dev/null || echo "sdc")
  echo "Disk device: $DISK_DEVICE" >> "$SUMMARY_LOG"
  echo "Average disk reads: $(awk -v dev="$DISK_DEVICE" '$1==dev && $6~/^[0-9.]+$/ {sum+=$6; count++} END {if(count>0) printf "%.1f KB/s", sum/count; else print "N/A"}' "$IOSTAT_LOG")" >> "$SUMMARY_LOG"
  echo "Average disk writes: $(awk -v dev="$DISK_DEVICE" '$1==dev && $7~/^[0-9.]+$/ {sum+=$7; count++} END {if(count>0) printf "%.1f KB/s", sum/count; else print "N/A"}' "$IOSTAT_LOG")" >> "$SUMMARY_LOG"
else
  echo "Disk I/O metrics: No data available (iostat log empty)" >> "$SUMMARY_LOG"
fi

# Print only a minimal output to terminal about the metrics
echo
echo "Metrics for shard '$SHARD_NAME' written to:"
echo "  • $SUMMARY_LOG"

# Exit with the build status
exit $BUILD_STATUS
