#!/bin/bash
set -e

# Parameters: mount_index shard_start [shard_end]
# Example: ./schedule_builds.sh 4 0 14
# Default to idx4 if not specified
MOUNT_IDX=${1:-4}
# Default to shard 0 if not specified
SHARD_START=${2:-0}
# Default to shard 14 if not specified
SHARD_END=${3:-14}

# Get the script directory
SCRIPT_DIR="$(dirname "$(readlink -f "$0")")"
BUILD="${SCRIPT_DIR}/build_with_metrics.sh"

echo "Starting builds for shards ${SHARD_START}-${SHARD_END} on /mnt/idx${MOUNT_IDX}"

# Process one shard at a time
process_shard() {
    local shard=$1
    echo "Processing shard ${shard}..."
    "$BUILD" "/mnt/idx${MOUNT_IDX}/docs/shard_${shard}" "/mnt/idx${MOUNT_IDX}/indices/shard_${shard}"
    echo "Completed shard ${shard}"
}

# Process the first shard
process_shard "$SHARD_START"

# Process remaining shards
for ((shard = SHARD_START + 1; shard <= SHARD_END; shard++)); do
    process_shard "$shard"
done

echo "All builds completed for shards ${SHARD_START}-${SHARD_END} on /mnt/idx${MOUNT_IDX}"
