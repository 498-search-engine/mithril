#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "Usage: $0 <shard_docs_dir> <shard_index_dir>"
  exit 1
fi

# Use environment variable if set, otherwise use the shared location
# You can set this in your .bashrc or before running the script
BIN_DIR="${MITHRIL_BIN_DIR:-/home/dnsge/mithril/bin}"
CONFIG_DIR="${MITHRIL_CONFIG_DIR:-/home/dnsge/mithril/bin/config}"

if [[ ! -d "$BIN_DIR" ]]; then
  echo "Error: Mithril binary directory not found at $BIN_DIR"
  echo "Either set MITHRIL_BIN_DIR environment variable or update the script."
  exit 1
fi

if [[ ! -d "$CONFIG_DIR" ]]; then
  echo "Error: Mithril config directory not found at $CONFIG_DIR"
  echo "Either set MITHRIL_CONFIG_DIR environment variable or update the script."
  exit 1
fi

DOCS_DIR="$1"
INDEX_DIR="$2"
SCRIPT_DIR="$(pwd)"

# ensure index output path exists
mkdir -p "$INDEX_DIR"

# Create config directory in current location if it doesn't exist
if [[ ! -d "./config" ]]; then
  echo "[$(date +%T)] ▶︎ Creating local config directory"
  mkdir -p "./config"
  
  # Copy or symlink config files from the source config directory
  echo "[$(date +%T)] ▶︎ Linking config files from $CONFIG_DIR"
  ln -sf "$CONFIG_DIR"/* "./config/"
fi

echo "[$(date +%T)] ▶︎ Running PageRank on $DOCS_DIR"
# "$BIN_DIR/pagerank_sim" "$DOCS_DIR"

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
