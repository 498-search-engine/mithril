# mithril

> search engine system monorepo

## Build Instructions

*For Debian/Ubuntu VMs*

### 1. Dependencies  
```bash
sudo apt update && sudo apt install -y \
  build-essential \
  cmake \
  libssl-dev \
  zlib1g-dev \
  libboost-all-dev \
  git \
  fish \
  vim \
  gh
```

#### 1.1 Get CMake
```bash
wget https://github.com/Kitware/CMake/releases/download/v3.31.6/cmake-3.31.6-linux-x86_64.sh
chmod +x cmake-*.sh && sudo ./cmake-*.sh --skip-license --prefix=/usr/local
```

### 2. Build
```bash
git clone https://github.com/498-search-engine/mithril.git
cd mithril
git submodule update --init --recursive
cmake -DCMAKE_BUILD_TYPE=Release -S . -B build && cmake --build build -j$(nproc)
```
if

`C++20 Not Supported` | Check `g++ --version` ≥12.2.0  

#### tip
One-liner for fresh VMs:  
```bash
sudo apt update && sudo apt install -y build-essential cmake libssl-dev zlib1g-dev libboost-all-dev git && \
git clone https://github.com/your-org/mithril.git && cd mithril && \
cmake -DCMAKE_BUILD_TYPE=Release -S . -B build && cmake --build build -j$(nproc)
```

---

> "Works on my machine" isn't a fix.

if something breaks, contact @mdvsh or @dsage