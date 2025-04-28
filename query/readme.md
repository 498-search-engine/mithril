# Querying, Ranking, and Constraint Solving

## Testing End-to-End

There are various `*_driver` executables testing various components of the constraint solving and query compiling. All of these CLIs have help messages if you run them with no arguments.

To test the system fully, run `manager_driver`. Again, reference the help message. In general, it takes in index directory source(s) and spawns a worker(s) to serve those queries in an interactive terminal session.

```bash
$ manager_driver {idx1 path} {idx2 path} ...
```

You ***must*** run it from the `bin` directory for ranking to work.

Warning: memory-mapping can take a good minute.

Here is an example:

```bash
╭─user ~/docs/mithril  ‹main›
╰─➤ cd bin/
╭─user ~/docs/mithril/bin  ‹main›
╰─➤ ../build/query/manager_driver path/to/idx

[22:12:48.698] [info] Loading indices
[22:12:48.700] [info] Making Query Manager
[22:12:48.700] [info] about to query manager for path/to/idx
unable to mlock path/to/idx
[22:13:19.241] [info] loading posDict path/to/idx
[22:13:46.445] [info] constructing term dictionary for path/to/idx
[22:13:49.337] [info] Memory mapped term dictionary with 14790776 terms
[22:13:49.337] [info] loading document map path/to/idx/document_map.data
[22:14:22.333] [info] about to make query engine for path/to/idx
[22:14:22.333] [info] about to make bm25 for path/to/idx
[22:14:22.333] [info] about to load index stats for path/to/idx
[22:14:22.338] [info] Loaded index stats: 2458028 documents, avg body lengths: 1623.82
[22:14:22.338] [info] Constructed Query Manager with 1 workers
[22:14:22.338] [info] Now serving queries. Enter below...

>> 
```

You can then enter queries just like in the web page frontend, though you may need to escape some special characters to deal with UNIX shell behaviour.

For example:

```bash
>> cat AND dog

[22:18:11.641] [info] Serving query cat AND dog...
[22:18:11.642] [info] 🚀 Evaluating query: cat AND dog
[22:18:11.642] [info] ⭐ Parsing query: cat AND dog
[22:18:11.642] [info] ⭐ Query structure: AND(TERM(cat [WORD: cat]), TERM(dog [WORD: dog]))
Successfully loaded term 'cat' using dictionary lookup
Successfully loaded term '#cat' using dictionary lookup
Successfully loaded term '@cat' using dictionary lookup
Successfully loaded term '%cat' using dictionary lookup
Successfully loaded term 'dog' using dictionary lookup
Successfully loaded term '#dog' using dictionary lookup
Successfully loaded term '@dog' using dictionary lookup
Successfully loaded term '%dog' using dictionary lookup
[22:18:11.813] [info] Ranking results of size: 23264
[22:18:18.382] [info] Returning results of size: 50
[22:18:18.383] [info] Found 50 matches in 6740.74ms
Best: doc 1444630 https://www.petage.com/digital_guide/purina-digital-guide-dog-cat-essential-pet-care-needs/
```