WeChat: cstutorcs
QQ: 749389476
Email: tutorcs@163.com
#pragma once

#include "../riscv.h"

#include "VSRTL/core/vsrtl_component.h"

namespace vsrtl {
namespace core {
using namespace Ripes;

//--------------- TODO: implement earlier branch resolving ----------
// Hint: the existing ports of hzunit now is not enough for detecting hazard. So
// you need to find out what else you need and add input ports in hzunit.
// you are free to modify the lambda expr here

class HazardEBUnit : public Component {
public:
  HazardEBUnit(const std::string &name, SimComponent *parent)
      : Component(name, parent) {
    hazardFEEnable << [=] { return !hasHazard() && !hasMemWait() && !hasALUWait(); };
    hazardIDEXEnable << [=] { return !hasEcallHazard() && !hasMemWait() && !hasALUWait(); };
    hazardEXMEMEnable << [=] { return !hasMemWait() && !hasALUWait(); };
    hazardMEMWBEnable << [=] { return !hasMemWait() && !hasALUWait(); };
    hazardEXMEMClear << [=] { return hasEcallHazard(); };
    hazardIDEXClear << [=] { return hasLoadUseHazard(); };
    stallEcallHandling << [=] { return hasEcallHazard(); };
  }

  INPUTPORT(id_reg1_idx, c_RVRegsBits);
  INPUTPORT(id_reg2_idx, c_RVRegsBits);

  // 分支指令使用的寄存器
  INPUTPORT(id_br_reg1_idx, c_RVRegsBits);
  INPUTPORT(id_br_reg2_idx, c_RVRegsBits);

  INPUTPORT(ex_reg_wr_idx, c_RVRegsBits);
  INPUTPORT(ex_do_mem_read_en, 1);

  INPUTPORT(mem_do_reg_write, 1);

  INPUTPORT(wb_do_reg_write, 1);

  INPUTPORT_ENUM(opcode, RVInstr);

  INPUTPORT(mem_wait, 1);
  INPUTPORT(alu_wait, 1);

  // Hazard Front End enable: Low when stalling the front end (shall be
  // connected to a register 'enable' input port). The
  OUTPUTPORT(hazardFEEnable, 1);

  // Hazard IDEX enable: Low when stalling due to an ECALL hazard
  OUTPUTPORT(hazardIDEXEnable, 1);

  OUTPUTPORT(hazardEXMEMEnable, 1);

  OUTPUTPORT(hazardMEMWBEnable, 1);

  // EXMEM clear: High when an ECALL hazard is detected
  OUTPUTPORT(hazardEXMEMClear, 1);
  // IDEX clear: High when a load-use hazard is detected
  OUTPUTPORT(hazardIDEXClear, 1);

  // Stall Ecall Handling: High whenever we are about to handle an ecall, but
  // have outstanding writes in the pipeline which must be comitted to the
  // register file before handling the ecall.
  OUTPUTPORT(stallEcallHandling, 1);

private:
  bool hasHazard() { return hasLoadUseHazard() || hasEcallHazard() || hasBranchHazard(); }

  bool hasLoadUseHazard() const {
    const unsigned exidx = ex_reg_wr_idx.uValue();
    const unsigned idx1 = id_reg1_idx.uValue();
    const unsigned idx2 = id_reg2_idx.uValue();
    const bool mrd = ex_do_mem_read_en.uValue();

    return (exidx == idx1 || exidx == idx2) && mrd;
  }

  bool hasMemWait() const {
    return (mem_wait.uValue() == 1);
  }

  bool hasALUWait() const {
    return (alu_wait.uValue() == 1);
  }

  bool hasEcallHazard() const {
    // Check for ECALL hazard. We are implictly dependent on all registers when
    // performing an ECALL operation. As such, all outstanding writes to the
    // register file must be performed before handling the ecall. Hence, the
    // front-end of the pipeline shall be stalled until the remainder of the
    // pipeline has been cleared and there are no more outstanding writes.
    const bool isEcall = opcode.uValue() == RVInstr::ECALL;
    return isEcall && (mem_do_reg_write.uValue() || wb_do_reg_write.uValue());
  }

  bool hasBranchHazard() const {
    const unsigned bridx1 = id_br_reg1_idx.uValue();
    const unsigned bridx2 = id_br_reg2_idx.uValue();
    const unsigned exidx = ex_reg_wr_idx.uValue();
    const bool memread = ex_do_mem_read_en.uValue();
    return (bridx1 == exidx || bridx2 == exidx) && memread;
  }

};
} // namespace core
} // namespace vsrtl
