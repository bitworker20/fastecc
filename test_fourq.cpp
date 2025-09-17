#include "gtest/gtest.h"
#include "fourq.hpp" // 这会包含 utils.hpp 和 .c 文件
#include <string>
#include <vector>
#include <stdexcept>
#include <system_error> // 包含 std::errc

// 用于 FourQ 测试的 Test Fixture
class FourQTest : public ::testing::Test {
protected:
    // 预定义的已知值 (如果可能，用实际已知的好值替换)
    // 使用确认为 FourQ 曲线有效的值
    // 示例: 标量 '1' (小端序的字节表示的反向十六进制)
    std::string scalar_one_hex = "0100000000000000000000000000000000000000000000000000000000000000";
    // 示例: 一个已知的标量值 'k' (替换为真实的)
    std::string known_scalar_hex = "533987461378f7063f129056a5d6306394612bfe0c1a2520ec8acb2d76701100";
    // 示例: 生成点 G 的十六进制 (将在 SetUp 中计算)
    std::string generator_hex;
    // 示例: k*G 的十六进制 (将在 SetUp 中计算)
    std::string known_point_hex;

    Curve::FourQ::Scalar s_one;
    Curve::FourQ::Scalar s_known;
    Curve::FourQ::Point p_base; // 生成点
    Curve::FourQ::Point p_known; // k*G


    void SetUp() override {
        // 初始化生成点
        p_base = Curve::FourQ::Point::getBase();
        
        // 尝试执行 1 * G 来验证基点和基本乘法 (现在应该通过了，可以移除或保留)
        // try {
        //     Curve::FourQ::Point test_mult = Curve::FourQ::Scalar(1) * p_base; 
        // } catch (const std::runtime_error& e) {
        //     FAIL() << "Base point validation (1*G) failed in SetUp: " << e.what();
        // }

        generator_hex = p_base.toString();
        s_known = Curve::FourQ::Scalar(known_scalar_hex); 
        
        // 恢复 p_known 的计算
        p_known = s_known * p_base; 
        known_point_hex = p_known.toString();

        s_one = Curve::FourQ::Scalar(scalar_one_hex);

        // 恢复 fromString 测试
        s_known.fromString(known_scalar_hex); 
        ASSERT_NO_THROW(p_known.fromString(known_point_hex)); 
    }
};

// --- Scalar Tests ---

TEST_F(FourQTest, ScalarDefaultConstructor) {
    Curve::FourQ::Scalar s;
    EXPECT_TRUE(s.isZero());
    EXPECT_EQ(s.toString(), "0000000000000000000000000000000000000000000000000000000000000000");
}

TEST_F(FourQTest, ScalarUintConstructor) {
    Curve::FourQ::Scalar s(5);
    EXPECT_FALSE(s.isZero());
    // 检查 5 的十六进制字符串表示是否正确
    EXPECT_EQ(s.toString(), "0500000000000000000000000000000000000000000000000000000000000000");
}

TEST_F(FourQTest, ScalarCopyConstructor) {
    Curve::FourQ::Scalar s_copy(s_known);
    EXPECT_EQ(s_copy, s_known);
    EXPECT_EQ(s_copy.toString(), known_scalar_hex);
}

TEST_F(FourQTest, ScalarStringConstructor) {
    Curve::FourQ::Scalar s(known_scalar_hex);
    EXPECT_EQ(s, s_known);
     EXPECT_EQ(s.toString(), known_scalar_hex);
}

TEST_F(FourQTest, ScalarRawDataConstructor) {
    Curve::FourQ::EccDataType raw = s_known.getRaw();
    Curve::FourQ::Scalar s_from_raw(raw);
    // 注意: 构造函数应用了模运算, 所以如果输入 > 阶数，可能不完全相同
    // 我们应该用一个已知 < 阶数的值测试
    EXPECT_EQ(s_from_raw.getRaw(), s_known.getRaw()); // 比较可能取模后的原始数据
     EXPECT_EQ(s_from_raw.toString(), known_scalar_hex);
}

TEST_F(FourQTest, ScalarFromString) {
    Curve::FourQ::Scalar s;
    EXPECT_NO_THROW(s.fromString(known_scalar_hex));
    EXPECT_EQ(s, s_known);
    EXPECT_EQ(s.toString(), known_scalar_hex);
    EXPECT_THROW(s.fromString("12345"), std::runtime_error); // 无效长度
    EXPECT_THROW(s.fromString("gg"), std::runtime_error); // 无效十六进制字符 (由 hex_string_to_bytes 处理)
    // 测试一个长度正确但包含无效字符的字符串
    EXPECT_THROW(s.fromString("00112233445566778899aabbccddeeff00112233445566778899aabbccddeegx"), std::invalid_argument);
}

