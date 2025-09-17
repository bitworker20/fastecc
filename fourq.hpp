#pragma once // 头文件保护

#include <array>    // for std::array
#include <cstdint>  // for uint8_t, uint32_t, uint64_t
#include <iosfwd>   // for std::ostream forward declaration
#include <string>   // for std::string
#include <vector>   // for std::vector
#include <cstring>

#include "utils.hpp" // for hex conversion utilities

// Include FourQ C headers (needed for types like digit_t, point_t, etc.)
#include "FourQlib/FourQ_64bit_and_portable/FourQ.h"
#include "FourQlib/FourQ_64bit_and_portable/FourQ_api.h"
#include "FourQlib/FourQ_64bit_and_portable/FourQ_internal.h"
#include "FourQlib/random/random.h"

// Define ECC_KEY_LENGTH based on FourQ types
#define ECC_KEY_LENGTH 32   //(sizeof(digit_t) * NWORDS_ORDER)

namespace Curve {
namespace FourQ {

// Forward declare Point for Scalar's friend declaration
class Point;

// Type Aliases
typedef std::array<digit_t, NWORDS_ORDER> fourq_scalar_t;
typedef std::array<uint8_t, ECC_KEY_LENGTH> EccDataType;

// --- Scalar Class Declaration ---
class Scalar {
private:
	friend class Point; // Point needs access to _b
	fourq_scalar_t _b;

	// Internal conversion helpers (implementation in .cpp)
	EccDataType toBytes(const fourq_scalar_t& w) const;
	fourq_scalar_t toWords(const EccDataType& b) const;

public:
	// Constructors (implementation in .cpp)
	Scalar(uint32_t val = 0);
	Scalar(const Scalar& br);
	Scalar(const std::string& ins);
	Scalar(const EccDataType& val);

	// Member Functions (implementation in .cpp)
	EccDataType getRaw() const;
	std::string toString() const;
	void fromString(const std::string& ins);
	size_t Size() const;
	bool isZero() const;
	Scalar& Sanitize(); // Ensure scalar is in the correct range

	// Assignment Operators (implementation in .cpp)
	Scalar& operator=(const Scalar& b);
	Scalar& operator=(const std::string& ins);

	// Comparison Operators (Inline for efficiency)
	inline friend bool operator==(const Scalar& lh, const Scalar& rh) {
		// Assuming fourq_scalar_t is POD and memcmp is appropriate
		return memcmp(lh._b.data(), rh._b.data(), sizeof(fourq_scalar_t)) == 0;
	}
	inline friend bool operator!=(const Scalar& lh, const Scalar& rh) {
		 return !(lh == rh); // Reuse operator==
	}
	
	// 添加operator<运算符
	inline friend bool operator<(const Scalar& lh, const Scalar& rh) {
		// 使用memcmp比较数据
		return memcmp(lh._b.data(), rh._b.data(), sizeof(fourq_scalar_t)) < 0;
	}

	// Arithmetic Friend Operators (declarations only, implementation in .cpp)
	friend Scalar operator+(const Scalar& lh, const Scalar& rh);
	friend Scalar operator-(const Scalar& lh, const Scalar& rh);
	friend Scalar operator*(const Scalar& lh, const Scalar& rh);
	friend Scalar operator/(const Scalar& lh, const Scalar& rh);

	// Stream Operator (declaration only, implementation in .cpp)
	friend std::ostream& operator<<(std::ostream& out, const Scalar& b);

	// Static Methods (implementation in .cpp)
	static Scalar invert(const Scalar& b);
	static Scalar negate(const Scalar& b);
	static Scalar getZero();
};


// --- Point Class Declaration ---
// Forward declare point_extproj struct if needed, though FourQ.h includes it
// struct point_extproj; // Or rely on FourQ.h
//typedef struct point_extproj *point_extproj_t; // Make the pointer type available

class Point {
private:
	point_extproj_t _pe;

public:
	// Constructors (implementation in .cpp)
	Point();
	Point(const Point& that);
	Point(Point&& that) noexcept; // Move constructor
	Point(const std::string& str);
	Point(const EccDataType& val);

