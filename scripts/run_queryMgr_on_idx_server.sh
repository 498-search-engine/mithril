#!/usr/bin/env bash
set -euo pipefail

# can do:
# ./run-indexer.sh 2 (serves all shards on idx2)
# serve only shards 0–3 and 7 on ID 4 on port 9000 (./run-indexer.sh 4 --port 9000 --shards 0-3,7)

[[ $# -ge 1 ]] || { echo "Usage: $0 <indexer-id> [--port PORT] [--bin-dir DIR] [--shards LIST]" >&2; exit 1; }

INDEXER_ID=$1; shift
PORT=8081
BIN_DIR=${MITHRIL_BIN_DIR:-"$HOME/mithril/bin"}
SHARD_SPEC=

while [[ $# -gt 0 ]]; do
  case $1 in
    --port)    PORT=$2;    shift 2 ;;
    --bin-dir) BIN_DIR=$2; shift 2 ;;
    --shards)  SHARD_SPEC=$2; shift 2 ;;
    *)         echo "Unknown option: $1" >&2; exit 1 ;;
  esac
done

ROOT="/mnt/idx${INDEXER_ID}/indices"
declare -a DIRS

if [[ -n $SHARD_SPEC ]]; then
  IFS=',' read -ra PARTS <<< "$SHARD_SPEC"
  for p in "${PARTS[@]}"; do
    if [[ $p == *-* ]]; then
      IFS=- read start end <<< "$p"
      for ((i=start; i<=end; i++)); do
        DIRS+=( "$ROOT/shard_$i" )
      done
    else
      DIRS+=( "$ROOT/shard_$p" )
    fi
  done
else
  for d in "$ROOT"/shard_*; do
    DIRS+=( "$d" )
  done
fi

ARGS=()
for d in "${DIRS[@]}"; do
  [[ -d $d ]] && ARGS+=(--index "$d")
done

(( ${#ARGS[@]} )) || { echo "No shards found under $ROOT" >&2; exit 2; }

exec "$BIN_DIR"/mithril_manager --port "$PORT" "${ARGS[@]}"