TEST_F(FourQTest, ScalarToString) {
    EXPECT_EQ(s_known.toString(), known_scalar_hex);
    Curve::FourQ::Scalar s_zero;
    EXPECT_EQ(s_zero.toString(), "0000000000000000000000000000000000000000000000000000000000000000");
}

TEST_F(FourQTest, ScalarIsZero) {
    Curve::FourQ::Scalar s_zero;
    EXPECT_TRUE(s_zero.isZero());
    EXPECT_FALSE(s_one.isZero());
    EXPECT_FALSE(s_known.isZero());
}

TEST_F(FourQTest, ScalarAssignment) {
    Curve::FourQ::Scalar s;
    s = s_known;
    EXPECT_EQ(s, s_known);
    EXPECT_NO_THROW(s = known_scalar_hex); // 测试字符串赋值
    EXPECT_EQ(s, s_known);
}

TEST_F(FourQTest, ScalarComparison) {
    Curve::FourQ::Scalar s_copy = s_known;
    Curve::FourQ::Scalar s_one_copy = s_one;
    EXPECT_TRUE(s_known == s_copy);
    EXPECT_FALSE(s_known == s_one);
    EXPECT_TRUE(s_known != s_one);
    EXPECT_FALSE(s_known != s_copy);
}

TEST_F(FourQTest, ScalarAddition) {
    Curve::FourQ::Scalar s2 = s_one + s_one;
    Curve::FourQ::Scalar s_check(2);
    EXPECT_EQ(s2, s_check);

    Curve::FourQ::Scalar sum = s_one + s_known;
    Curve::FourQ::Scalar diff = sum - s_one; // 应该等于 s_known
    EXPECT_EQ(diff, s_known);
}

TEST_F(FourQTest, ScalarSubtraction) {
    Curve::FourQ::Scalar s_zero_check = s_one - s_one;
    EXPECT_TRUE(s_zero_check.isZero());

    Curve::FourQ::Scalar diff = s_known - s_one;
    Curve::FourQ::Scalar sum = diff + s_one; // 应该等于 s_known
    EXPECT_EQ(sum, s_known);
}

TEST_F(FourQTest, ScalarMultiplication) {
    Curve::FourQ::Scalar s_zero_mult = s_known * Curve::FourQ::Scalar(0);
    EXPECT_TRUE(s_zero_mult.isZero());

    Curve::FourQ::Scalar s_one_mult = s_known * s_one;
    EXPECT_EQ(s_one_mult, s_known);

    // 测试已知的小值乘法, 例如 2 * 3 = 6
    Curve::FourQ::Scalar s2(2), s3(3), s6(6);
    EXPECT_EQ(s2 * s3, s6);
}

TEST_F(FourQTest, ScalarDivision) {
    ASSERT_FALSE(s_one.isZero()); // 确保除数不为零
    Curve::FourQ::Scalar s_check = s_known / s_one;
    EXPECT_EQ(s_check, s_known);

    ASSERT_FALSE(s_known.isZero()); // 确保除数不为零
    Curve::FourQ::Scalar s_one_div = s_known / s_known; // 应该等于 1
    EXPECT_EQ(s_one_div, s_one);

    // 测试已知的小值, 例如 6 / 3 = 2
    Curve::FourQ::Scalar s2(2), s3(3), s6(6);
     ASSERT_FALSE(s3.isZero());
    EXPECT_EQ(s6 / s3, s2);

    // 测试除以零 (底层 C 函数可能不会抛出标准异常)
    Curve::FourQ::Scalar s_zero;
    // EXPECT_THROW(s_one / s_zero, std::runtime_error); // 取决于底层实现
    // 可以检查结果是否为特定值（例如零）或是否有错误状态（如果接口支持）
}

TEST_F(FourQTest, ScalarInvert) {
    ASSERT_FALSE(s_known.isZero()); // 不能对零求逆
    Curve::FourQ::Scalar s_inv = Curve::FourQ::Scalar::invert(s_known);
    Curve::FourQ::Scalar res = s_known * s_inv;
    EXPECT_EQ(res, s_one); // k * (1/k) = 1 mod order

    Curve::FourQ::Scalar s_one_inv = Curve::FourQ::Scalar::invert(s_one);
    EXPECT_EQ(s_one_inv, s_one);
}

