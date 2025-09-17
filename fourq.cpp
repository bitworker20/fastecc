#include "fourq.hpp"

#include <cstring>  // For memset, memcpy, memcmp
#include <stdexcept> // For std::runtime_error, std::invalid_argument
#include <string>    // For std::to_string in error messages
#include <vector>
#include <array>
#include <iostream> // For std::ostream

// Bring in FourQ C headers (needed for types and function declarations)
// extern "C" block might still be useful if headers lack it
extern "C" {
#include "FourQlib/FourQ_64bit_and_portable/FourQ.h"
#include "FourQlib/FourQ_64bit_and_portable/FourQ_api.h"
#include "FourQlib/FourQ_64bit_and_portable/FourQ_internal.h"
#include "FourQlib/FourQ_64bit_and_portable/FourQ_params.h" // Include params for curve_order
// Removed includes for .c files as they are now compiled separately
// #include "FourQlib/FourQ_64bit_and_portable/eccp2_core.c"
// #include "FourQlib/FourQ_64bit_and_portable/eccp2_no_endo.c"
// #include "FourQlib/FourQ_64bit_and_portable/crypto_util.c"
}


// --- Internal Helper Implementation ---
namespace { // Anonymous namespace for internal linkage

// _M64 Union for byte/word conversion (Consider endianness-safe alternatives if needed)
typedef union
{
  uint64_t u64;
  struct
  {
	uint8_t b0, b1, b2, b3, b4, b5, b6, b7;
  } u8;
} _M64;

// Byte reversal helper
// Note: Original function modified the output buffer directly and returned it.
// This version keeps that behavior. Ensure 'out' has sufficient space (32 bytes).
uint8_t* reverse_bytes(uint8_t* out, const uint8_t* in)
{
	for (int i = 0; i < 32; i++) {
		out[i] = in[31 - i];
	}
	return out;
}

} // anonymous namespace


