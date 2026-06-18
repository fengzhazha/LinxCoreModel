#include <gtest/gtest.h>
#include <iostream>

#include "ISA.h"
#include "calculate/MInstCalculator.h"

using namespace JCore;
class MinstTest : public testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};
TEST_F(MinstTest, DecodeBasic)
{
    std::cout << "decode basic" << std::endl;
    MInst inst;
    inst.DecodeBin(0x5056, 2); // 11216: 5056         	c.setret	0x11218, ->ra <_end>
    std::cout << inst.Dump() << std::endl;
}