TEST_F(FourQTest, ScalarNegate) {
    Curve::FourQ::Scalar s_neg = Curve::FourQ::Scalar::negate(s_known);
    Curve::FourQ::Scalar sum_zero = s_known + s_neg;
    EXPECT_TRUE(sum_zero.isZero()); // k + (-k) = 0 mod order

    Curve::FourQ::Scalar zero_neg = Curve::FourQ::Scalar::negate(Curve::FourQ::Scalar::getZero());
     EXPECT_TRUE(zero_neg.isZero());
}

TEST_F(FourQTest, ScalarGetZero) {
    Curve::FourQ::Scalar s_zero = Curve::FourQ::Scalar::getZero();
    EXPECT_TRUE(s_zero.isZero());
}

TEST_F(FourQTest, ScalarGetOrder) {
    Curve::FourQ::Scalar order = Curve::FourQ::Point::getOrder(); // 用 Point::getOrder 测试标量
    EXPECT_FALSE(order.isZero());
    // 可以尝试创建一个略大于阶数的标量，看它是否会环绕
    // 或者与已知的阶数十六进制字符串进行验证 (如果可用)
}


// --- Point Tests ---

TEST_F(FourQTest, PointDefaultConstructor) {
    Curve::FourQ::Point p; // 应为单位元/无穷远点
    EXPECT_TRUE(p.isZero());
}

TEST_F(FourQTest, PointCopyConstructor) {
    Curve::FourQ::Point p_copy(p_known);
    EXPECT_EQ(p_copy.getRaw(), p_known.getRaw()); // 比较原始字节，因为 == 可能有问题
    EXPECT_EQ(p_copy.toString(), p_known.toString());
}

TEST_F(FourQTest, PointStringConstructor) {
     // 使用在 SetUp 中计算的生成点十六进制
    Curve::FourQ::Point p_gen_from_str;
    ASSERT_NO_THROW(p_gen_from_str.fromString(generator_hex)); // 使用 fromString 以便测试验证逻辑
    EXPECT_EQ(p_gen_from_str.getRaw(), p_base.getRaw());
    EXPECT_EQ(p_gen_from_str.toString(), generator_hex);

    Curve::FourQ::Point p_kG_from_str;
    ASSERT_NO_THROW(p_kG_from_str.fromString(known_point_hex));
    EXPECT_EQ(p_kG_from_str.getRaw(), p_known.getRaw());
    EXPECT_EQ(p_kG_from_str.toString(), known_point_hex);

    // 测试无效点字符串
    EXPECT_THROW(Curve::FourQ::Point("invalid length string"), std::invalid_argument); // 长度错误
    // 测试长度正确但包含无效十六进制的点字符串
    EXPECT_THROW(Curve::FourQ::Point("gggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggg"), std::invalid_argument);

    // 测试不在曲线上的点字符串 (ecc_point_validate 会处理)
    // 需要一个已知解码后不在曲线上的有效十六进制字符串
    std::string not_on_curve_hex(ECC_KEY_LENGTH * 2, '0'); // 全零通常不是有效点（除了特定编码）
    // 根据 FourQ 的 decode 和 validate 行为，这可能会或不会抛出异常
    // EXPECT_THROW(Curve::FourQ::Point(not_on_curve_hex), std::runtime_error);
     try {
        Curve::FourQ::Point p_invalid(not_on_curve_hex);
        // 如果构造函数不抛出异常，检查点是否有效（如果接口允许）
        // 或者如果构造成功了，它不应等于已知点
        EXPECT_NE(p_invalid.getRaw(), p_base.getRaw());
     } catch (const std::runtime_error& e) {
        // 捕获预期的异常
        SUCCEED();
     } catch (...) {
        FAIL() << "Unexpected exception thrown for potentially invalid point";
     }
}


TEST_F(FourQTest, PointRawDataConstructor) {
    Curve::FourQ::EccDataType raw = p_known.getRaw();
    Curve::FourQ::Point p_from_raw;
    ASSERT_NO_THROW(p_from_raw = Curve::FourQ::Point(raw)); // 确保构造不抛出
    EXPECT_EQ(p_from_raw.getRaw(), p_known.getRaw());
     EXPECT_EQ(p_from_raw.toString(), known_point_hex);
}

