#!/bin/bash

# The current shard number
SHARD=$1

# Execute the current build
./build_with_metrics.sh /mnt/idx1/docs/shard_${SHARD} /mnt/idx1/indices/shard_${SHARD}

# Calculate the next shard
NEXT_SHARD=$((SHARD + 1))

# If we haven't reached the end (shard 15), schedule the next one
if [ $NEXT_SHARD -le 14 ]; then
    # Schedule the next job to start immediately after this one finishes
    bash -c "$(realpath $0) $NEXT_SHARD" &
fi
