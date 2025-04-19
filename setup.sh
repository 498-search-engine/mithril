#!/bin/bash

cd mithril

mkdir build

cmake -DCMAKE_BUILD_TYPE=Release -S . -B build

cmake --build build

cd build

rm -rf data

mkdir data
mkdir data/snapshot
mkdir data/docs
mkdir data/state

# Run the crawler in the background
./crawler/mithril_crawler ../crawler/crawler.conf &

# Get the PID of the last background command (the crawler)
PID=$!

# Sleep for 30 seconds
sleep 30

# Terminate the crawler after 30 seconds
kill $PID

echo "Crawler terminated after 30 seconds"

# Define the directory containing the chunks
docs_dir="data/docs"

# Loop through each subdirectory (chunk) in data/docs/
for chunk in "$docs_dir"/*/; do
    # Check if it's a directory (chunk)
    if [ -d "$chunk" ]; then
        echo "Processing chunk: $chunk"

        # Get a list of all files (docs) in the chunk directory
        doc_files=("$chunk"*)

        # Loop through each doc file in the chunk
        for doc in "${doc_files[@]}"; do
            # Check if it's a file (not a subdirectory)
            if [ -f "$doc" ]; then
                # Extract the numeric part of the filename (e.g., doc_1234567890)
                base_name=$(basename "$doc")
                num_part=$(echo "$base_name" | sed 's/[^0-9]*//g')

                # Calculate the new filename (in increasing order with no gaps)
                new_name="doc_$(printf "%010d" $num_part)"

                # Rename the document file
                mv "$doc" "$chunk$new_name"
                # echo "Renamed $base_name to $new_name"
            fi
        done
    fi
done

echo "Renaming complete"

cd ranking

./pagerank_sim ../data/docs

../index/mithril_indexer ../data

mv index_output/ .
