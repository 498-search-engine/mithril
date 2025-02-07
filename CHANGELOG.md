# Changelog

All notable changes to mithril will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added
- Add support for custom user agents (#12).
- Implement rate limiting for crawler (#13).

### Fixed
- Resolve memory leak in URL parsing (#14).

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