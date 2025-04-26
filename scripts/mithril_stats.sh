#!/bin/bash
# mithril_stats.sh (works only on linux not macos)

format_number() {
  printf "%'d" $1 | sed ':a;s/\B[0-9]\{3\}\>/,&/;ta'
}

format_size() {
  local size=$1
  if [ $size -ge 1000000000 ]; then
    echo "$(echo "scale=2; $size/1000000000" | bc) GB"
  elif [ $size -ge 1000000 ]; then
    echo "$(echo "scale=2; $size/1000000" | bc) MB"
  else
    echo "$(echo "scale=2; $size/1000" | bc) KB"
  fi
}

parse_stats_file() {
  local stats_file=$1

  # Parse document count (4 bytes, uint32)
  doc_count=$(od -An -t u4 -j 0 -N 4 "$stats_file" | tr -d ' ')

  # Parse token counts (8 bytes each, uint64)
  body_tokens=$(od -An -t u8 -j 4 -N 8 "$stats_file" | tr -d ' ')
  title_tokens=$(od -An -t u8 -j 12 -N 8 "$stats_file" | tr -d ' ')
  url_tokens=$(od -An -t u8 -j 20 -N 8 "$stats_file" | tr -d ' ')
  desc_tokens=$(od -An -t u8 -j 28 -N 8 "$stats_file" | tr -d ' ')

  # Calculate total tokens
  total_tokens=$((body_tokens + title_tokens + url_tokens + desc_tokens))

  # Calculate index size
  index_dir=$(dirname "$stats_file")
  index_size=$(du -sb "$index_dir" | awk '{print $1}')

  # Print statistics
  echo "=== MITHRIL INDEX STATS ==="
  echo "Total documents: $(format_number $doc_count)"
  echo "Total tokens: $(format_number $total_tokens)"
  echo "  - Body tokens: $(format_number $body_tokens)"
  echo "  - Title tokens: $(format_number $title_tokens)"
  echo "  - URL tokens: $(format_number $url_tokens)"
  echo "  - Description tokens: $(format_number $desc_tokens)"
  echo "Total index size: $(format_size $index_size)"

  if [ $doc_count -gt 0 ]; then
    # Calculate averages
    avg_tokens=$(echo "scale=1; $total_tokens / $doc_count" | bc)
    avg_size=$(echo "scale=0; $index_size / $doc_count" | bc)
    echo "Avg tokens per doc: $avg_tokens"
    echo "Avg index size per doc: $(format_size $avg_size)"
  fi

  # Print component sizes
  for component in document_map.data final_index.data positions.data term_dictionary.data; do
    if [ -f "$index_dir/$component" ]; then
      component_size=$(stat -c %s "$index_dir/$component" 2>/dev/null || stat -f %z "$index_dir/$component")
      echo "$component: $(format_size $component_size)"
    fi
  done
}

