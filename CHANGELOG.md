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

### Fixed
- Fix copying body data from buffer (#24)
- Improve performance by splitting one lock into many (#34)
- improve robots.txt throughput (probably) (#54)
- fix getaddrinfo failure handling (#58)
- catch exceptions around string parsing (#61)
- use spec-compliant header parsing in connection machinery (#67)
- fix: allow gzip reads of size 0 (#72)
- remove problematic alloca for const_cast (#74)

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