	// Destructor (optional, only needed if managing resources like _pe memory)
	// ~Point(); // Likely not needed if _pe points to stack or static memory managed by FourQ

	// Member Functions (implementation in .cpp)
	EccDataType getRaw() const;
	std::string toString() const;
	void fromString(const std::string& str);
	bool isZero() const;

	// Assignment Operators (implementation in .cpp)
	Point& operator=(const Point& b);
	Point& operator=(Point&& b) noexcept; // Move assignment

	// Compound Assignment Operators (implementation in .cpp)
	Point& operator+=(const Point& that);
	Point& operator-=(const Point& that);
	Point& operator*=(const Scalar& b);

	// Special Operations (implementation in .cpp)
	Point MulAdd(const Scalar& mG, const Scalar& mP) const; // Note: Added const

	// Comparison Operators (declarations only, implementation in .cpp)
	// Pointer comparison is likely wrong, implement comparison logic in cpp
	friend bool operator==(const Point& lh, const Point& rh);
	friend bool operator!=(const Point& lh, const Point& rh);
	
	// 添加operator<运算符，用于支持std::set排序
	friend bool operator<(const Point& lh, const Point& rh);

	// Arithmetic Friend Operators (declarations only, implementation in .cpp)
	friend Point operator*(const Scalar& b, const Point& ep);
	friend Point operator-(const Point& lh, const Point& rh);
	friend Point operator+(const Point& lh, const Point& rh);

	// Stream Operator (declaration only, implementation in .cpp)
	friend std::ostream& operator<<(std::ostream& out, const Point& ep);

	// Static Methods (implementation in .cpp)
	static Scalar getOrder();
	static Point getBase();
	static Point negate(const Point& p0);
	static Point getZero();
	static Point mulBase(const Scalar& b);
};

template<typename T>
bool SchnorrQSign(const Scalar& secretKey, const T& msg, std::array<uint8_t, 64>& sig)
{
    auto skraw = secretKey.getRaw();
    auto pubkey = Point::mulBase(secretKey);
    auto pkraw = pubkey.getRaw();

    if (msg.size() == 0) {
        return false;
    }
    
    auto sret = ::SchnorrQ_Sign(skraw.data(), pkraw.data(), msg.data(), (unsigned int)msg.size(), sig.data());
    //auto sret = ::SchnorrQ_Sign(secretKey._b.data(), pkraw.data(), msg.data(), msg.size(), sig.data());
    return sret == ECCRYPTO_SUCCESS;
}

template<typename T>
bool SchnorrQVerify(const Point& pubkey, const T& msg, const std::array<uint8_t, 64>& sig)
{
    unsigned int valid = 0;
    auto pkraw = pubkey.getRaw();
    auto vret = ::SchnorrQ_Verify(pkraw.data(), msg.data(), (unsigned int)msg.size(), sig.data(), &valid);
    return vret == ECCRYPTO_SUCCESS && valid;
}

// 显式特例化声明（实现在.cpp中）
template<>
bool SchnorrQSign<std::string>(const Scalar& secretKey, const std::string& msg, std::array<uint8_t, 64>& sig);

template<>
bool SchnorrQVerify<std::string>(const Point& pubkey, const std::string& msg, const std::array<uint8_t, 64>& sig);

// 便利函数声明
bool SchnorrQSignMsg(const Scalar& secretKey, const std::vector<uint8_t>& msg, std::array<uint8_t, 64>& sig);
bool SchnorrQVerifyMsg(const Point& pubkey, const std::vector<uint8_t>& msg, const std::array<uint8_t, 64>& sig);

// --- Collection Typedefs ---
typedef std::vector<Scalar> Scalars;
typedef std::vector<Point> Points;

} // namespace FourQ
} // namespace Curve