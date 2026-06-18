                DecodeBlock16_extract_C_SETRET(inst, bin);
                inst->opcode = OP_C_SETRET;
                break;
            }
            break;
        case 0x0018:
            inst->dst0 = 31;
            DecodeBlock16_extract_C_ARITH(inst, bin);
            inst->opcode = OP_C_SUB;
            break;
        case 0x001a:
            inst->src2Type = 1;
            inst->src2 = MInst::Sextract32(bin, 11, 5);
            inst->dst0 = 31;
            inst->src0 = MInst::Extract32(bin, 6, 5);
            inst->src2Shamt = 3;
            DecodeBlock16_extract_C_LD_SD(inst, bin);
            inst->opcode = OP_C_LDI;
            break;
        case 0x001c:
            DecodeBlock16_extract_C_EXT(inst, bin);
            switch ((bin >> 11) & 0x1f) {
                case 0x0:
                    inst->opcode = OP_C_SETC_TGT;
                    break;
                case 0x8:
                    inst->dst0 = 31;
                    inst->opcode = OP_C_SEXT_B;
                    break;
                case 0x9:
                    inst->dst0 = 31;
                    inst->opcode = OP_C_SEXT_H;
                    break;
                case 0xa:
                    inst->dst0 = 31;
                    inst->opcode = OP_C_SEXT_W;
                    break;
                case 0xb:
                    inst->dst0 = 31;
                    inst->opcode = OP_C_ZEXT_B;
                    break;
                case 0xc:
                    inst->dst0 = 31;
                    inst->opcode = OP_C_ZEXT_H;
                    break;
                case 0xd:
                    inst->dst0 = 31;
                    inst->opcode = OP_C_ZEXT_W;
                    break;
            }
            break;
        case 0x0026:
            DecodeBlock16_extract_C_ARITH(inst, bin);
            inst->opcode = OP_C_SETC_EQ;
            break;
        case 0x0028:
            inst->dst0 = 31;
            DecodeBlock16_extract_C_ARITH(inst, bin);
            inst->opcode = OP_C_AND;
            break;
        case 0x002a:
            inst->src2Type = 1;
            inst->src2 = MInst::Sextract32(bin, 11, 5);
            inst->src0 = 24;
            inst->src1 = MInst::Extract32(bin, 6, 5);
            inst->src1Type = 0;
            inst->src2Shamt = 2;
            DecodeBlock16_extract_C_LD_SD(inst, bin);
            inst->opcode = OP_C_SWI;
            break;
        case 0x002c:
            switch ((bin >> 11) & 0x1f) {
                case 0x0:
                    inst->dst0 = 31;
                    inst->src0Type = 0;
                    inst->src0 = 24;
                    DecodeBlock16_extract_C_CMP_S(inst, bin);
                    inst->opcode = OP_C_CMP_EQ_I;
                    break;
                case 0x1:
                    inst->dst0 = 31;
                    inst->src0Type = 0;
                    inst->src0 = 24;
                    DecodeBlock16_extract_C_CMP_S(inst, bin);
                    inst->opcode = OP_C_CMP_NE_I;
                    break;
                case 0x2:
                    inst->dst0 = 31;
                    inst->src0Type = 0;
                    inst->src0 = 24;
                    DecodeBlock16_extract_C_CMP_U(inst, bin);
                    inst->opcode = OP_C_SLLI;
                    break;
                case 0x3:
                    inst->dst0 = 31;
                    inst->src0Type = 0;
                    inst->src0 = 24;
                    DecodeBlock16_extract_C_CMP_U(inst, bin);
                    inst->opcode = OP_C_SRLI;
                    break;
                case 0x10:
                    inst->dst0 = 31;
                    DecodeBlock16_extract_C_CMP_U(inst, bin);
                    inst->opcode = OP_C_SSRGET;
                    break;
            }
            break;
        case 0x0036:
            DecodeBlock16_extract_C_ARITH(inst, bin);
            inst->opcode = OP_C_SETC_NE;
            break;
        case 0x0038:
            inst->dst0 = 31;
            DecodeBlock16_extract_C_ARITH(inst, bin);
            inst->opcode = OP_C_OR;
            break;
        case 0x003a:
            inst->src2Type = 1;
            inst->src2 = MInst::Sextract32(bin, 11, 5);
            inst->src0 = 24;
            inst->src1 = MInst::Extract32(bin, 6, 5);
            inst->src1Type = 0;
            inst->src2Shamt = 3;
            DecodeBlock16_extract_C_LD_SD(inst, bin);
            inst->opcode = OP_C_SDI;
            break;
    }
    return inst;
}
} // namespace JCore
