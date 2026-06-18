#include <bits/stdc++.h>


extern "C" {
    #include "../include/softfloat.h"
}

using namespace std;


void bf16_test()
{
    cout << __func__ << endl;
    float_status status;
    bfloat16 a = 0x3f99;    // 1.2
    bfloat16 b = 0x4013;    // 2.3

    // add
    bfloat16 c = 0x4060;    // 3.5

    // sub
    bfloat16 d = 0xbf8c;    // -1.1

    // mul
    bfloat16 e = 0x4030;    // 2.76

    // div
    bfloat16 f = 0x3f0f;    // 0.56

    // sqrt a
    bfloat16 g = 0x3f8c;    // 1.095

    // muladd a * b + c
    bfloat16 h = 0x40c8;    // 2.76 + 3.5 = 6.26

    // mulsub a * b - c
    bfloat16 i = 0xbf3d;    // 2.76 - 3.5 = -0.74

    cout << "a: " << hex << a << "  | ";
    cout << "b: " << hex << b << endl;
    
    cout << "chatgpt(a + b): " << hex << c << "  | ";
    cout << "fadd(a, b): " << hex << bfloat16_add(a, b, &status) << endl;

    cout << "chatgpt(a - b): " << hex << d << "  | ";
    cout << "fsub(a, b): " << hex << bfloat16_sub(a, b, &status) << endl;

    cout << "chatgpt(a * b): " << hex << e << "  | ";
    cout << "fmul(a, b): " << hex << bfloat16_mul(a, b, &status) << endl;

    cout << "chatgpt(a / b): " << hex << f << "  | ";
    cout << "fdiv(a, b): " << hex << bfloat16_div(a, b, &status) << endl;

    cout << "chatgpt(sqrt(a)): " << hex << g << "  | ";
    cout << "fsqrt(a): " << hex << bfloat16_sqrt(a, &status) << endl;

    cout << "chatgpt(max(a, b)): " << hex << b << "  | ";
    cout << "fmax(a, b): " << hex << bfloat16_max(a, b, &status) << endl;

    cout << "chatgpt(min(a, b)): " << hex << a << "  | ";
    cout << "fmin(a, b): " << hex << bfloat16_min(a, b, &status) << endl;

    cout << "chatgpt(muladd(a, b, c)): " << hex << h << "  | ";
    cout << "fmuladd(a, b, c): " << hex << bfloat16_muladd(a, b, c, 0, &status) << endl;

    cout << "chatgpt(mulsub(a, b, c)): " << hex << i << "  | ";
    cout << "fmulsub(a, b, c): " << hex << bfloat16_muladd(a, b, 0xc060, 0, &status) << endl;
}


void f16_test()
{
    cout << __func__ << endl;
    float_status status;
    float16 a = 0x3ccd;    // 1.2
    float16 b = 0x409a;    // 2.3

    // add
    float16 c = 0x4300;    // 3.5

    // sub
    float16 d = 0xbc66;    // -1.1

    // mul
    float16 e = 0x4185;    // 2.76

    // div
    float16 f = 0x387b;    // 0.56

    // sqrt a
    float16 g = 0x3c61;    // 1.095

    // muladd a * b + c
    float16 h = 0x4643;    // 2.76 + 3.5 = 6.26

    // mulsub a * b - c
    float16 i = 0xb9ec;    // 2.76 - 3.5 = -0.74

    cout << "a: " << hex << a << "  | ";
    cout << "b: " << hex << b << endl;
    
    cout << "chatgpt(a + b): " << hex << c << "  | ";
    cout << "fadd(a, b): " << hex << float16_add(a, b, &status) << endl;

    cout << "chatgpt(a - b): " << hex << d << "  | ";
    cout << "fsub(a, b): " << hex << float16_sub(a, b, &status) << endl;

    cout << "chatgpt(a * b): " << hex << e << "  | ";
    cout << "fmul(a, b): " << hex << float16_mul(a, b, &status) << endl;

    cout << "chatgpt(a / b): " << hex << f << "  | ";
    cout << "fdiv(a, b): " << hex << float16_div(a, b, &status) << endl;

    cout << "chatgpt(sqrt(a)): " << hex << g << "  | ";
    cout << "fsqrt(a): " << hex << float16_sqrt(a, &status) << endl;

    cout << "chatgpt(max(a, b)): " << hex << b << "  | ";
    cout << "fmax(a, b): " << hex << float16_max(a, b, &status) << endl;

    cout << "chatgpt(min(a, b)): " << hex << a << "  | ";
    cout << "fmin(a, b): " << hex << float16_min(a, b, &status) << endl;

    cout << "chatgpt(muladd(a, b, c)): " << hex << h << "  | ";
    cout << "fmuladd(a, b, c): " << hex << float16_muladd(a, b, c, 0, &status) << endl;

    cout << "chatgpt(mulsub(a, b, c)): " << hex << i << "  | ";
    cout << "fmulsub(a, b, c): " << hex << float16_muladd(a, b, 0xc300, 0, &status) << endl;
}


