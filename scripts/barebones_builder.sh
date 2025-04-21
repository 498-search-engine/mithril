#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "Usage: $0 <shard_docs_dir> <shard_index_dir>"
  exit 1
fi

BIN_DIR="/home/madhavss/src/mithril/bin"
DOCS_DIR="$1"
INDEX_DIR="$2"

# ensure index output path exists
mkdir -p "$INDEX_DIR"

echo "[$(date +%T)] ▶︎ Running PageRank on $DOCS_DIR"
"$BIN_DIR/pagerank_sim" "$DOCS_DIR"

echo "[$(date +%T)] ▶︎ Building index at $INDEX_DIR"
# pagerank.bin still lives in pwd, so PageRankReader can open it
"$BIN_DIR/mithril_indexer" "$DOCS_DIR" --output="$INDEX_DIR" --force

# now safely move the bin file
if [[ -f pagerank.bin ]]; then
  echo "[$(date +%T)] ▶︎ Moving pagerank.bin → $INDEX_DIR/"
  mv pagerank.bin "$INDEX_DIR/"
else
  echo "Warning: pagerank.bin not found after indexing" >&2
fi

echo "[$(date +%T)] Done."

