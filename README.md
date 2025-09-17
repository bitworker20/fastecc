### fastecc

基于微软 FourQ 椭圆曲线库的快速 ECC 封装（C++20）。本项目将 FourQ C 实现打包为静态库，并提供简洁的 C++ 封装类型（`Scalar`、`Point`）以及 SchnorrQ 签名/验签便利接口。

- **目标产物**: `libfourq.a`
- **封装命名空间**: `Curve::FourQ`
- **支持功能**:
  - 标量（模阶）运算：加/减/乘/除、求逆、取负
  - 点运算：加/减、标量乘、固定基点乘 `mulBase`、双标量乘 `MulAdd`
  - SchnorrQ 签名与验签（封装模板与便捷函数）

## 目录结构

- 根目录
  - `fourq.hpp` / `fourq.cpp`: C++ 封装
  - `schnorrq_new.c`: SchnorrQ 实现（基于 FourQlib，做了安全性修订）
  - `utils.hpp`: 十六进制/字节转换工具
  - `CMakeLists.txt`: 构建配置
  - `test_fourq.cpp`: 单测（默认未在 CMake 中接线）
- `FourQlib/`
  - `FourQ_64bit_and_portable/`: 主要使用的 C 实现（含 `FourQ_api.h` 等）
  - `random/`, `sha512/`: 随机与 SHA-512
  - 其他平台实现与文档

## 构建

前置要求：
- CMake ≥ 3.20
- GCC/Clang（Linux）

快速构建：
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build -j
```

说明：
- 为避免 FourQ 在高优化下的潜在运行时问题，Release 默认对目标 `fourq` 强制降级到 `-O1`。
- 第三方源码可能产生若干编译告警（详见“告警与安全”）。

## 集成与使用

将 `build/libfourq.a` 与头文件加入你的工程。

CMake 示例：
```cmake
add_executable(demo main.cpp)
target_link_libraries(demo PRIVATE fourq)
target_include_directories(demo PRIVATE
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/FourQlib/FourQ_64bit_and_portable
)
```

示例（生成密钥、签名与验签）：
```cpp
#include "fourq.hpp"
#include <array>
#include <string>
#include <iostream>

int main() {
  using namespace Curve::FourQ;

  // 生成私钥与公钥
  EccDataType skraw{};
  ::random_bytes(skraw.data(), skraw.size());
  Scalar sk(skraw);
  Point pk = Point::mulBase(sk);

  // 签名与验签
  std::string msg = "Hello, FourQ!";
  std::array<uint8_t, 64> sig{};
  if (!SchnorrQSign(sk, msg, sig)) {
    std::cerr << "sign failed\n"; return 1;
  }
  bool ok = SchnorrQVerify(pk, msg, sig);
  std::cout << "verify: " << (ok ? "ok" : "fail") << "\n";
}
```

示例（点/标量运算）：
```cpp
using namespace Curve::FourQ;

Scalar a(2), b(3);
Scalar s = a * b;             // 6 (mod order)
Point  G  = Point::getBase();
Point  P  = a * G;            // 2*G
Point  Q  = b * G;            // 3*G
Point  R  = P + Q;            // 5*G
Point  E  = Point::mulBase(a + b);
bool   eq = (R.getRaw() == E.getRaw()); // true
```

## API 速览

- `Scalar`
  - 构造：`Scalar(uint32_t)`, `Scalar(std::string hex)`, `Scalar(EccDataType)`
  - 基本：`getRaw()`, `toString()`, `fromString()`, `Size()`, `isZero()`, `Sanitize()`
  - 算术：`+ - * /`, `invert`, `negate`, `getZero`
  - 比较：`== != <`
- `Point`
  - 构造：默认（单位元）、从 `std::string`/`EccDataType`
  - 基本：`getRaw()`, `toString()`, `fromString()`, `isZero()`
  - 算术：`+= -= *= (标量)`, `negate`, `mulBase`, `MulAdd(a,b)`（a*G + b*P）
  - 静态：`getBase()`, `getZero()`, `getOrder()`
  - 比较：`== != <`（按规范化编码比较）
- SchnorrQ
  - 模板：`SchnorrQSign<T>(sk,msg,sig)`, `SchnorrQVerify<T>(pk,msg,sig)`
  - 便捷：`SchnorrQSignMsg(vector<uint8_t>, ...)`, `SchnorrQVerifyMsg(...)`

注意：
- `Scalar::toString()` 为小端字节的十六进制；`Point::toString()`/`fromString()` 按 FourQ 约定做了字节反转处理。
- `ECC_KEY_LENGTH = 32` 字节。

## 测试

仓库包含 `test_fourq.cpp`（基于 GTest），但当前在 `CMakeLists.txt` 中默认注释未接线。你可自行启用：
- 取消注释测试目标与依赖，或将其迁入你的测试工程。

## 告警与安全

- 来自 `FourQ_params.h` 的“定义未使用”告警（如 A0/A1/b0/b1）是因为不同实现/优化路径下未被引用，通常无功能性影响。
- 对于 `-Wstringop-overflow` 一类告警，建议配合 ASan/UBSan 与 FourQlib 官方测试集进行运行验证。
- Release 构建对目标 `fourq` 强制 `-O1`，用于规避潜在 UB 或编译器优化问题；如要提升优化级别，务必先完善测试。
- SchnorrQ 建议采用官方流程：`k = H(SecretKey)`（64 字节），其中高 32 字节参与 nonce 派生。当前 `schnorrq_new.c` 已避免未初始化内存参与运算，若需完全对齐官方安全建议，请启用该哈希派生路径。

## 平台与宏

- 自动根据平台定义 `_AMD64_` 或 `_ARM64_`，并在 Linux 定义 `__LINUX__`。构建脚本已做平台宏设置。
- 主要使用 `FourQ_64bit_and_portable/` 实现，x86_64 与 AArch64 皆可构建。

## 许可证

- `FourQlib` 使用 MIT 许可证（见 `FourQlib/LICENSE`）。
- 本仓库 C++ 封装部分的许可证请根据你的需求补充（建议 MIT/Apache-2.0）。

## 常见问题（FAQ）

- 看到很多 “unused variable” 告警？
  - 属第三方源码正常现象，可在 `CMakeLists.txt` 针对 FourQlib 源增加 `-Wno-unused-variable -Wno-unused-const-variable` 局部抑制，不建议全局关闭。
- 消息很大时签名报错？
  - 底层签名/验签 API 的消息长度参数为 `unsigned int`。若 `msg.size()` 超过其上限，请先分段或自行处理上限检查。


