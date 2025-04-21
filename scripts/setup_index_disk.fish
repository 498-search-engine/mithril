#!/usr/bin/env fish

# Usage: ./setup_index_disk.fish <mount_path> <starting_chunk> <disk_num> <total_disks>
# Example for first disk: ./setup_index_disk.fish /mnt/idx1 0 1 5
# Example for second disk: ./setup_index_disk.fish /mnt/idx2 2000 2 5

if test (count $argv) -ne 4
    echo "Usage: ./setup_index_disk.fish <mount_path> <starting_chunk> <disk_num> <total_disks>"
    exit 1
end

# Inputs
set DEST_ROOT $argv[1]
set START_CHUNK $argv[2]
set DISK_NUM $argv[3]
set TOTAL_DISKS $argv[4]

# Constants
set NUM_SHARDS 15
set DOCS_PER_CHUNK 10000
set DOCS_PER_DISK 20000000  # 20M docs per disk
set CHUNKS_PER_DISK (math "$DOCS_PER_DISK / $DOCS_PER_CHUNK")  # 2000 chunks
set CHUNKS_PER_SHARD (math "ceil($CHUNKS_PER_DISK / $NUM_SHARDS)")  # ~134 chunks per shard
set DOCS_PER_SHARD (math "$CHUNKS_PER_SHARD * $DOCS_PER_CHUNK / 1000000")

echo "Setting up index disk #$DISK_NUM at: $DEST_ROOT"
echo "Disk will handle chunks $START_CHUNK to "(math "$START_CHUNK + $CHUNKS_PER_DISK - 1")
echo "Total docs on this disk: $DOCS_PER_DISK"
echo "Dividing into $NUM_SHARDS shards → $CHUNKS_PER_SHARD chunks per shard (~$DOCS_PER_SHARD M docs per shard)"

# Create directory structure
for shard in (seq 0 (math "$NUM_SHARDS - 1"))
    mkdir -p "$DEST_ROOT/docs/shard_$shard"
    mkdir -p "$DEST_ROOT/indices/shard_$shard"
end

# Link chunks
set SRC_DIR "/mnt/mithrildocs/docs"
set chunks_processed 0
set chunk_counter $START_CHUNK

for shard in (seq 0 (math "$NUM_SHARDS - 1"))
    set shard_dir "$DEST_ROOT/docs/shard_$shard"
    echo "Shard $shard: linking into $shard_dir"
    set shard_chunks 0

    while test $shard_chunks -lt $CHUNKS_PER_SHARD -a $chunks_processed -lt $CHUNKS_PER_DISK
        set chunk_name (printf "chunk_%010d" $chunk_counter)
        set src_path "$SRC_DIR/$chunk_name"

        if test -e "$src_path"
            ln -s $src_path "$shard_dir/$chunk_name"
            set shard_chunks (math "$shard_chunks + 1")
            set chunks_processed (math "$chunks_processed + 1")
            set chunk_counter (math "$chunk_counter + 1")
        else
            echo "Error: Missing $src_path - aborting!" >&2
            exit 1
        end
    end

    echo "  -> Linked $shard_chunks chunks to shard_$shard"
end

echo "Done. $chunks_processed chunks symlinked across $NUM_SHARDS shards."
echo "This disk now contains chunks $START_CHUNK to "(math "$chunk_counter - 1")
