# Tasks from

https://books.google.com/books/about/Advanced_Programming_in_the_UNIX_Environ.html?id=kCTMFpEcIOwC

## 1. Prerequisites

Before building, ensure you have the following components installed on your Linux system:

- **OS**: Linux (required for `liburing`).
- **Compiler**: GCC 13+ or 14+ (required for full C++23 support).
- **CMake**: Version **3.20** or higher.
- **Ninja**: A build system for faster compilation.
- **Libraries**: `liburing`, `pkg-config`.

## 2. Installation (Ubuntu/Debian)

Run the following commands in your terminal to install the necessary dependencies:

### Step 1: Update system

```bash
sudo apt update
```

### Step 2: Install build tools and libraries

You need the compiler, cmake, ninja, pkg-config, and the development files for liburing.

```bash
sudo apt install -y build-essential g++ cmake pkg-config ninja-build liburing-dev
```

### Step 3: Verify GCC Version

This project strictly requires C++23. Check that your `g++` version is 13 or newer.

```bash
g++ --version
```

_If your version is older than 13 (e.g., on Ubuntu 20.04/22.04), you may need to install a newer version via the `ppa:ubuntu-toolchain-r/test` repository._

## 3. Building the Project

The project includes a `CMakePresets.json` file to simplify the configuration process.

### 1. Configuration

This command will create the build directory, check for required libraries, and generate build files using the `linux-debug` preset.

```bash
cmake --preset linux-debug
```

### 2. Compilation

Build the project using the generated configuration.

```bash
cmake --build out/build/linux-debug
```

## 4. Running

After a successful build, the executable files will be located in the `out/build/linux-debug` directory.

Example usage (replace `target_name` with the name of your source file without the extension):

```bash
./out/build/linux-debug/target_name
```
