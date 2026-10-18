# Inventory Management System

A C++17 console project for managing products, stock, prices, and transactions.
Uses object-oriented design, STL containers, validated input, and file persistence.
No third-party libraries or network access are required at runtime.

## Features

- Add products with unique positive IDs, names, stock, and prices.
- Search by case-insensitive name fragment or exact product ID.
- Restock products, record sales, update prices, and remove products with confirmation.
- View low-stock products and the total value of current inventory.
- Keep a UTC transaction history, including products that have been removed.
- Save automatically after every successful change and reload on startup.
- Validate quantities, prices, duplicate IDs, malformed input, and file records.
- Run 24 automated test cases without an external test framework.

Currency is INR. Prices are stored as integer paise, so `149.50` is stored as
`14950`. Sales record the price at the time of sale. The inventory-value report
uses current prices; it is not a profit report or an accounting ledger.

## Quick start on Windows (PowerShell)

You need a C++17 compiler. These commands assume `g++` is already available
in your terminal. Open the extracted `inventory-management` folder in VS Code,
then open its terminal and run:

```powershell
g++ -std=c++17 -Wall -Wextra -Wpedantic -Iinclude src/inventory.cpp src/main.cpp -o inventory.exe
.\inventory.exe
```

To run the tests:

```powershell
g++ -std=c++17 -Wall -Wextra -Wpedantic -Iinclude src/inventory.cpp tests/test_inventory.cpp -o inventory_tests.exe
.\inventory_tests.exe
```

If your terminal says `g++ is not recognized`, your compiler is not installed
or its `bin` folder is not on PATH. VS Code itself does not include a compiler.
You can also use Visual Studio's Developer Command Prompt:

```bat
cl /std:c++17 /EHsc /W4 /Iinclude src\inventory.cpp src\main.cpp /Fe:inventory.exe
inventory.exe
cl /std:c++17 /EHsc /W4 /Iinclude src\inventory.cpp tests\test_inventory.cpp /Fe:inventory_tests.exe
inventory_tests.exe
```

## Linux / macOS

From the extracted project folder:

```sh
g++ -std=c++17 -Wall -Wextra -Wpedantic -Iinclude src/inventory.cpp src/main.cpp -o inventory
./inventory
g++ -std=c++17 -Wall -Wextra -Wpedantic -Iinclude src/inventory.cpp tests/test_inventory.cpp -o inventory_tests
./inventory_tests
```

On macOS, `clang++` can replace `g++`.

## Optional CMake build

With CMake 3.16+ and a C++17 compiler installed:

```sh
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Run `./build/inventory` for a single-configuration build, or
`.\build\Release\inventory.exe` with Visual Studio's multi-configuration build.
The direct compiler commands above do not require CMake.

## Try it

1. Choose **2**, ID `101`, name `Keyboard`, stock `10`, price `999.50`.
2. Choose **4**, ID `101`, quantity `5` to restock.
3. Choose **5**, ID `101`, quantity `2` to sell. Sale total: INR `1999.00`.
4. Choose **1**: the keyboard should have `13` units remaining.
5. Choose **10**: stock value should be INR `12993.50`.
6. Choose **8** to see the add, restock, and sale records.
7. Choose **0**, restart, and choose **1** to verify persistence.

Invalid menu input is retried. Invalid operations show an error and return to
the menu. Closing input (Ctrl+Z then Enter on Windows, Ctrl+D on Unix) exits
cleanly. No extra save command is needed.

## Data file

The default is `inventory.db` in the terminal's current working directory.
To choose another location, pass one argument; the parent folder must exist:

```powershell
.\inventory.exe "C:\Users\YourName\Documents\inventory.db"
```

The database is a versioned text snapshot containing `P` product records,
`T` transaction records, and an `END` record with counts. Strings are quoted
and escaped. Every mutation writes a sibling `.tmp` file and replaces the old
snapshot only after the write succeeds. If saving fails, the operation is
rejected and the previous in-memory inventory is retained.

An invalid or truncated data file stops startup instead of silently clearing
it. Keep a backup and restore it manually if necessary. The format is meant
for this app; do not edit it while the app is running.

## Project structure

```text
include/inventory.hpp       Product, Transaction, and Inventory interfaces
src/inventory.cpp           Business rules, parsing, and persistence
src/main.cpp                Interactive console interface
tests/test_inventory.cpp   Automated tests (no external framework)
CMakeLists.txt              Optional CMake and CTest configuration
README.md                   Build, usage, design, and limitations
VALIDATION.md               Checks performed on this deliverable
```

## Design and limits

- `Inventory` encapsulates state and persistence. The UI calls its public methods.
- `std::map<int, Product>` provides unique IDs and sorted product listings.
- Mutations validate, copy state, append a transaction, save, and commit.
- Historical records preserve the product name and price at each event.
- IDs: positive `int`; stock: 0–1,000,000 per product.
- Price: INR 0–10,000,000,000.00. Aggregate value overflow is checked.
- Names: 1–100 bytes, no control characters; outer whitespace is trimmed.
- Limits: 100,000 current products and 1,000,000 transactions.
- Intended for a small, local, single-process college project. Do not run two
  instances against the same file. There is no locking or multi-user support.
- The snapshot is rewritten and state copied on each mutation, so performance
  scales with total products and history. A database would suit larger workloads.
- File replacement helps avoid partial snapshots during normal saves, but the
  app does not promise recovery after power loss, disk failure, or manual edits.
- Search case folding is byte-based and intended for English names. It is not
  full Unicode case-insensitive search.
- Removing a product removes its remaining stock; its history is retained.
  Reusing an ID starts a new current product while old named events remain.
- No authentication, tax calculation, barcode integration, or profit tracking.

## Explain it in an interview

Start with the `Product` and `Transaction` structs, then show how `Inventory`
separates business logic from the console menu. Walk through `sell`: validate
stock, create a proposed state, append a historical record, save, and commit.
Explain integer paise, duplicate-ID checks, failed-save rollback, and the
round-trip test. Then discuss how SQLite and a repository interface could
replace the full-file snapshot for a larger system.

Suggested resume description, after you run and understand the project:

> Built a C++17 console inventory system using OOP, STL containers, and file
> handling; implemented product search, stock operations, sales history, input
> validation, and 24 automated test cases.
