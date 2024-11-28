WeChat: cstutorcs
QQ: 749389476
Email: tutorcs@163.com
#pragma once

#include "../riscv.h"
#include "VSRTL/core/vsrtl_component.h"

namespace vsrtl {
namespace core {
using namespace Ripes;

template <unsigned XLEN>
class BranchDst : public Component {
public:
  BranchDst(const std::string &name, SimComponent *parent)
      : Component(name, parent) {
    // clang-format off
    res << [=] {
      switch(opcode.uValue()){
        case RVInstr::BEQ: case RVInstr::BNE: case RVInstr::BLT:
        case RVInstr::BGE: case RVInstr::BLTU: case RVInstr::BGEU:
        case RVInstr::JAL:
          return pc_reg.sValue() + imm.sValue();
        default:
          return r1_reg.sValue() + imm.sValue();
      }
    };
    // clang-format on
  }

  INPUTPORT_ENUM(opcode, RVInstr);
  INPUTPORT(pc_reg, XLEN);
  INPUTPORT(r1_reg, XLEN);
  INPUTPORT(imm, XLEN);
  OUTPUTPORT(res, XLEN);
};

} // namespace core
} // namespace vsrtl