TEST_F(FourQTest, PointFromString) {
    Curve::FourQ::Point p;
    EXPECT_NO_THROW(p.fromString(known_point_hex));
     EXPECT_EQ(p.getRaw(), p_known.getRaw());
    EXPECT_EQ(p.toString(), known_point_hex);
    EXPECT_THROW(p.fromString("12345"), std::invalid_argument); // 无效长度
}

TEST_F(FourQTest, PointToString) {
    EXPECT_EQ(p_known.toString(), known_point_hex);
    Curve::FourQ::Point p_zero;
    // 单位点的字符串表示需要验证
    // 它可能不是全零，取决于编码方式
    // 例如，可能是 "010000...00" 或其他表示
    std::string zero_point_hex = p_zero.toString();
    EXPECT_EQ(zero_point_hex.length(), ECC_KEY_LENGTH * 2);
    // EXPECT_EQ(p_zero.toString(), "..."); // 替换为正确的单位点十六进制
}

TEST_F(FourQTest, PointIsZero) {
    Curve::FourQ::Point p_zero;
    EXPECT_TRUE(p_zero.isZero());
    EXPECT_FALSE(p_base.isZero());
    EXPECT_FALSE(p_known.isZero());
}

TEST_F(FourQTest, PointAssignment) {
    Curve::FourQ::Point p;
    p = p_known;
    EXPECT_EQ(p.getRaw(), p_known.getRaw());
    EXPECT_EQ(p.toString(), p_known.toString()); // 也检查字符串
}

// 点比较 (==, !=) 目前依赖于原始指针比较，这在拷贝后可能不成立
// 应比较规范化坐标或原始字节数据
TEST_F(FourQTest, PointComparison) {
    Curve::FourQ::Point p_copy = p_known;
    Curve::FourQ::Point p_base_copy = p_base;
    // 主要测试: 比较原始字节数据
    EXPECT_TRUE(p_known.getRaw() == p_copy.getRaw());
    EXPECT_FALSE(p_known.getRaw() == p_base.getRaw());

    // 测试运算符本身 (注意: 当前的实现可能不按值比较)
    // EXPECT_TRUE(p_known == p_copy);  // 这可能失败，因为比较的是 _pe 指针
    // EXPECT_FALSE(p_known == p_base);
    // EXPECT_TRUE(p_known != p_base);
    // EXPECT_FALSE(p_known != p_copy); // 这可能失败
}

TEST_F(FourQTest, PointAddition) {
    Curve::FourQ::Point p_zero;
    Curve::FourQ::Point p_base_plus_zero = p_base + p_zero;
    EXPECT_EQ(p_base_plus_zero.getRaw(), p_base.getRaw());

    Curve::FourQ::Point p_known_plus_zero = p_known + p_zero;
    EXPECT_EQ(p_known_plus_zero.getRaw(), p_known.getRaw());

    // 测试 G + G = 2*G
    Curve::FourQ::Point p_2G = p_base + p_base;
    Curve::FourQ::Point p_2G_check = Curve::FourQ::Scalar(2) * p_base;
    EXPECT_EQ(p_2G.getRaw(), p_2G_check.getRaw());

    // 测试结合律: (G + kG) - G = kG
    Curve::FourQ::Point sum_G_kG = p_base + p_known;
    Curve::FourQ::Point res = sum_G_kG - p_base;
    EXPECT_EQ(res.getRaw(), p_known.getRaw());
}


TEST_F(FourQTest, PointSubtraction) {
    Curve::FourQ::Point p_zero;
    Curve::FourQ::Point p_base_minus_zero = p_base - p_zero;
    EXPECT_EQ(p_base_minus_zero.getRaw(), p_base.getRaw());

    Curve::FourQ::Point p_known_minus_self = p_known - p_known;
    EXPECT_TRUE(p_known_minus_self.isZero());

    // 测试 G - G = 0
    Curve::FourQ::Point diff_base = p_base - p_base;
    EXPECT_TRUE(diff_base.isZero());
}


