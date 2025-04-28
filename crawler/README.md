# crawler

This directory contains the crawler component. Build the `mithril_crawler` CMake target. The crawler is configured with a `crawler.conf` file. This file also points to a `seed_list.conf` file and a `blacklisted_hosts.conf` file.

To run the crawler, invoke the binary as such: `./mithril_crawler path/to/crawler.conf`.

The `crawler.conf` file specifies a `docs_directory`, a `state_directory`, and a `snapshot_directory`. Make sure the directories all exist before starting the program.

You can interrupt the crawler with CTRL+C. The crawler will gracefully shutdown and can be restarted where it left off.