//e4m3
void f8_test()
{
    cout << __func__ << endl;
    float_status status;
    float8 a = 0x39;    // 1.2
    float8 b = 0x41;    // 2.3

    // add
    float8 c = 0x46;    // 3.5

    // sub
    float8 d = 0xb8;    // -1.1

    // mul
    float8 e = 0x43;    // 2.76

    // div
    float8 f = 0x31;    // 0.56

    // sqrt a
    float8 g = 0x38;    // 1.095

    // muladd a * b + c
    float8 h = 0x4c;    // 2.76 + 3.5 = 6.26

    // mulsub a * b - c
    float8 i = 0xb3;    // 2.76 - 3.5 = -0.74

    cout << "a: " << hex << a << "  | ";
    cout << "b: " << hex << b << endl;
    
    cout << "chatgpt(a + b): " << hex << c << "  | ";
    cout << "fadd(a, b): " << hex << float8_add(a, b, &status) << endl;

    cout << "chatgpt(a - b): " << hex << d << "  | ";
    cout << "fsub(a, b): " << hex << float8_sub(a, b, &status) << endl;

    cout << "chatgpt(a * b): " << hex << e << "  | ";
    cout << "fmul(a, b): " << hex << float8_mul(a, b, &status) << endl;

    cout << "chatgpt(a / b): " << hex << f << "  | ";
    cout << "fdiv(a, b): " << hex << float8_div(a, b, &status) << endl;

    cout << "chatgpt(sqrt(a)): " << hex << g << "  | ";
    cout << "fsqrt(a): " << hex << float8_sqrt(a, &status) << endl;

    cout << "chatgpt(max(a, b)): " << hex << b << "  | ";
    cout << "fmax(a, b): " << hex << float8_max(a, b, &status) << endl;

    cout << "chatgpt(min(a, b)): " << hex << a << "  | ";
    cout << "fmin(a, b): " << hex << float8_min(a, b, &status) << endl;

    cout << "chatgpt(muladd(a, b, c)): " << hex << h << "  | ";
    cout << "fmuladd(a, b, c): " << hex << float8_muladd(a, b, c, 0, &status) << endl;

    cout << "chatgpt(mulsub(a, b, c)): " << hex << i << "  | ";
    cout << "fmulsub(a, b, c): " << hex << float8_muladd(a, b, 0xc6, 0, &status) << endl;

}



//e5m2
void f8_1_test()
{
    cout << __func__ << endl;
    float_status status;
    float8 a = 0x3c;    // 1.2
    float8 b = 0x40;    // 2.3

    // add
    float8 c = 0x43;    // 3.5

    // sub
    float8 d = 0xbc;    // -1.1

    // mul
    float8 e = 0x41;    // 2.76

    // div
    float8 f = 0x38;    // 0.56

    // sqrt a
    float8 g = 0x3c;    // 1.095

    // muladd a * b + c
    float8 h = 0x46;    // 2.76 + 3.5 = 6.26

    // mulsub a * b - c
    float8 i = 0xb9;    // 2.76 - 3.5 = -0.74

    cout << "a: " << hex << a << "  | ";
    cout << "b: " << hex << b << endl;
    
    cout << "chatgpt(a + b): " << hex << c << "  | ";
    cout << "fadd(a, b): " << hex << float8_1_add(a, b, &status) << endl;

    cout << "chatgpt(a - b): " << hex << d << "  | ";
    cout << "fsub(a, b): " << hex << float8_1_sub(a, b, &status) << endl;

    cout << "chatgpt(a * b): " << hex << e << "  | ";
    cout << "fmul(a, b): " << hex << float8_1_mul(a, b, &status) << endl;

    cout << "chatgpt(a / b): " << hex << f << "  | ";
    cout << "fdiv(a, b): " << hex << float8_1_div(a, b, &status) << endl;

    cout << "chatgpt(sqrt(a)): " << hex << g << "  | ";
    cout << "fsqrt(a): " << hex << float8_1_sqrt(a, &status) << endl;

    cout << "chatgpt(max(a, b)): " << hex << b << "  | ";
    cout << "fmax(a, b): " << hex << float8_1_max(a, b, &status) << endl;

    cout << "chatgpt(min(a, b)): " << hex << a << "  | ";
    cout << "fmin(a, b): " << hex << float8_1_min(a, b, &status) << endl;

    cout << "chatgpt(muladd(a, b, c)): " << hex << h << "  | ";
    cout << "fmuladd(a, b, c): " << hex << float8_1_muladd(a, b, c, 0, &status) << endl;

    cout << "chatgpt(mulsub(a, b, c)): " << hex << i << "  | ";
    cout << "fmulsub(a, b, c): " << hex << float8_1_muladd(a, b, 0xc3, 0, &status) << endl;

}




int main()
{

    // x86 不支持 低精度浮点数, 这里使用chatgpt提供的低精度二进制编码来简单测试函数的功能

    f8_test();
    f8_1_test();

    f16_test();
    bf16_test();
    return 0;
}