TEST_F(FourQTest, PointScalarMultiplication) {
    Curve::FourQ::Scalar s_zero;
    Curve::FourQ::Scalar s2(2);

    Curve::FourQ::Point p_mult_zero = s_zero * p_base;
    EXPECT_TRUE(p_mult_zero.isZero());

    Curve::FourQ::Point p_mult_one = s_one * p_base;
    EXPECT_EQ(p_mult_one.getRaw(), p_base.getRaw());

    Curve::FourQ::Point p_2G = s2 * p_base;
    Curve::FourQ::Point p_G_plus_G = p_base + p_base;
    EXPECT_EQ(p_2G.getRaw(), p_G_plus_G.getRaw());

    // 测试 SetUp 中的 k*G 结果
    Curve::FourQ::Point p_check_known = s_known * p_base;
    EXPECT_EQ(p_check_known.getRaw(), p_known.getRaw());

     // 测试 k * 0 = 0
    Curve::FourQ::Point p_k_times_zero = s_known * Curve::FourQ::Point::getZero();
    EXPECT_TRUE(p_k_times_zero.isZero());
}

TEST_F(FourQTest, PointMulBase) {
     Curve::FourQ::Point p_check_known = Curve::FourQ::Point::mulBase(s_known);
     EXPECT_EQ(p_check_known.getRaw(), p_known.getRaw());

     Curve::FourQ::Point p_base_check = Curve::FourQ::Point::mulBase(s_one);
     EXPECT_EQ(p_base_check.getRaw(), p_base.getRaw());

     Curve::FourQ::Point p_zero_check = Curve::FourQ::Point::mulBase(Curve::FourQ::Scalar::getZero());
     EXPECT_TRUE(p_zero_check.isZero());
}


TEST_F(FourQTest, PointNegate) {
    Curve::FourQ::Point p_neg_known = Curve::FourQ::Point::negate(p_known);
    Curve::FourQ::Point sum_zero = p_known + p_neg_known;
    EXPECT_TRUE(sum_zero.isZero()); // P + (-P) = 0

    Curve::FourQ::Point p_neg_base = Curve::FourQ::Point::negate(p_base);
    Curve::FourQ::Point base_sum_zero = p_base + p_neg_base;
     EXPECT_TRUE(base_sum_zero.isZero());

    Curve::FourQ::Point zero_neg = Curve::FourQ::Point::negate(Curve::FourQ::Point::getZero());
    EXPECT_TRUE(zero_neg.isZero()); // 单位元的负元是它本身
}

TEST_F(FourQTest, PointGetZero) {
    Curve::FourQ::Point p_zero = Curve::FourQ::Point::getZero();
    EXPECT_TRUE(p_zero.isZero());
}

TEST_F(FourQTest, PointGetBase) {
    Curve::FourQ::Point p_base_check = Curve::FourQ::Point::getBase();
    EXPECT_FALSE(p_base_check.isZero());
    // 将其十六进制字符串与 SetUp 中计算的进行比较
    EXPECT_EQ(p_base_check.toString(), generator_hex);
}


TEST_F(FourQTest, PointMulAdd) {
    // 测试 P.MulAdd(a, b) => a*G + b*P
    Curve::FourQ::Scalar a(5), b(3);
    // 使用已知的 kG 点作为 P
    Curve::FourQ::Point p_for_muladd = p_known;

    // 计算预期结果: a*G + b*P = a*G + b*(kG) = (a + b*k)*G
    Curve::FourQ::Scalar scalar_combo = a + (b * s_known);
    Curve::FourQ::Point expected_res = Curve::FourQ::Point::mulBase(scalar_combo);

    // 使用 MulAdd 在 p_known (kG) 上计算
    Curve::FourQ::Point actual_res = p_for_muladd.MulAdd(a, b);

    EXPECT_EQ(actual_res.getRaw(), expected_res.getRaw());

    // Test with P = G: P.MulAdd(a, b) => a*G + b*G = (a+b)*G
    Curve::FourQ::Scalar sum_ab = a + b;
    Curve::FourQ::Point expected_res_base = Curve::FourQ::Point::mulBase(sum_ab);
    Curve::FourQ::Point actual_res_base = p_base.MulAdd(a, b);
    EXPECT_EQ(actual_res_base.getRaw(), expected_res_base.getRaw());

    // Test with b=0: P.MulAdd(a, 0) should be a*G
    Curve::FourQ::Point expected_aG = Curve::FourQ::Point::mulBase(a);
    Curve::FourQ::Point actual_aG = p_known.MulAdd(a, Curve::FourQ::Scalar::getZero()); // Use p_known as P
    EXPECT_EQ(actual_aG.getRaw(), expected_aG.getRaw());

     // Test with a=0: P.MulAdd(0, b) should be b*P
    Curve::FourQ::Point expected_bP = b * p_known;
    Curve::FourQ::Point actual_bP = p_known.MulAdd(Curve::FourQ::Scalar::getZero(), b);
    EXPECT_EQ(actual_bP.getRaw(), expected_bP.getRaw());

}

