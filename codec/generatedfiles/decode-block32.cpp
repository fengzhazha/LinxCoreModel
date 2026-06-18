                        DecodeBlock32_extract_SETRET(inst, bin);
                        inst->opcode = OP_SETRET;
                        break;
                    }
                    DecodeBlock32_extract_constant(inst, bin);
                    inst->opcode = OP_ADDTPC;
                    break;
                case 0x1:
                    DecodeBlock32_extract_constant(inst, bin);
                    inst->opcode = OP_LUI;
                    break;
                case 0x2:
                    switch ((bin >> 12) & 0x7) {
                        case 0x0:
                            DecodeBlock32_extract_branch(inst, bin);
                            inst->opcode = OP_B_EQ;
                            break;
                        case 0x1:
                            DecodeBlock32_extract_branch(inst, bin);
                            inst->opcode = OP_B_NE;
                            break;
                        case 0x2:
                            DecodeBlock32_extract_branch(inst, bin);
                            inst->opcode = OP_B_LT;
                            break;
                        case 0x3:
                            DecodeBlock32_extract_branch(inst, bin);
                            inst->opcode = OP_B_GE;
                            break;
                        case 0x4:
                            DecodeBlock32_extract_branch(inst, bin);
                            inst->opcode = OP_B_LTU;
                            break;
                        case 0x5:
                            DecodeBlock32_extract_branch(inst, bin);
                            inst->opcode = OP_B_GEU;
                            break;
                        case 0x6:
                            DecodeBlock32_extract_JR(inst, bin);
                            switch ((bin >> 20) & 0x1f) {
                                case 0x0:
                                    inst->opcode = OP_JR;
                                    break;
                            }
                            break;
                    }
                    break;
                case 0x3:
                    DecodeBlock32_extract_J(inst, bin);
                    switch ((bin >> 12) & 0x7) {
                        case 0x0:
                            inst->opcode = OP_J;
                            break;
                        case 0x1:
                            inst->opcode = OP_B_Z;
                            break;
                        case 0x2:
                            inst->opcode = OP_B_NZ;
                            break;
                    }
                    break;
                case 0x4:
                    switch (bin & 0x06007000) {
                        case 0x00000000:
                            DecodeBlock32_extract_shift(inst, bin);
                            switch ((bin >> 27) & 0x1f) {
                                case 0x0:
                                    inst->opcode = OP_MUL;
                                    break;
                            }
                            break;
                        case 0x00001000:
                            DecodeBlock32_extract_shift(inst, bin);
                            switch ((bin >> 27) & 0x1f) {
                                case 0x0:
                                    inst->opcode = OP_MULU;
                                    break;
                            }
                            break;
                        case 0x00002000:
                            DecodeBlock32_extract_shift(inst, bin);
                            switch ((bin >> 27) & 0x1f) {
                                case 0x0:
                                    inst->opcode = OP_MULW;
                                    break;
                            }
                            break;
                        case 0x00003000:
                            DecodeBlock32_extract_shift(inst, bin);
                            switch ((bin >> 27) & 0x1f) {
                                case 0x0:
                                    inst->opcode = OP_MULUW;
                                    break;
                            }
                            break;
                        case 0x00006000:
                            DecodeBlock32_extract_MADD(inst, bin);
                            inst->opcode = OP_MADD;
                            break;
                        case 0x00007000:
                            DecodeBlock32_extract_MADD(inst, bin);
                            inst->opcode = OP_MADDW;
                            break;
                    }
                    break;
                case 0x5:
                    DecodeBlock32_extract_multi(inst, bin);
                    switch (bin & 0xfe007000u) {
                        case 0x00000000:
                            inst->opcode = OP_DIV;
                            break;
                        case 0x00001000:
                            inst->opcode = OP_DIVU;
                            break;
                        case 0x00002000:
                            inst->opcode = OP_DIVW;
                            break;
                        case 0x00003000:
                            inst->opcode = OP_DIVUW;
                            break;
                        case 0x00004000:
                            inst->opcode = OP_REM;
                            break;
                        case 0x00005000:
                            inst->opcode = OP_REMU;
                            break;
                        case 0x00006000:
                            inst->opcode = OP_REMW;
                            break;
                        case 0x00007000:
                            inst->opcode = OP_REMUW;
                            break;
                    }
                    break;
                case 0x6:
                    switch ((bin >> 12) & 0x7) {
                        case 0x0:
                            DecodeBlock32_extract_bo(inst, bin);
                            inst->opcode = OP_BXS;
                            break;
                        case 0x1:
                            DecodeBlock32_extract_bo(inst, bin);
                            inst->opcode = OP_BXU;
                            break;
                        case 0x2:
                            DecodeBlock32_extract_bo(inst, bin);
                            inst->opcode = OP_BIC;
                            break;
                        case 0x3:
                            DecodeBlock32_extract_bo(inst, bin);
                            inst->opcode = OP_BIS;
                            break;
                        case 0x4:
                            DecodeBlock32_extract_one_src(inst, bin);
                            inst->opcode = OP_CTZ;
                            break;
                        case 0x5:
                            DecodeBlock32_extract_one_src(inst, bin);
                            inst->opcode = OP_CLZ;
                            break;
                        case 0x6:
                            DecodeBlock32_extract_one_src(inst, bin);
                            inst->opcode = OP_BCNT;
                            break;
                        case 0x7:
                            DecodeBlock32_extract_one_src(inst, bin);
                            inst->opcode = OP_REV;
                            break;
                    }
                    break;
                case 0x7:
                    DecodeBlock32_extract_CSEL(inst, bin);
                    switch ((bin >> 12) & 0x7) {
                        case 0x0:
                            inst->opcode = OP_CSEL;
                            break;
                    }
                    break;
            }
            break;
        case 0x00000009:
            switch (bin & 0x00007070) {
                case 0x00000000:
                    DecodeBlock32_extract_arith(inst, bin);
                    inst->opcode = OP_LB;
                    break;
                case 0x00000010:
                    inst->src2 = MInst::Sextract32(bin, 20, 12);
                    DecodeBlock32_extract_arith_si(inst, bin);
                    inst->opcode = OP_LBI;
                    break;
                case 0x00000030:
                    DecodeBlock32_extract_LoadSymbol(inst, bin);
                    inst->opcode = OP_LB_PCR;
                    break;
                case 0x00000040:
                    DecodeBlock32_extract_store(inst, bin);
                    switch ((bin >> 7) & 0x1f) {
                        case 0x0:
                            inst->opcode = OP_SB;
                            break;
                    }
                    break;
                case 0x00000050:
                    inst->src2 = MInst::Deposit(MInst::Extract32(bin, 25, 7), 7, MInst::Sextract32(bin, 7, 5));
                    DecodeBlock32_extract_store_imm(inst, bin);
                    inst->opcode = OP_SBI;
                    break;
                case 0x00000060:
                    DecodeBlock32_extract_StoreSymbol(inst, bin);
                    inst->opcode = OP_SB_PCR;
                    break;
                case 0x00001000:
                    DecodeBlock32_extract_arith(inst, bin);
                    inst->opcode = OP_LH;
                    break;
                case 0x00001010:
                    inst->src2 = MInst::ExShift1(MInst::Sextract32(bin, 20, 12));
                    DecodeBlock32_extract_arith_si(inst, bin);
                    inst->opcode = OP_LHI;
                    break;
                case 0x00001020:
                    inst->src2 = MInst::Sextract32(bin, 20, 12);
                    DecodeBlock32_extract_arith_si(inst, bin);
                    inst->opcode = OP_LHI_U;
                    break;
                case 0x00001030:
                    DecodeBlock32_extract_LoadSymbol(inst, bin);
                    inst->opcode = OP_LH_PCR;
                    break;
                case 0x00001040:
                    DecodeBlock32_extract_store(inst, bin);
                    switch ((bin >> 7) & 0x1f) {
                        case 0x0:
                            inst->opcode = OP_SH;
                            break;
                    }
                    break;
                case 0x00001050:
                    inst->src2 = MInst::ExShift1(MInst::Deposit(MInst::Extract32(bin, 25, 7), 7, MInst::Sextract32(bin, 7, 5)));
                    DecodeBlock32_extract_store_imm(inst, bin);
                    inst->opcode = OP_SHI;
                    break;
                case 0x00001060:
                    DecodeBlock32_extract_StoreSymbol(inst, bin);
                    inst->opcode = OP_SH_PCR;
                    break;
                case 0x00002000:
                    DecodeBlock32_extract_arith(inst, bin);
                    inst->opcode = OP_LW;
                    break;
                case 0x00002010:
                    inst->src2 = MInst::ExShift2(MInst::Sextract32(bin, 20, 12));
                    DecodeBlock32_extract_arith_si(inst, bin);
                    inst->opcode = OP_LWI;
                    break;
                case 0x00002020:
                    inst->src2 = MInst::Sextract32(bin, 20, 12);
                    DecodeBlock32_extract_arith_si(inst, bin);
                    inst->opcode = OP_LWI_U;
                    break;
                case 0x00002030:
                    DecodeBlock32_extract_LoadSymbol(inst, bin);
                    inst->opcode = OP_LW_PCR;
                    break;
                case 0x00002040:
                    DecodeBlock32_extract_store(inst, bin);
                    switch ((bin >> 7) & 0x1f) {
                        case 0x0:
                            inst->opcode = OP_SW;
                            break;
                    }
                    break;
                case 0x00002050:
                    inst->src2 = MInst::ExShift2(MInst::Deposit(MInst::Extract32(bin, 25, 7), 7, MInst::Sextract32(bin, 7, 5)));
                    DecodeBlock32_extract_store_imm(inst, bin);
                    inst->opcode = OP_SWI;
                    break;
                case 0x00002060:
                    DecodeBlock32_extract_StoreSymbol(inst, bin);
                    inst->opcode = OP_SW_PCR;
                    break;
                case 0x00003000:
                    DecodeBlock32_extract_arith(inst, bin);
                    inst->opcode = OP_LD;
                    break;
                case 0x00003010:
                    inst->src2 = MInst::ExShift3(MInst::Sextract32(bin, 20, 12));
                    DecodeBlock32_extract_arith_si(inst, bin);
                    inst->opcode = OP_LDI;
                    break;
                case 0x00003020:
                    inst->src2 = MInst::Sextract32(bin, 20, 12);
                    DecodeBlock32_extract_arith_si(inst, bin);
                    inst->opcode = OP_LDI_U;
                    break;
                case 0x00003030:
                    DecodeBlock32_extract_LoadSymbol(inst, bin);
                    inst->opcode = OP_LD_PCR;
                    break;
                case 0x00003040:
                    DecodeBlock32_extract_store(inst, bin);
                    switch ((bin >> 7) & 0x1f) {
                        case 0x0:
                            inst->opcode = OP_SD;
                            break;
                    }
                    break;
                case 0x00003050:
                    inst->src2 = MInst::ExShift3(MInst::Deposit(MInst::Extract32(bin, 25, 7), 7, MInst::Sextract32(bin, 7, 5)));
                    DecodeBlock32_extract_store_imm(inst, bin);
                    inst->opcode = OP_SDI;
                    break;
                case 0x00003060:
                    DecodeBlock32_extract_StoreSymbol(inst, bin);
                    inst->opcode = OP_SD_PCR;
                    break;
                case 0x00004000:
                    DecodeBlock32_extract_arith(inst, bin);
                    inst->opcode = OP_LBU;
                    break;
                case 0x00004010:
                    inst->src2 = MInst::Sextract32(bin, 20, 12);
                    DecodeBlock32_extract_arith_si(inst, bin);
                    inst->opcode = OP_LBUI;
                    break;
                case 0x00004030:
                    DecodeBlock32_extract_LoadSymbol(inst, bin);
                    inst->opcode = OP_LBU_PCR;
                    break;
                case 0x00005000:
                    DecodeBlock32_extract_arith(inst, bin);
                    inst->opcode = OP_LHU;
                    break;
                case 0x00005010:
                    inst->src2 = MInst::ExShift1(MInst::Sextract32(bin, 20, 12));
                    DecodeBlock32_extract_arith_si(inst, bin);
                    inst->opcode = OP_LHUI;
                    break;
                case 0x00005020:
                    inst->src2 = MInst::Sextract32(bin, 20, 12);
                    DecodeBlock32_extract_arith_si(inst, bin);
                    inst->opcode = OP_LHUI_U;
                    break;
                case 0x00005030:
                    DecodeBlock32_extract_LoadSymbol(inst, bin);
                    inst->opcode = OP_LHU_PCR;
                    break;
                case 0x00005040:
                    DecodeBlock32_extract_store(inst, bin);
                    switch ((bin >> 7) & 0x1f) {
                        case 0x0:
                            inst->opcode = OP_SH_U;
                            break;
                    }
                    break;
                case 0x00005050:
                    inst->src2 = MInst::Deposit(MInst::Extract32(bin, 25, 7), 7, MInst::Sextract32(bin, 7, 5));
                    DecodeBlock32_extract_store_imm(inst, bin);
                    inst->opcode = OP_SHI_U;
                    break;
                case 0x00006000:
                    DecodeBlock32_extract_arith(inst, bin);
                    inst->opcode = OP_LWU;
                    break;
                case 0x00006010:
                    inst->src2 = MInst::ExShift2(MInst::Sextract32(bin, 20, 12));
                    DecodeBlock32_extract_arith_si(inst, bin);
                    inst->opcode = OP_LWUI;
                    break;
                case 0x00006020:
                    inst->src2 = MInst::Sextract32(bin, 20, 12);
                    DecodeBlock32_extract_arith_si(inst, bin);
                    inst->opcode = OP_LWUI_U;
                    break;
                case 0x00006030:
                    DecodeBlock32_extract_LoadSymbol(inst, bin);
                    inst->opcode = OP_LWU_PCR;
                    break;
                case 0x00006040:
                    DecodeBlock32_extract_store(inst, bin);
                    switch ((bin >> 7) & 0x1f) {
                        case 0x0:
                            inst->opcode = OP_SW_U;
                            break;
                    }
                    break;
                case 0x00006050:
                    inst->src2 = MInst::Deposit(MInst::Extract32(bin, 25, 7), 7, MInst::Sextract32(bin, 7, 5));
                    DecodeBlock32_extract_store_imm(inst, bin);
                    inst->opcode = OP_SWI_U;
                    break;
                case 0x00007040:
                    DecodeBlock32_extract_store(inst, bin);
                    switch ((bin >> 7) & 0x1f) {
                        case 0x0:
                            inst->opcode = OP_SD_U;
                            break;
                    }
                    break;
                case 0x00007050:
                    inst->src2 = MInst::Deposit(MInst::Extract32(bin, 25, 7), 7, MInst::Sextract32(bin, 7, 5));
                    DecodeBlock32_extract_store_imm(inst, bin);
                    inst->opcode = OP_SDI_U;
                    break;
            }
            break;
        case 0x0000000b:
            if ((bin & 0xfffffff0u) == 0x10002020) {
                /* 00010000 00000000 00100000 00101011 */
                DecodeBlock32_extract_FENCE_I(inst, bin);
                inst->opcode = OP_FENCE_I;
                break;
            }
            if ((bin & 0xfff07ff0u) == 0x00000020) {
                /* 00000000 0000.... .0000000 00101011 */
                DecodeBlock32_extract_exec_ctrl(inst, bin);
                inst->opcode = OP_BSE;
                break;
            }
            if ((bin & 0xfff07ff0u) == 0x00100020) {
                /* 00000000 0001.... .0000000 00101011 */
                DecodeBlock32_extract_exec_ctrl(inst, bin);
                inst->opcode = OP_BWE;
                break;
            }
            if ((bin & 0xfff07ff0u) == 0x00200020) {
                /* 00000000 0010.... .0000000 00101011 */
                DecodeBlock32_extract_exec_ctrl(inst, bin);
                inst->opcode = OP_BWI;
                break;
            }
            if ((bin & 0xfff07ff0u) == 0x00001020) {
                /* 00000000 0000.... .0010000 00101011 */
                DecodeBlock32_extract_exec_ctrl(inst, bin);
                inst->opcode = OP_ASSERT;
                break;
            }
            if ((bin & 0xfff07ff0u) == 0x00004030) {
                /* 00000000 0000.... .1000000 00111011 */
                DecodeBlock32_extract_SETC_TGT(inst, bin);
                inst->opcode = OP_SETC_TGT;
                break;
            }
            if ((bin & 0xff0ffff0u) == 0x00003020) {
                /* 00000000 ....0000 00110000 00101011 */
                DecodeBlock32_extract_acrc(inst, bin);
                inst->opcode = OP_ACRC;
                break;
            }
            if ((bin & 0xff0ffff0u) == 0x01003020) {
                /* 00000001 ....0000 00110000 00101011 */
                DecodeBlock32_extract_acre(inst, bin);
                inst->opcode = OP_ACRE;
                break;
            }
            if ((bin & 0xfe007070u) == 0x00004050) {
                /* 0000000. ........ .100.... .1011011 */
                DecodeBlock32_extract_max_min(inst, bin);
                inst->opcode = OP_MAX;
                break;
            }
            if ((bin & 0xfe007070u) == 0x08004050) {
                /* 0000100. ........ .100.... .1011011 */
                DecodeBlock32_extract_max_min(inst, bin);
                inst->opcode = OP_MAXU;
                break;
            }
            if ((bin & 0xfe007070u) == 0x00005050) {
                /* 0000000. ........ .101.... .1011011 */
                DecodeBlock32_extract_max_min(inst, bin);
                inst->opcode = OP_MIN;
                break;
            }
            if ((bin & 0xfe007070u) == 0x08005050) {
                /* 0000100. ........ .101.... .1011011 */
                DecodeBlock32_extract_max_min(inst, bin);
                inst->opcode = OP_MINU;
                break;
            }
            if ((bin & 0xf9f07070u) == 0x00000070) {
                /* 00000..0 0000.... .000.... .1111011 */
                DecodeBlock32_extract_fpa3(inst, bin);
                inst->opcode = OP_FABS;
                break;
            }
            if ((bin & 0xf9f07070u) == 0x00001070) {
                /* 00000..0 0000.... .001.... .1111011 */
                DecodeBlock32_extract_fpa3(inst, bin);
                inst->opcode = OP_FSQRT;
                break;
            }
            if ((bin & 0xf9f07070u) == 0x00002070) {
                /* 00000..0 0000.... .010.... .1111011 */
                DecodeBlock32_extract_fpa3(inst, bin);
                inst->opcode = OP_FEXP;
                break;
            }
            if ((bin & 0xf9f07070u) == 0x00006070) {
                /* 00000..0 0000.... .110.... .1111011 */
                DecodeBlock32_extract_fpa3(inst, bin);
                inst->opcode = OP_FRECIP;
                break;
            }
            if ((bin & 0xf8007070u) == 0x00006050) {
                /* 00000... ........ .110.... .1011011 */
                DecodeBlock32_extract_fmax_fmin(inst, bin);
                inst->opcode = OP_FMAX;
                break;
            }
            if ((bin & 0xf8007070u) == 0x00007050) {
                /* 00000... ........ .111.... .1011011 */
                DecodeBlock32_extract_fmax_fmin(inst, bin);
                inst->opcode = OP_FMIN;
                break;
            }
            if ((bin & 0xf8007070u) == 0x00000040) {
                /* 00000... ........ .000.... .1001011 */
                DecodeBlock32_extract_fpa(inst, bin);
                inst->opcode = OP_FADD;
                break;
            }
            if ((bin & 0xf8007070u) == 0x00001040) {
                /* 00000... ........ .001.... .1001011 */
                DecodeBlock32_extract_fpa(inst, bin);
                inst->opcode = OP_FSUB;
                break;
            }
            if ((bin & 0xf8007070u) == 0x00002040) {
                /* 00000... ........ .010.... .1001011 */
                DecodeBlock32_extract_fpa(inst, bin);
                inst->opcode = OP_FMUL;
                break;
            }
            if ((bin & 0xf8007070u) == 0x00003040) {
                /* 00000... ........ .011.... .1001011 */
                DecodeBlock32_extract_fpa(inst, bin);
                inst->opcode = OP_FDIV;
                break;
            }
            if ((bin & 0xf8007070u) == 0x00000050) {
                /* 00000... ........ .000.... .1011011 */
                DecodeBlock32_extract_fpc(inst, bin);
                inst->opcode = OP_FEQ;
                break;
            }
            if ((bin & 0xf8007070u) == 0x00001050) {
                /* 00000... ........ .001.... .1011011 */
                DecodeBlock32_extract_fpc(inst, bin);
                inst->opcode = OP_FNE;
                break;
            }
            if ((bin & 0xf8007070u) == 0x00002050) {
                /* 00000... ........ .010.... .1011011 */
                DecodeBlock32_extract_fpc(inst, bin);
                inst->opcode = OP_FLT;
                break;
            }
            if ((bin & 0xf8007070u) == 0x00003050) {
                /* 00000... ........ .011.... .1011011 */
                DecodeBlock32_extract_fpc(inst, bin);
                inst->opcode = OP_FGE;
                break;
            }
            if ((bin & 0xf8007070u) == 0x08000050) {
                /* 00001... ........ .000.... .1011011 */
                DecodeBlock32_extract_fpc(inst, bin);
                inst->opcode = OP_FEQS;
                break;
            }
            if ((bin & 0xf8007070u) == 0x08001050) {
                /* 00001... ........ .001.... .1011011 */
                DecodeBlock32_extract_fpc(inst, bin);
                inst->opcode = OP_FNES;
                break;
            }
            if ((bin & 0xf8007070u) == 0x08002050) {
                /* 00001... ........ .010.... .1011011 */
                DecodeBlock32_extract_fpc(inst, bin);
                inst->opcode = OP_FLTS;
                break;
            }
            if ((bin & 0xf8007070u) == 0x08003050) {
                /* 00001... ........ .011.... .1011011 */
                DecodeBlock32_extract_fpc(inst, bin);
                inst->opcode = OP_FGES;
                break;
            }
            if ((bin & 0xf4007ff0u) == 0x00003000) {
                /* 0000.0.. ........ .0110000 00001011 */
                DecodeBlock32_extract_atomic_st(inst, bin);
                inst->opcode = OP_SW_ADD;
                break;
            }
            if ((bin & 0xf4007ff0u) == 0x10003000) {
                /* 0001.0.. ........ .0110000 00001011 */
                DecodeBlock32_extract_atomic_st(inst, bin);
                inst->opcode = OP_SW_AND;
                break;
            }
            if ((bin & 0xf4007ff0u) == 0x20003000) {
                /* 0010.0.. ........ .0110000 00001011 */
                DecodeBlock32_extract_atomic_st(inst, bin);
                inst->opcode = OP_SW_OR;
                break;
            }
            if ((bin & 0xf4007ff0u) == 0x30003000) {
                /* 0011.0.. ........ .0110000 00001011 */
                DecodeBlock32_extract_atomic_st(inst, bin);
                inst->opcode = OP_SW_XOR;
                break;
            }
            if ((bin & 0xf4007ff0u) == 0x40003000) {
                /* 0100.0.. ........ .0110000 00001011 */
                DecodeBlock32_extract_atomic_st(inst, bin);
                inst->opcode = OP_SW_SMAX;
                break;
            }
            if ((bin & 0xf4007ff0u) == 0x50003000) {
                /* 0101.0.. ........ .0110000 00001011 */
                DecodeBlock32_extract_atomic_st(inst, bin);
                inst->opcode = OP_SW_SMIN;
                break;
            }
            if ((bin & 0xf4007ff0u) == 0x60003000) {
                /* 0110.0.. ........ .0110000 00001011 */
                DecodeBlock32_extract_atomic_st(inst, bin);
                inst->opcode = OP_SW_UMAX;
                break;
            }
            if ((bin & 0xf4007ff0u) == 0x70003000) {
                /* 0111.0.. ........ .0110000 00001011 */
                DecodeBlock32_extract_atomic_st(inst, bin);
                inst->opcode = OP_SW_UMIN;
                break;
            }
            if ((bin & 0xf4007ff0u) == 0x00005000) {
                /* 0000.0.. ........ .1010000 00001011 */
                DecodeBlock32_extract_atomic_st(inst, bin);
                inst->opcode = OP_SD_ADD;
                break;
            }
            if ((bin & 0xf4007ff0u) == 0x10005000) {
                /* 0001.0.. ........ .1010000 00001011 */
                DecodeBlock32_extract_atomic_st(inst, bin);
                inst->opcode = OP_SD_AND;
                break;
            }
            if ((bin & 0xf4007ff0u) == 0x20005000) {
                /* 0010.0.. ........ .1010000 00001011 */
                DecodeBlock32_extract_atomic_st(inst, bin);
                inst->opcode = OP_SD_OR;
                break;
            }
            if ((bin & 0xf4007ff0u) == 0x30005000) {
                /* 0011.0.. ........ .1010000 00001011 */
                DecodeBlock32_extract_atomic_st(inst, bin);
                inst->opcode = OP_SD_XOR;
                break;
            }
            if ((bin & 0xf4007ff0u) == 0x40005000) {
                /* 0100.0.. ........ .1010000 00001011 */
                DecodeBlock32_extract_atomic_st(inst, bin);
                inst->opcode = OP_SD_SMAX;
                break;
            }
            if ((bin & 0xf4007ff0u) == 0x50005000) {
                /* 0101.0.. ........ .1010000 00001011 */
                DecodeBlock32_extract_atomic_st(inst, bin);
                inst->opcode = OP_SD_SMIN;
                break;
            }
            if ((bin & 0xf4007ff0u) == 0x60005000) {
                /* 0110.0.. ........ .1010000 00001011 */
                DecodeBlock32_extract_atomic_st(inst, bin);
                inst->opcode = OP_SD_UMAX;
                break;
            }
            if ((bin & 0xf4007ff0u) == 0x70005000) {
                /* 0111.0.. ........ .1010000 00001011 */
                DecodeBlock32_extract_atomic_st(inst, bin);
                inst->opcode = OP_SD_UMIN;
                break;
            }
            if ((bin & 0xf1f07070u) == 0x00000000) {
                /* 0000...0 0000.... .000.... .0001011 */
                DecodeBlock32_extract_lr(inst, bin);
                inst->opcode = OP_LR_B;
                break;
            }
            if ((bin & 0xf1f07070u) == 0x10000000) {
                /* 0001...0 0000.... .000.... .0001011 */
                DecodeBlock32_extract_lr(inst, bin);
                inst->opcode = OP_LR_H;
                break;
            }
            if ((bin & 0xf1f07070u) == 0x20000000) {
                /* 0010...0 0000.... .000.... .0001011 */
                DecodeBlock32_extract_lr(inst, bin);
                inst->opcode = OP_LR_W;
                break;
            }
            if ((bin & 0xf1f07070u) == 0x30000000) {
                /* 0011...0 0000.... .000.... .0001011 */
                DecodeBlock32_extract_lr(inst, bin);
                inst->opcode = OP_LR_D;
                break;
            }
            if ((bin & 0xf00ffff0u) == 0x00002020) {
                /* 0000.... ....0000 00100000 00101011 */
                DecodeBlock32_extract_FENCE_D(inst, bin);
                inst->opcode = OP_FENCE_D;
                break;
            }
            if ((bin & 0xf0007070u) == 0x00001000) {
                /* 0000.... ........ .001.... .0001011 */
                DecodeBlock32_extract_sc(inst, bin);
                inst->opcode = OP_SC_B;
                break;
            }
            if ((bin & 0xf0007070u) == 0x10001000) {
                /* 0001.... ........ .001.... .0001011 */
                DecodeBlock32_extract_sc(inst, bin);
                inst->opcode = OP_SC_H;
                break;
            }
            if ((bin & 0xf0007070u) == 0x20001000) {
                /* 0010.... ........ .001.... .0001011 */
                DecodeBlock32_extract_sc(inst, bin);
                inst->opcode = OP_SC_W;
                break;
            }
            if ((bin & 0xf0007070u) == 0x30001000) {
                /* 0011.... ........ .001.... .0001011 */
                DecodeBlock32_extract_sc(inst, bin);
                inst->opcode = OP_SC_D;
                break;
            }
            if ((bin & 0xf0007070u) == 0x00002000) {
                /* 0000.... ........ .010.... .0001011 */
                DecodeBlock32_extract_atomic_ld(inst, bin);
                inst->opcode = OP_LW_ADD;
                break;
            }
            if ((bin & 0xf0007070u) == 0x10002000) {
                /* 0001.... ........ .010.... .0001011 */
                DecodeBlock32_extract_atomic_ld(inst, bin);
                inst->opcode = OP_LW_AND;
                break;
            }
            if ((bin & 0xf0007070u) == 0x20002000) {
                /* 0010.... ........ .010.... .0001011 */
                DecodeBlock32_extract_atomic_ld(inst, bin);
                inst->opcode = OP_LW_OR;
                break;
            }
            if ((bin & 0xf0007070u) == 0x30002000) {
                /* 0011.... ........ .010.... .0001011 */
                DecodeBlock32_extract_atomic_ld(inst, bin);
                inst->opcode = OP_LW_XOR;
                break;
            }
            if ((bin & 0xf0007070u) == 0x40002000) {
                /* 0100.... ........ .010.... .0001011 */
                DecodeBlock32_extract_atomic_ld(inst, bin);
                inst->opcode = OP_LW_SMAX;
                break;
            }
            if ((bin & 0xf0007070u) == 0x50002000) {
                /* 0101.... ........ .010.... .0001011 */
                DecodeBlock32_extract_atomic_ld(inst, bin);
                inst->opcode = OP_LW_SMIN;
                break;
            }
            if ((bin & 0xf0007070u) == 0x60002000) {
                /* 0110.... ........ .010.... .0001011 */
                DecodeBlock32_extract_atomic_ld(inst, bin);
                inst->opcode = OP_LW_UMAX;
                break;
            }
            if ((bin & 0xf0007070u) == 0x70002000) {
                /* 0111.... ........ .010.... .0001011 */
                DecodeBlock32_extract_atomic_ld(inst, bin);
                inst->opcode = OP_LW_UMIN;
                break;
            }
            if ((bin & 0xf0007070u) == 0x00004000) {
                /* 0000.... ........ .100.... .0001011 */
                DecodeBlock32_extract_atomic_ld(inst, bin);
                inst->opcode = OP_LD_ADD;
                break;
            }
            if ((bin & 0xf0007070u) == 0x10004000) {
                /* 0001.... ........ .100.... .0001011 */
                DecodeBlock32_extract_atomic_ld(inst, bin);
                inst->opcode = OP_LD_AND;
                break;
            }
            if ((bin & 0xf0007070u) == 0x20004000) {
                /* 0010.... ........ .100.... .0001011 */
                DecodeBlock32_extract_atomic_ld(inst, bin);
                inst->opcode = OP_LD_OR;
                break;
            }
            if ((bin & 0xf0007070u) == 0x30004000) {
                /* 0011.... ........ .100.... .0001011 */
                DecodeBlock32_extract_atomic_ld(inst, bin);
                inst->opcode = OP_LD_XOR;
                break;
            }
            if ((bin & 0xf0007070u) == 0x40004000) {
                /* 0100.... ........ .100.... .0001011 */
                DecodeBlock32_extract_atomic_ld(inst, bin);
                inst->opcode = OP_LD_SMAX;
                break;
            }
            if ((bin & 0xf0007070u) == 0x50004000) {
                /* 0101.... ........ .100.... .0001011 */
                DecodeBlock32_extract_atomic_ld(inst, bin);
                inst->opcode = OP_LD_SMIN;
                break;
            }
            if ((bin & 0xf0007070u) == 0x60004000) {
                /* 0110.... ........ .100.... .0001011 */
                DecodeBlock32_extract_atomic_ld(inst, bin);
                inst->opcode = OP_LD_UMAX;
                break;
            }
            if ((bin & 0xf0007070u) == 0x70004000) {
                /* 0111.... ........ .100.... .0001011 */
                DecodeBlock32_extract_atomic_ld(inst, bin);
                inst->opcode = OP_LD_UMIN;
                break;
            }
            if ((bin & 0xf0007070u) == 0x00006000) {
                /* 0000.... ........ .110.... .0001011 */
                DecodeBlock32_extract_sc(inst, bin);
                inst->opcode = OP_SWAPB;
                break;
            }
            if ((bin & 0xf0007070u) == 0x10006000) {
                /* 0001.... ........ .110.... .0001011 */
                DecodeBlock32_extract_sc(inst, bin);
                inst->opcode = OP_SWAPH;
                break;
            }
            if ((bin & 0xf0007070u) == 0x20006000) {
                /* 0010.... ........ .110.... .0001011 */
                DecodeBlock32_extract_sc(inst, bin);
                inst->opcode = OP_SWAPW;
                break;
            }
            if ((bin & 0xf0007070u) == 0x30006000) {
                /* 0011.... ........ .110.... .0001011 */
                DecodeBlock32_extract_sc(inst, bin);
                inst->opcode = OP_SWAPD;
                break;
            }
            if ((bin & 0x01f07070) == 0x00000060) {
                /* .......0 0000.... .000.... .1101011 */
                DecodeBlock32_extract_CVT(inst, bin);
                inst->opcode = OP_FCVT;
                break;
            }
            if ((bin & 0x01f07070) == 0x00001060) {
                /* .......0 0000.... .001.... .1101011 */
                DecodeBlock32_extract_CVT(inst, bin);
                inst->opcode = OP_FCVTA;
                break;
            }
            if ((bin & 0x01f07070) == 0x00002060) {
                /* .......0 0000.... .010.... .1101011 */
                DecodeBlock32_extract_CVT(inst, bin);
                inst->opcode = OP_FCVTM;
                break;
            }
            if ((bin & 0x01f07070) == 0x00003060) {
                /* .......0 0000.... .011.... .1101011 */
                DecodeBlock32_extract_CVT(inst, bin);
                inst->opcode = OP_FCVTN;
                break;
            }
            if ((bin & 0x01f07070) == 0x00004060) {
                /* .......0 0000.... .100.... .1101011 */
                DecodeBlock32_extract_CVT(inst, bin);
                inst->opcode = OP_FCVTP;
                break;
            }
            if ((bin & 0x01f07070) == 0x00005060) {
                /* .......0 0000.... .101.... .1101011 */
                DecodeBlock32_extract_CVT(inst, bin);
                inst->opcode = OP_FCVTZ;
                break;
            }
            if ((bin & 0x01f07070) == 0x00006060) {
                /* .......0 0000.... .110.... .1101011 */
                DecodeBlock32_extract_CVT(inst, bin);
                inst->opcode = OP_SCVTF;
                break;
            }
            if ((bin & 0x01f07070) == 0x00007060) {
                /* .......0 0000.... .111.... .1101011 */
                DecodeBlock32_extract_CVT(inst, bin);
                inst->opcode = OP_UCVTF;
                break;
            }
            if ((bin & 0x000ff070) == 0x00000030) {
                /* ........ ....0000 0000.... .0111011 */
                DecodeBlock32_extract_SSRGET(inst, bin);
                inst->opcode = OP_SSRGET;
                break;
            }
            if ((bin & 0x00007070) == 0x00004040) {
                /* ........ ........ .100.... .1001011 */
                DecodeBlock32_extract_fpa2(inst, bin);
                inst->opcode = OP_FMADD;
                break;
            }
            if ((bin & 0x00007070) == 0x00005040) {
                /* ........ ........ .101.... .1001011 */
                DecodeBlock32_extract_fpa2(inst, bin);
                inst->opcode = OP_FMSUB;
                break;
            }
            if ((bin & 0x00007070) == 0x00006040) {
                /* ........ ........ .110.... .1001011 */
                DecodeBlock32_extract_fpa2(inst, bin);
                inst->opcode = OP_FNMADD;
                break;
            }
            if ((bin & 0x00007070) == 0x00007040) {
                /* ........ ........ .111.... .1001011 */
                DecodeBlock32_extract_fpa2(inst, bin);
                inst->opcode = OP_FNMSUB;
                break;
            }
            if ((bin & 0x00007ff0) == 0x00001030) {
                /* ........ ........ .0010000 00111011 */
                DecodeBlock32_extract_SSRSET(inst, bin);
                inst->opcode = OP_SSRSET;
                break;
            }
            break;
    }
    return inst;
}
} // namespace JCore
