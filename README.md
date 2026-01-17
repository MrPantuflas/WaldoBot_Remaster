# WaldoBot_Remaster

## Installation Instructions

### Linux (Arch, Ubuntu/Debian, Rocky/Red Hat)

1. **Install build tools, yt-dlp, ffmpeg, and required libraries:**

    - **Arch Linux:**
      ```sh
      sudo pacman -S base-devel cmake yt-dlp ffmpeg openssl dpp libcurl-gnutls liboggz opus zlib
      ```

    - **Ubuntu/Debian:**
      ```sh
      sudo apt update
      sudo apt install build-essential cmake yt-dlp ffmpeg libssl-dev libcurl4-openssl-dev liboggz-dev opus-tools libopus-dev zlib1g-dev
      ```

    - **Rocky/Red Hat/Alma Linux:**
      ```sh
      sudo dnf groupinstall "Development Tools"
      sudo dnf install cmake ffmpeg openssl-devel libcurl-devel liboggz liboggz-devel opus-devel zlib-devel
      pip3 install --user yt-dlp
      ```

#### Note: DPP may not be available in standard repos, CMake will build from source if needed

2. **Build the project:**

    **Default build (uses shared DPP if available and compatible):**
    ```sh
    mkdir build && cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release
    make -j$(nproc)
    ```

    **Force building DPP from source:**
    ```sh
    mkdir build && cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release -DUSE_SHARED_DPP=OFF
    make -j$(nproc)
    ```

    **For Alma Linux 9 or systems with zlib detection issues:**
    ```sh
    cmake .. -DCMAKE_BUILD_TYPE=Release -DZLIB_LIBRARY=/usr/lib64/libz.so -DZLIB_INCLUDE_DIR=/usr/include
    make -j$(nproc)
    ```

    **Note:** CMake will automatically detect ABI compatibility issues with shared DPP and fall back to building from source if needed.

3. **Create a `.env` file in the `build` directory:**
    ```
    BOT_TOKEN=ODMx...
    ```

4. **Run the bot:**
    ```sh
    ./bot
    ```

---

### Windows

1. **Install build tools:**
    - Install [Visual Studio](https://visualstudio.microsoft.com/) with C++ development tools.
    - Install [CMake](https://cmake.org/download/).
    - Install [vcpkg](https://vcpkg.io/en/getting-started.html) for dependency management.

2. **Install dependencies via vcpkg:**
    ```sh
    vcpkg install dpp curl openssl zlib opus
    ```

3. **Install yt-dlp and ffmpeg:**
    - Download [yt-dlp](https://github.com/yt-dlp/yt-dlp#installation) and [ffmpeg](https://ffmpeg.org/download.html), and add them to your `PATH`.

4. **Build the project:**
    - Open a terminal (e.g., Developer Command Prompt for VS) and run:
      ```sh
      mkdir build && cd build
      cmake .. -DCMAKE_BUILD_TYPE=Release -DUSE_SHARED_DPP=ON -DCMAKE_TOOLCHAIN_FILE=[path to vcpkg]/scripts/buildsystems/vcpkg.cmake
      cmake --build . --config Release
      ```

5. **Create a `.env` file in the `build` directory:**
    ```
    BOT_TOKEN=ODMx...
    ```

6. **Run the bot:**
    ```sh
    .\bot.exe
    ```

## Build Options

- `-DUSE_SHARED_DPP=OFF`: Build DPP from source (default - most reliable)
- `-DUSE_SHARED_DPP=ON`: Use system-installed DPP library (faster but may have ABI compatibility issues)
- `-DZLIB_LIBRARY=/path/to/libz.so`: Manually specify zlib library path
- `-DZLIB_INCLUDE_DIR=/path/to/zlib/headers`: Manually specify zlib include directory

## Troubleshooting

- **Default approach**: Building DPP from source (default) is the most reliable method and works across different systems
- **ABI compatibility issues**: If using shared DPP fails with GLIBC_2.38/GLIBCXX errors, use the default build (DPP from source)
- **Missing DPP**: Shared DPP packages: `libdpp-dev` (Ubuntu/Debian), `dpp` (Arch), but building from source is recommended