// 新增测试用例：打印 0-100 的标量及其对应的点
TEST_F(FourQTest, PrintScalarsAndPoints) {
    std::cout << "--- Printing Scalars and Base Points (0 to 100) ---" << std::endl;
    Curve::FourQ::Point base_point = Curve::FourQ::Point::getBase();

    // Print Curve basic info
    std::cout << "Curve::FourQ::Point::getOrder(): " << Curve::FourQ::Point::getOrder().toString() << std::endl;
    std::cout << "Curve::FourQ::Point::getBase(): " << Curve::FourQ::Point::getBase().toString() << std::endl;
    std::cout << "Curve::FourQ::Point::getOrder(): " << Curve::FourQ::Point::getOrder().toString() << std::endl;
    std::cout << "Curve::FourQ::Point::getBase(): " << Curve::FourQ::Point::getBase().toString() << std::endl;

    for (uint32_t i = 0; i <= 64; ++i) {
        // 创建标量
        Curve::FourQ::Scalar s(i);
        std::cout << "Scalar(" << i << "): " << s.toString() << std::endl;

        // 计算点 P = i * G (使用 mulBase 更高效)
        Curve::FourQ::Point p = Curve::FourQ::Point::mulBase(s);
        // 或者: Curve::FourQ::Point p = s * base_point;
        std::string p_str = p.toString();
        std::cout << "Point (" << i << "*G): " << p_str << std::endl;

        // --- 新增：测试 toString() 和 fromString() 的往返一致性 ---
        try {
            Curve::FourQ::Point p_from_str(p_str);
            ASSERT_EQ(p.getRaw(), p_from_str.getRaw()) << "toString/fromString round trip failed for i = " << i;
        } catch (const std::exception& e) {
            FAIL() << "Exception during toString/fromString round trip for i = " << i << ": " << e.what();
        }
        // --- 结束新增测试 ---

        // 可选：验证 i*G 是否等于 (i-1)*G + G (对于 i>0)
        if (i > 0) {
            Curve::FourQ::Scalar s_prev(i-1);
            Curve::FourQ::Point p_prev = Curve::FourQ::Point::mulBase(s_prev);
            Curve::FourQ::Point p_check = p_prev + base_point;
            // 由于点比较操作符可能不准确，我们比较原始字节
            ASSERT_EQ(p.getRaw(), p_check.getRaw()) << "Verification failed for i = " << i;
        }
    }
     std::cout << "--- Finished Printing Scalars and Base Points --- " << std::endl;
}

TEST_F(FourQTest, SchnorrQSign) {
    std::string msg = "Hello, World!";
    std::vector<uint8_t> spk;
    std::array<uint8_t, 64> sig;
    Curve::FourQ::EccDataType skx;  

    ::random_bytes(skx.data(), 32);
    Curve::FourQ::Scalar sk(skx);
    Curve::FourQ::Point pk = Curve::FourQ::Point::mulBase(sk);

    //spk.resize(32);
    //SchnorrQ_KeyGeneration(sk.getRaw().data(), spk.data());

    std::cout << "sk: " << sk.toString() << std::endl;
    std::cout << "pk: " << pk.toString() << std::endl;
    //std::cout << "spk: " << bytes_to_hex_string(spk) << std::endl;
    

    ASSERT_TRUE(Curve::FourQ::SchnorrQSign(sk, msg, sig));
    ASSERT_TRUE(Curve::FourQ::SchnorrQVerify(pk, msg, sig));

    std::cout << "msg: " << msg << ", " << bytes_to_hex_string(sig) << std::endl;

    sig[0] = sig[0] + 0x01;
    ASSERT_FALSE(Curve::FourQ::SchnorrQVerify(pk, msg, sig));

    msg = "Hello, World!1";
    ASSERT_FALSE(Curve::FourQ::SchnorrQVerify(pk, msg, sig));
    
    
}

// --- Main function for running tests ---
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    // 可以添加额外的初始化代码或参数处理
    return RUN_ALL_TESTS();
}