analyze_cloud() {
  echo "Starting cloud analysis..."

  # Find all mount points
  mounts=()
  for i in {1..9}; do
    if [ -d "/mnt/idx$i" ]; then
      mounts+=("/mnt/idx$i")
    fi
  done

  if [ ${#mounts[@]} -eq 0 ]; then
    echo "No index mount points found!"
    exit 1
  fi

  echo "Found ${#mounts[@]} mount points: ${mounts[*]}"

  # Find all shards
  total_doc_count=0
  total_tokens=0
  total_body_tokens=0
  total_title_tokens=0
  total_url_tokens=0
  total_desc_tokens=0
  total_index_size=0
  valid_shards=0
  total_shards=0

  for mount in "${mounts[@]}"; do
    for shard_dir in "$mount"/indices/shard_*; do
      if [ -d "$shard_dir" ]; then
        total_shards=$((total_shards + 1))
        stats_file="$shard_dir/index_stats.data"

        if [ -f "$stats_file" ]; then
          echo "Processing $shard_dir..."

          # Parse document count
          shard_doc_count=$(od -An -t u4 -j 0 -N 4 "$stats_file" | tr -d ' ')

          # Parse token counts
          shard_body_tokens=$(od -An -t u8 -j 4 -N 8 "$stats_file" | tr -d ' ')
          shard_title_tokens=$(od -An -t u8 -j 12 -N 8 "$stats_file" | tr -d ' ')
          shard_url_tokens=$(od -An -t u8 -j 20 -N 8 "$stats_file" | tr -d ' ')
          shard_desc_tokens=$(od -An -t u8 -j 28 -N 8 "$stats_file" | tr -d ' ')

          # Calculate index size
          shard_index_size=$(du -sb "$shard_dir" | awk '{print $1}')

          # Add to totals
          total_doc_count=$((total_doc_count + shard_doc_count))
          total_body_tokens=$((total_body_tokens + shard_body_tokens))
          total_title_tokens=$((total_title_tokens + shard_title_tokens))
          total_url_tokens=$((total_url_tokens + shard_url_tokens))
          total_desc_tokens=$((total_desc_tokens + shard_desc_tokens))
          total_index_size=$((total_index_size + shard_index_size))

          valid_shards=$((valid_shards + 1))
        else
          echo "Skipping $shard_dir: No stats file found"
        fi
      fi
    done
  done

  total_tokens=$((total_body_tokens + total_title_tokens + total_url_tokens + total_desc_tokens))

  # Print aggregated statistics
  echo ""
  echo "=== MITHRIL CLOUD INDEX STATS ($valid_shards/$total_shards shards) ==="
  echo "Total documents: $(format_number $total_doc_count)"
  echo "Total tokens: $(format_number $total_tokens)"
  echo "  - Body tokens: $(format_number $total_body_tokens)"
  echo "  - Title tokens: $(format_number $total_title_tokens)"
  echo "  - URL tokens: $(format_number $total_url_tokens)"
  echo "  - Description tokens: $(format_number $total_desc_tokens)"
  echo "Total index size: $(format_size $total_index_size)"

  if [ $total_doc_count -gt 0 ]; then
    # Calculate averages
    avg_tokens=$(echo "scale=1; $total_tokens / $total_doc_count" | bc)
    avg_size=$(echo "scale=0; $total_index_size / $total_doc_count" | bc)
    echo "Avg tokens per doc: $avg_tokens"
    echo "Avg index size per doc: $(format_size $avg_size)"
  fi

  # Print a report-ready summary
  echo ""
  echo "=== REPORT SUMMARY ==="
  echo "• Number of shards: $valid_shards"
  echo "• Total documents indexed: $(format_number $total_doc_count)"
  echo "• Total tokens indexed: $(format_number $total_tokens) (~$(echo "scale=1; $total_tokens/1000000000" | bc) billion tokens)"
  echo "• Index storage footprint: $(format_size $total_index_size)"
  echo "• Average tokens per document: $avg_tokens"
  echo "• Average storage per document: $(format_size $avg_size)"

  # Calculate percentage breakdown of token types
  body_pct=$(echo "scale=1; 100 * $total_body_tokens / $total_tokens" | bc)
  title_pct=$(echo "scale=1; 100 * $total_title_tokens / $total_tokens" | bc)
  url_pct=$(echo "scale=1; 100 * $total_url_tokens / $total_tokens" | bc)
  desc_pct=$(echo "scale=1; 100 * $total_desc_tokens / $total_tokens" | bc)

  echo "• Token breakdown:"
  echo "  - Body: $body_pct%"
  echo "  - Title: $title_pct%"
  echo "  - URL: $url_pct%"
  echo "  - Description: $desc_pct%"
}

if [ "$1" = "--cloud" ]; then
  analyze_cloud
elif [ -f "$1" ]; then
  parse_stats_file "$1"
else
  echo "Usage: $0 [--cloud | path/to/index_stats.data]"
  echo "  --cloud: Analyze all cloud shards"
  echo "  path/to/index_stats.data: Analyze a single index stats file"
  exit 1
fi