namespace Curve {
namespace FourQ {

// --- Scalar Member Function Implementations ---

EccDataType Scalar::toBytes(const fourq_scalar_t& w) const {
	int i;
	_M64 m;
	EccDataType Y;
	// Assuming NWORDS_ORDER is 4, so ECC_KEY_LENGTH is 32
	for (i = 0; i < static_cast<int>(NWORDS_ORDER); ++i) {
		m.u64 = w[i];
		Y[i * 8 + 0] = m.u8.b0;
		Y[i * 8 + 1] = m.u8.b1;
		Y[i * 8 + 2] = m.u8.b2;
		Y[i * 8 + 3] = m.u8.b3;
		Y[i * 8 + 4] = m.u8.b4;
		Y[i * 8 + 5] = m.u8.b5;
		Y[i * 8 + 6] = m.u8.b6;
		Y[i * 8 + 7] = m.u8.b7;
	}
	return Y;
}

fourq_scalar_t Scalar::toWords(const EccDataType& b) const {
	int i;
	_M64 m;
	fourq_scalar_t Y;
	for (i = 0; i < static_cast<int>(NWORDS_ORDER); i++) {
		m.u8.b0 = b[i * 8 + 0];
		m.u8.b1 = b[i * 8 + 1];
		m.u8.b2 = b[i * 8 + 2];
		m.u8.b3 = b[i * 8 + 3];
		m.u8.b4 = b[i * 8 + 4];
		m.u8.b5 = b[i * 8 + 5];
		m.u8.b6 = b[i * 8 + 6];
		m.u8.b7 = b[i * 8 + 7];
		Y[i] = m.u64;
	}
	return Y;
}

Scalar::Scalar(uint32_t val) {
	_b.fill(0);
	_b[0] = val; // Assumes little-endian storage in digit_t array
	// Apply modulo order if val could exceed it? FourQ ops usually handle this.
}

Scalar::Scalar(const Scalar& br) {
	_b = br._b; // Use std::array's copy assignment
}

Scalar::Scalar(const std::string& ins) {
	// Delegate to fromString method
	fromString(ins);
}

Scalar::Scalar(const EccDataType& val) {
	fourq_scalar_t tmp = toWords(val);
	// Ensure .data() is used for C functions expecting pointers
	modulo_order(tmp.data(), _b.data());
}

EccDataType Scalar::getRaw() const {
	EccDataType ret = toBytes(_b);
	return ret;
}

std::string Scalar::toString() const {
	//EccDataType brev; // Not reversing bytes for scalar based on tests
	auto btmp = getRaw();
	// reverse_bytes(brev.data(), btmp.data());
	// return bytes_to_hex_string(brev);
	return bytes_to_hex_string(btmp);
}

void Scalar::fromString(const std::string& ins) {
	if (ins.size() != 2 * ECC_KEY_LENGTH) {
		 // Use std::to_string which requires <string>
		throw std::runtime_error("Invalid input scalar length: expected " + std::to_string(2 * ECC_KEY_LENGTH) + ", got " + std::to_string(ins.size()));
	}

	EccDataType btmp;
	// Use the utility from utils.hpp, it throws on error
	hex_string_to_bytes(ins, btmp);

	// Assuming no byte reversal needed for scalar based on tests
	auto tmp = toWords(btmp);
	// Reduce modulo order, ensure .data() is used
	modulo_order(tmp.data(), _b.data());
}

size_t Scalar::Size() const {
	 return ECC_KEY_LENGTH;
}

bool Scalar::isZero() const {
	// Check if all underlying words are zero
	for (const auto& word : _b) {
		if (word != 0) {
			return false;
		}
	}
	return true;
	// Original simpler check (assumes NWORDS_ORDER == 4):
	// return (_b[0] | _b[1] | _b[2] | _b[3]) == 0;
}

Scalar& Scalar::Sanitize() {
	// Reduce modulo order in place
	fourq_scalar_t tmp = _b; // Copy current value
	// Ensure .data() is used
	modulo_order(tmp.data(), _b.data()); // Reduce tmp into _b
	return *this;
}

Scalar& Scalar::operator=(const Scalar& b) {
	if (this != &b) { // Handle self-assignment
		_b = b._b;
	}
	return *this;
}

Scalar& Scalar::operator=(const std::string& ins) {
	fromString(ins); // Reuse fromString logic
	return *this;
}


// --- Scalar Friend Operator Implementations ---

Scalar operator+(const Scalar& lh, const Scalar& rh) {
	Scalar ret;
	add_mod_order(lh._b.data(), rh._b.data(), ret._b.data());
	return ret;
}

Scalar operator-(const Scalar& lh, const Scalar& rh) {
	Scalar ret;
	subtract_mod_order(lh._b.data(), rh._b.data(), ret._b.data());
	return ret;
}

Scalar operator*(const Scalar& lh, const Scalar& rh) {
	Scalar ret;
	fourq_scalar_t lt, rt;

	// Convert operands to Montgomery form
	to_Montgomery(lh._b.data(), lt.data());
	to_Montgomery(rh._b.data(), rt.data());

	// Perform Montgomery multiplication
	Montgomery_multiply_mod_order(lt.data(), rt.data(), ret._b.data());

	// Convert result back from Montgomery form
	from_Montgomery(ret._b.data(), ret._b.data());
	return ret;
}

Scalar operator/(const Scalar& lh, const Scalar& rh) {
	if (rh.isZero()) {
		throw std::runtime_error("Scalar division by zero");
	}
	// Restore missing/correct declarations
	Curve::FourQ::Scalar ret; // Ensure fully qualified type
	Curve::FourQ::fourq_scalar_t lt, rt_mont, ivt_mont; // Ensure fully qualified type

	// Convert operands to Montgomery form
	to_Montgomery(lh._b.data(), lt.data());
	to_Montgomery(rh._b.data(), rt_mont.data());

	// Calculate modular inverse in Montgomery form
	Montgomery_inversion_mod_order(rt_mont.data(), ivt_mont.data());

	// Perform Montgomery multiplication (effectively division)
	Montgomery_multiply_mod_order(lt.data(), ivt_mont.data(), ret._b.data());

	// Convert result back from Montgomery form
	from_Montgomery(ret._b.data(), ret._b.data());
	return ret;
}

std::ostream& operator<<(std::ostream& out, const Scalar& b) {
	out << b.toString(); // Use toString method
	return out;
}

// --- Scalar Static Method Implementations ---

Scalar Scalar::invert(const Scalar& b) {
	 if (b.isZero()) {
		throw std::runtime_error("Cannot invert zero scalar");
	}
	Scalar ret;
	fourq_scalar_t b_mont;

	to_Montgomery(b._b.data(), b_mont.data());
	Montgomery_inversion_mod_order(b_mont.data(), ret._b.data());
	from_Montgomery(ret._b.data(), ret._b.data());
	return ret;
}

Scalar Scalar::negate(const Scalar& b) {
	if (b.isZero()) {
		return Scalar(); // Negation of zero is zero
	}
	Scalar ret;
	// Calculate (order - b) mod order
	subtract_mod_order(curve_order, b._b.data(), ret._b.data());
	// The result of subtract_mod_order should already be reduced.
	// An extra modulo_order might be redundant but safe.
	// modulo_order(ret._b.data(), ret._b.data()); // Optional safeguard
	return ret;
}

Scalar Scalar::getZero() {
	// Default constructor creates zero
	return Scalar();
}


// --- Point Member Function Implementations ---

Point::Point() {
	// Initialize with the identity point (affine 0, 1)
	point_t zero_affine;
	fp2zero1271(zero_affine->x); // x = 0
	fp2zero1271(zero_affine->y); // y = 0
	zero_affine->y[0][0] = 1;    // Set y = 1
	point_setup(zero_affine, _pe); // Setup internal representation
}


Point::Point(const Point& that) {
	// Copy the array content
	memcpy(_pe, that._pe, sizeof(point_extproj_t));
}

Point::Point(Point&& that) noexcept { // Add noexcept
	// "Move" for a fixed-size array member is just a copy.
	memcpy(_pe, that._pe, sizeof(point_extproj_t));
	// No need to nullify that._pe as it's not a pointer.
}


Point::Point(const std::string& str) {
	// Delegate to fromString method. Ensure _pe is initialized before fromString modifies it.
	// The default Point() call isn't needed if fromString fully overwrites via point_setup.
	fromString(str); // fromString uses point_setup which writes to _pe
}

Point::Point(const EccDataType& val) {
	point_t pa;
	// Decode the bytes into an affine point representation
	if (decode(val.data(), pa) != ECCRYPTO_SUCCESS) { // Check return code
		 throw std::runtime_error("Point decoding failed");
	}
	// Setup the internal representation from the affine point
	point_setup(pa, _pe); // Writes to _pe

	// Validate the decoded point
	if (!ecc_point_validate(_pe)) { // Pass _pe (decays to pointer)
		 throw std::runtime_error("ecc_point_validate failed: decoded point not on curve or invalid");
	}
}

// Destructor not needed for array member _pe

EccDataType Point::getRaw() const {
	// No null check needed for array member
	EccDataType rd;
	point_t pa;
	// Normalize a copy to avoid modifying const object state and casting away const
	Point p_copy = *this;
	eccnorm(p_copy._pe, pa);

	// Encode the affine point into bytes - encode returns void
	encode(pa, rd.data());
	return rd;
}

std::string Point::toString() const {
	EccDataType brev;
	EccDataType raw = getRaw(); // Handles normalization and encoding
	// Apply byte reversal as per original logic for points
	reverse_bytes(brev.data(), raw.data());
	return bytes_to_hex_string(brev);
}

void Point::fromString(const std::string& str) {
	if (str.length() != 2 * ECC_KEY_LENGTH) {
		throw std::invalid_argument("Invalid input point length: expected " + std::to_string(2 * ECC_KEY_LENGTH) + ", got " + std::to_string(str.length()));
	}

	point_t pa;
	EccDataType brev, btmp;

	// Convert hex string to bytes, throws on error
	hex_string_to_bytes(str, btmp);

	// Apply byte reversal for points
	reverse_bytes(brev.data(), btmp.data());

	// Decode bytes into affine point
	decode(brev.data(), pa);

	// Setup internal representation (_pe)
	point_setup(pa, _pe); // Pass _pe (decays to pointer)

	// Validate the point
	if (ecc_point_validate(_pe) == false) { // Pass _pe
		throw std::runtime_error("ecc_point_validate failed: point from string not on curve or invalid");
	}
}


bool Point::isZero() const {
	// No null check needed

	point_t normalized_point;
	// Use a copy since eccnorm might modify the input (or needs non-const)
	Point p_copy = *this;
	eccnorm(p_copy._pe, normalized_point); // Pass the copy's _pe

	// Get the canonical zero point (identity) in affine form (0, 1)
	point_t normalized_canonical_zero;
	fp2zero1271(normalized_canonical_zero->x); // x = 0
	fp2zero1271(normalized_canonical_zero->y); // y = 0
	normalized_canonical_zero->y[0][0] = 1;    // Set y = 1

	// Compare affine coordinates using memcmp on underlying data
	bool x_equal = (memcmp(normalized_point[0].x, normalized_canonical_zero[0].x, sizeof(f2elm_t)) == 0);
	bool y_equal = (memcmp(normalized_point[0].y, normalized_canonical_zero[0].y, sizeof(f2elm_t)) == 0);

	return x_equal && y_equal;
	// Old memcmp method:
	// return (memcmp(&normalized_point, &normalized_canonical_zero, sizeof(point_t)) == 0);
}


Point& Point::operator=(const Point& b) {
	if (this != &b) {
		// Copy the array content
		memcpy(_pe, b._pe, sizeof(point_extproj_t));
	}
	return *this;
}

Point& Point::operator=(Point&& b) noexcept { // Ensure signature matches declaration
	 if (this != &b) {
		// "Move" is just a copy for array member
		memcpy(_pe, b._pe, sizeof(point_extproj_t));
	 }
	 return *this;
}

Point& Point::operator+=(const Point& that) {
	// No null check needed
	// Precompute for addition as required by eccadd
	point_extproj_precomp_t pthat_precomp;
	// R1_to_R2 needs non-const point_extproj*, use a copy of 'that' if needed
	Point that_copy = that;
	R1_to_R2(that_copy._pe, pthat_precomp); // Pass copy
	eccadd(pthat_precomp, _pe); // Pass _pe
	return *this;
}

Point& Point::operator-=(const Point& that) {
	// P -= Q  <=>  P += (-Q)
	*this += Point::negate(that);
	return *this;
}

Point& Point::operator*=(const Scalar& b) {
	// No null check needed
	point_t P_affine, Q_affine;

	// Normalize the current point (_pe) to affine P_affine
	eccnorm(_pe, P_affine); // Pass _pe

	// Perform scalar multiplication: Q_affine = b * P_affine
	// ecc_mul expects digit_t*, need to cast away const from b._b.data()
	if (!ecc_mul(P_affine, const_cast<digit_t*>(b._b.data()), Q_affine, false)) { // Use non-fixed base mul
		throw std::runtime_error("ecc_mul failed during Point::operator*=");
	}

	// Update internal representation (_pe) from the result Q_affine
	point_setup(Q_affine, _pe); // Pass _pe
	return *this;
}

Point Point::MulAdd(const Scalar& mG, const Scalar& mP) const {
	 // No null check needed
	 Point ret; // Result point
	 point_t pthis_affine, pr_affine; // Affine representations

	 // Normalize a copy of 'this' point to affine
	 Point p_copy = *this;
	 eccnorm(p_copy._pe, pthis_affine); // Use copy's _pe

	 // Perform double scalar multiplication: pr = mG*G + mP*pthis
	 // ecc_mul_double expects digit_t*, need to cast away const
	 if (!ecc_mul_double(const_cast<digit_t*>(mG._b.data()), pthis_affine, const_cast<digit_t*>(mP._b.data()), pr_affine)) {
		 throw std::runtime_error("ecc_mul_double failed in Point::MulAdd");
	 }

	 // Setup the result point's internal representation
	 point_setup(pr_affine, ret._pe); // Pass ret._pe
	 return ret;
}


// --- Point Friend Operator Implementations ---

bool operator==(const Point& lh, const Point& rh) {
	// Compare normalized raw byte representations for semantic equality
	return lh.getRaw() == rh.getRaw();
}

bool operator!=(const Point& lh, const Point& rh) {
	return !(lh == rh); // Reuse operator==
}

Point operator*(const Scalar& b, const Point& ep) {
	Point tmp = ep; // Make a copy
	tmp *= b;      // Use compound assignment
	return tmp;
}

Point operator-(const Point& lh, const Point& rh) {
	Point ret = lh; // Make a copy
	ret -= rh;      // Use compound assignment
	return ret;
}

Point operator+(const Point& lh, const Point& rh) {
	Point ret = lh; // Make a copy
	ret += rh;      // Use compound assignment
	return ret;
}

std::ostream& operator<<(std::ostream& out, const Point& ep) {
	out << ep.toString(); // Use toString method
	return out;
}

// --- Point Static Method Implementations ---

Scalar Point::getOrder() {
	Scalar r;
	// Copy the curve order from the FourQ constants
	// Ensure curve_order is accessible (likely included via FourQ_internal.h or similar)
	std::memcpy(r._b.data(), curve_order, sizeof(curve_order));
	return r;
}

Point Point::getBase() {
	point_t G0_affine;
	Point bp;
	eccset(G0_affine); // Correct usage for base point
	point_setup(G0_affine, bp._pe); // Pass bp._pe
	return bp;
}

Point Point::negate(const Point& p0) {
	if (p0.isZero()) { // Optimization: Negation of zero is zero
		return Point::getZero();
	}
	// Calculate -P as P * (order - 1)
	Scalar minus_one;
	subtract_mod_order(curve_order, Scalar(1)._b.data(), minus_one._b.data());
	return minus_one * p0; // Reuse operator*(Scalar, Point)
}

Point Point::getZero() {
	// Default constructor creates zero/identity
	return Point();
}

Point Point::mulBase(const Scalar& b) {
	point_t Q_affine; // Result in affine coordinates
	Point ret;

	// Perform fixed-base scalar multiplication: Q = b * G
	// ecc_mul_fixed expects digit_t*, need to cast away const
	if (!ecc_mul_fixed(const_cast<digit_t*>(b._b.data()), Q_affine)) {
		 throw std::runtime_error("ecc_mul_fixed failed in Point::mulBase");
	}

	// Setup the result point's internal representation
	point_setup(Q_affine, ret._pe); // Pass ret._pe
	return ret;
}

// Point comparison (<) - 添加此函数用于支持std::set
bool operator<(const Point& lh, const Point& rh) {
	// 转换为EccDataType后使用字典序比较
	EccDataType lraw = lh.getRaw();
	EccDataType rraw = rh.getRaw();
	return memcmp(lraw.data(), rraw.data(), lraw.size()) < 0;
}


template<>
bool SchnorrQSign(const Scalar& secretKey, const std::string& msg, std::array<uint8_t, 64>& sig)
{
	auto msgvector = std::vector<uint8_t>(msg.begin(), msg.end());
	return SchnorrQSign(secretKey, msgvector, sig);
}

template<>  
bool SchnorrQVerify(const Point& pubkey, const std::string& msg, const std::array<uint8_t, 64>& sig)
{
	auto msgvector = std::vector<uint8_t>(msg.begin(), msg.end());
	return SchnorrQVerify(pubkey, msgvector, sig);
}

bool SchnorrQSignMsg(const Scalar& secretKey, const std::vector<uint8_t>& msg, std::array<uint8_t, 64>& sig)
{
	return SchnorrQSign(secretKey, msg, sig);
}

bool SchnorrQVerifyMsg(const Point& pubkey, const std::vector<uint8_t>& msg, const std::array<uint8_t, 64>& sig)
{
	return SchnorrQVerify(pubkey, msg, sig);
}

} // namespace FourQ
} // namespace Curve
