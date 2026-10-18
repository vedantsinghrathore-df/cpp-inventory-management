# Validation

Validated on 18 October 2026 using GCC 13.3.0 on Linux, with C++17.

## Build

Both the console app and test executable compiled successfully with:

```sh
-std=c++17 -Wall -Wextra -Wpedantic -Werror
```

No compiler warnings were emitted.

## Automated tests

**24 passed, 0 failed.** Tests cover initial state, valid products, duplicate IDs,
invalid IDs/names/quantities/prices, search, sorting, restocking, sales, overselling,
price history, removal, low stock, valuation, aggregate overflow, integer and money
parsing, persistence round trips, failed-save rollback, missing parent folders,
and malformed-file rejection without overwriting the original.

The 24 cases include multiple assertions and invalid-input examples. Checks use
explicit failures and remain active in release builds (not the `assert` macro).
Each test run uses its own temporary directory and never touches your inventory.

## Console integration checks

Passed: invalid menu input retry; adding a product; restocking; recording a sale;
correct sale total and inventory value; cancelling removal; restart and reload;
transaction display; clean EOF exit midway through input; and corrupt-file startup
failure preserving the file.

## Not validated here

Windows/MSVC compilation, macOS compilation, and the optional CMake workflow were
not executed in this environment. CMake was unavailable; direct GCC builds were
used. No multi-process or power-loss guarantees are claimed.
