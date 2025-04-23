# Changelog

All notable changes to mithril will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added

- redirect support, crawler config reading, improve url parsing, normalization (#15)
- robots.txt handling (#25)
- google test capabilities (#28)
- add spdlog (#32)
- request timeout and max response size (#33)
- serialize/deserialize documents (#37)
- port index constructor and close the loop with crawler (#38)
- static ranking for crawler (#41)
- persistent, interruptible crawling (#45)
- middle queue for rate limiting (#49)
- index v2 (positional indexing, perf improvs, field tagging, v1 isr) (#48)
- reject page based on `Content-Language` header (#52)
- split url queue into high and low scoring queue (#53)
- metrics for crawler (#50)
- async name resolution (#62)
- periodic crawler state snapshots (#63)
- index querying support with (docMapReader, ISRs, sep opt positions idx, termReader, termDict) etc and more index niceties (#60)
  - more details mostly in a index handoff doc soon
- parser improvements and html entity decoding (#66)
- url encoding and decoding, canonicalization (#68)
- add gzip support to requests (#69)
- split documents into chunk directories (#70)
- more document pruning and better configurability (#73)
- parse <meta> tags in document (#75)
- index perf improvements after adding postn index, doubled throughput (#76)
- query module with support for term queries and performance measurement
  - implemented `Query` and `TermQuery` classes for basic term searching
  - added `query_driver` utility for testing and benchmarking term queries
  - integrated performance timing and formatted output for query evaluation
- Add project website to `User-Agent` header of crawler (#78)
- Add blacklist-after-the-fact capabilities (#78)
- expose metric with number of documents in corpus (#80)
- support trailing wildcards in robots.txt (#87)
- respect crawl-delay directive (#88)
- use lru caches in crawler (#92)
- support Crawler-delay directive (#92)
- web server module (w threadpool, nonblocking io, and static file serving) and minimal snappy clean frontend (#97)
- url parsing additions (#98)
- add subdomain & extension crawler ranking factors (#99)
- discard urls with score < 0 (#101)
- create Dockerfile for crawler (#102)
- add numbers in URL factors to crawler ranking (#104)
- last minute crawler config changes (#103)
- global and robust host-level rate limiting (#107)
- dns-level rate limiting over host-level (#111)
- add bm25 stats support in index for stastic ranking and dev POC, actually fix tired merge, improve position (add field tags) and core indexing (performance, cleanup) (#112)
  - anubhav will finish this!
- more config and metrics for rate limits and caching (#113)
- more metrics and attempts at improvements (#114)
- old middle queue host cooldown combined with rate limit (#117)
- introduced static ranking (#115)
- add pagerank support for index (#118)
- crawler deployment updates (#120, #122)
- init dynamic ranker (#121)
- first pass of querycoordinator integration (w searchplugin impl) in client server, better js, etc (#127)
- integrate query engine/coordinator with client server and CLOSE!THE!LOOP! 🥳 (#128)
- integrate ranking into everything (#130)
- manual document crawling (#138)
- searching `nocache` will disable client side query caching (#142)
- added CLI option for binary output to `pagerank_sim` and no longer saves human readable output by default (#146)
- add boolean presence ranking flags (#139)
- Made GenericTermReader to union over all fields, not just search the body (#152)
- add basic multiterm support for ranking (#153)
- add scripts that made us build idx1(/5)
- query coverage, density + position based ranking (#159)
- Link server to mithril managers
- add order sensitive ranking & BM25 (#166)

### Fixed

- Fix copying body data from buffer (#24)
- Improve performance by splitting one lock into many (#34)
- improve robots.txt throughput (probably) (#54)
- fix getaddrinfo failure handling (#58)
- catch exceptions around string parsing (#61)
- use spec-compliant header parsing in connection machinery (#67)
- fix: allow gzip reads of size 0 (#72)
- remove problematic alloca for `const_cast` (#74)
- support desc field in commmon/doc, also test phrase searching (and based on position), make shouldstore pos less restrictive (#77)
- improve HTML parser handling of elements with attributes and comments (#78)
- make HTTP response header parsing more robust (#78)
- fix tracking of active robots requests (#78)
- correctly mark robots req as done after decode error (#79, #81)
- ensure valid status code range (#84)
- small header and cmake version fix to compile and get work on gcp vm (#86)
- gcp vm build, only use doc metadata in saving doc map to reduce mem use, ty @dsage (#90)
- fix: termReader freq, positions decoding as index encoded now (#91)
- fix connection eof empty headers bug (#92)
- perf-improvement: halve position index size + double throughput by more aggressive `shouldStorePos` + dsage invertedIdx `std::move(doc)` (#93)
- have anu's page rank stuff compile on VM (#95)
- cmake compile for apple clang + remove dupl compile warnings (#100)
- install libgomp1 in docker container (#106)
- clamp robots crawl delay to reasonable range (#109)
- check in flight robot requests at start of fill (#110)
- cleanup ranking & tests directory w.r.t to ranking tests (#116)
- fixed domain name whitelist being penalized + add `en.wikipedia.org` to whitelist for Crawler URL ranking (#115)
- normalize Static Ranker + fix some small issues (#119)
- only use fadvise dontneed on linux (#123)
- added command line option for specifying directory for `./pagerank_sim` (#124)
- index housekeeping to improve perf (now 100k in 40s), use sync points, termAnd improve (#125)
- fix compile issue on VM, use globalstopwords dict (does give some postnsize idx decrease), lib compile bug fix (#126)
- reduce pagerank memory usage (#133)
- ISRs now share a position index instead of creating their own (#134)
- better integration of ranking, query & index (#137)
- Fixed parser driver (#140)
- ISRs and Position Index now use memory mapped files instead of reading byte-by-byte from ifstreams (#141)
- Network support for query engine (#142)
- Fixed parser drive (#143)
- Changes from the runs on VM (perf/fixes for idx and ranking)(#132)
- Fixed mmap bug in QueryEngine (#144)
- Added favicon to front end (#150)
- Added frequency count for queries (#151)
- Added some code into QueryManager for token multiplicities (#152)
- Added support for config files on mithril_manager and wrote documentation for how to use (#153)
- better utf8 handling in json escaping for frontend (#161)
- Hooked up distributed backend with frontend web-server (#162)
- Sending urls and titles over network from backend (#163)
- Strip all HTML from title on frontend (#168)
- Fix hardcoded path (#171)

## [0.1.0] - 2025-02-07

### Added

- Basic in-memory crawler (#1).
- Non-blocking connect and SSL_connect (#11).
- Adjust URL frontier public API (#9).

### Fixed

- Don't add 1 to result of `EndingOfTag` without checking for `nullptr` (#10).

### Updated

- Update `lib` submodule and test include (#8).

### Infrastructure

- Initialize project structure.
- Add `clang-format` and `clang-tidy` configuration.
