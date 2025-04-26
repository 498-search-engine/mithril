The relevant ranking executables produced are:
- pagerank_sim <path to folder containing raw document data = docs> <output binary name = pagerank.bin>
This is automatically stored in the `bin` folder. It reads in raw document data to produce page rank binary file, which can later be used alongside the index.

Utilities
- pagerank_bench
Rough benchmark simulating 100M documents having random links between 4-100. No data is put in - this is just to measure time.

- pagerank_reader 
Reads in pagerank.bin and allows you to read pagerank value

- crawler_rank_test <input file>, crawler_rank_struct_test <input file>
Tests crawler ranking structures and outputs what their rank would be

- static_rank_test <input file>, static_rank_struct_test <input file>
Same for as above but for static ranking.