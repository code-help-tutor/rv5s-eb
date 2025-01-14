WeChat: cstutorcs
QQ: 749389476
Email: tutorcs@163.com
#pragma once

#include "VSRTL/core/vsrtl_adder.h"
#include "VSRTL/core/vsrtl_constant.h"
#include "VSRTL/core/vsrtl_design.h"
#include "VSRTL/core/vsrtl_logicgate.h"
#include "VSRTL/core/vsrtl_multiplexer.h"

#include "../../ripesvsrtlprocessor.h"

// Functional units
#include "../riscv.h"
#include "../rv_alu.h"
#include "../rv_branch.h"
#include "../rv_control.h"
#include "../rv_decode.h"
#include "../rv_ecallchecker.h"
#include "../rv_immediate.h"
#include "../rv_memory_stall.h"
#include "../rv_registerfile.h"
#include "../rv_uncompress.h"
#include "rv5s_eb_branchdst.h"

// Stage separating registers
#include "../rv5s/rv5s_exmem.h"
#include "../rv5s/rv5s_idex.h"
#include "../rv5s/rv5s_memwb.h"
#include "../rv5s/rv5s_forwardingunit.h"
#include "../rv5s_no_fw_hz/rv5s_no_fw_hz_ifid.h"

// Forwarding & Hazard detection unit
#include "rv5s_eb_forwardingunit.h"
#include "rv5s_eb_hazardunit.h"

namespace vsrtl {
namespace core {
using namespace Ripes;

template <typename XLEN_T>
class RV5S_EB : public RipesVSRTLProcessor {
  static_assert(std::is_same<uint32_t, XLEN_T>::value ||
                    std::is_same<uint64_t, XLEN_T>::value,
                "Only supports 32- and 64-bit variants");
  static constexpr unsigned XLEN = sizeof(XLEN_T) * CHAR_BIT;

public:
  enum Stage { IF = 0, ID = 1, EX = 2, MEM = 3, WB = 4, STAGECOUNT };
  RV5S_EB(const QStringList &extensions)
      : RipesVSRTLProcessor(
            "5-Stage RISC-V Processor with early branch resolving") {
    m_enabledISA = std::make_shared<ISAInfo<XLenToRVISA<XLEN>()>>(extensions);
    decode->setISA(m_enabledISA);
    uncompress->setISA(m_enabledISA);

    // -----------------------------------------------------------------------
    // Program counter
    pc_reg->out >> pc_4->op1;
    pc_inc->out >> pc_4->op2;
    pc_src->out >> pc_reg->in;
    0 >> pc_reg->clear;
    hzunit->hazardFEEnable >> pc_reg->enable;

    2 >> pc_inc->get(PcInc::INC2);
    4 >> pc_inc->get(PcInc::INC4);
    uncompress->Pc_Inc >> pc_inc->select;

    //--------------- TODO: implement earlier branch resolving ----------
    // Note: pc_src works uses the PcSrc enum, but is selected by the boolean
    // signal from the controlflow OR gate. PcSrc enum values must adhere to the
    // boolean 0/1 values.

    decode->branch_taken >> branch_predicted;    // 获取分支预测
    decode->branch_target >> branch_target;      // 获取分支目标地址

    // 控制流信号更新，选择跳转地址或PC+4
    pc_src->select = (branch_predicted) ? branch_target : pc_4;  // 如果预测为跳转，选择分支目标地址，否则选择PC+4


    controlflow_or->out >> pc_src->select;

    controlflow_or->out >> *efsc_or->in[0];
    ecallChecker->syscallExit >> *efsc_or->in[1];

    efsc_or->out >> *efschz_or->in[0];
    hzunit->hazardIDEXClear >> *efschz_or->in[1];

    // -----------------------------------------------------------------------
    // Instruction memory
    pc_reg->out >> instr_mem->addr;
    instr_mem->setMemory(m_memory);

    // -----------------------------------------------------------------------
    // Decode
    ifid_reg->instr_out >> decode->instr;

    // -----------------------------------------------------------------------
    // Control signals
    decode->opcode >> control->opcode;

    // -----------------------------------------------------------------------
    // Immediate
    decode->opcode >> immediate->opcode;
    ifid_reg->instr_out >> immediate->instr;

    // -----------------------------------------------------------------------
    // Registers
    decode->r1_reg_idx >> registerFile->r1_addr;
    decode->r2_reg_idx >> registerFile->r2_addr;
    reg_wr_src->out >> registerFile->data_in;

    memwb_reg->wr_reg_idx_out >> registerFile->wr_addr;
    memwb_reg->reg_do_write_out >> registerFile->wr_en;
    memwb_reg->mem_read_out >> reg_wr_src->get(RegWrSrc::MEMREAD);
    memwb_reg->alures_out >> reg_wr_src->get(RegWrSrc::ALURES);
    memwb_reg->pc4_out >> reg_wr_src->get(RegWrSrc::PC4);
    memwb_reg->reg_wr_src_ctrl_out >> reg_wr_src->select;

    registerFile->setMemory(m_regMem);

    //--------------- TODO: implement earlier branch resolving ----------
    // Branch
    // Hint: you should use reg1/reg2_br_fw_src
    control->comp_ctrl >> branch->comp_op;
    reg1_br_fw_src->out >> branch->op1;
    reg2_br_fw_src->out >> branch->op2;

    branch->res >> *br_and->in[0];
    idex_reg->do_br_out >> *br_and->in[1];
    br_and->out >> *controlflow_or->in[0];
    idex_reg->do_jmp_out >> *controlflow_or->in[1];
    // Hint: you should utilize the one output port of hazard unit

    // Hint: the pc update logic could be written here
    // Hint: you may use pc_br of type BranchDst
    pc_4->out >> pc_src->get(PcSrc::PC4);
    alu->res >> pc_src->get(PcSrc::ALU);

    // -----------------------------------------------------------------------
    // ALU

    // Forwarding multiplexers for destination EX
    idex_reg->r1_out >> reg1_fw_src->get(ForwardingSrc::IdStage);
    exmem_reg->alures_out >> reg1_fw_src->get(ForwardingSrc::MemStage);
    reg_wr_src->out >> reg1_fw_src->get(ForwardingSrc::WbStage);
    funit->alu_reg1_forwarding_ctrl >> reg1_fw_src->select;

    idex_reg->r2_out >> reg2_fw_src->get(ForwardingSrc::IdStage);
    exmem_reg->alures_out >> reg2_fw_src->get(ForwardingSrc::MemStage);
    reg_wr_src->out >> reg2_fw_src->get(ForwardingSrc::WbStage);
    funit->alu_reg2_forwarding_ctrl >> reg2_fw_src->select;

    //--------------- TODO: implement earlier branch resolving ----------
    // Forwarding multiplexers for destination ID
    // Hint: about reg1/reg2_br_fw_src and funit

    //--------------------------------------------------------------------------
    // ALU operand multiplexers
    reg1_fw_src->out >> alu_op1_src->get(AluSrc1::REG1);
    idex_reg->pc_out >> alu_op1_src->get(AluSrc1::PC);
    idex_reg->alu_op1_ctrl_out >> alu_op1_src->select;

    reg2_fw_src->out >> alu_op2_src->get(AluSrc2::REG2);
    idex_reg->imm_out >> alu_op2_src->get(AluSrc2::IMM);
    idex_reg->alu_op2_ctrl_out >> alu_op2_src->select;

    alu_op1_src->out >> alu->op1;
    alu_op2_src->out >> alu->op2;

    idex_reg->alu_ctrl_out >> alu->ctrl;

    // -----------------------------------------------------------------------
    // Data memory
    exmem_reg->alures_out >> data_mem->addr;
    exmem_reg->mem_do_write_out >> data_mem->wr_en;
    exmem_reg->r2_out >> data_mem->data_in;
    exmem_reg->mem_op_out >> data_mem->op;
    data_mem->mem->setMemory(m_memory);

    // -----------------------------------------------------------------------
    // Ecall checker

    idex_reg->opcode_out >> ecallChecker->opcode;
    ecallChecker->setSyscallCallback(&trapHandler);
    hzunit->stallEcallHandling >> ecallChecker->stallEcallHandling;

    // -----------------------------------------------------------------------
    // IF/ID
    pc_4->out >> ifid_reg->pc4_in;
    pc_reg->out >> ifid_reg->pc_in;
    uncompress->exp_instr >> ifid_reg->instr_in;
    hzunit->hazardFEEnable >> ifid_reg->enable;
    efsc_or->out >> ifid_reg->clear;
    1 >> ifid_reg->valid_in; // Always valid unless register is cleared

    // -----------------------------------------------------------------------
    // Increment
    instr_mem->data_out >> uncompress->instr;

    // -----------------------------------------------------------------------
    // ID/EX
    hzunit->hazardIDEXEnable >> idex_reg->enable;
    hzunit->hazardIDEXClear >> idex_reg->stalled_in;
    efschz_or->out >> idex_reg->clear;

    // Data
    ifid_reg->pc4_out >> idex_reg->pc4_in;
    ifid_reg->pc_out >> idex_reg->pc_in;
    registerFile->r1_out >> idex_reg->r1_in;
    registerFile->r2_out >> idex_reg->r2_in;
    immediate->imm >> idex_reg->imm_in;

    // Control
    decode->wr_reg_idx >> idex_reg->wr_reg_idx_in;
    control->reg_wr_src_ctrl >> idex_reg->reg_wr_src_ctrl_in;
    control->reg_do_write_ctrl >> idex_reg->reg_do_write_in;
    control->alu_op1_ctrl >> idex_reg->alu_op1_ctrl_in;
    control->alu_op2_ctrl >> idex_reg->alu_op2_ctrl_in;
    control->mem_do_write_ctrl >> idex_reg->mem_do_write_in;
    control->alu_ctrl >> idex_reg->alu_ctrl_in;
    control->mem_ctrl >> idex_reg->mem_op_in;
    control->comp_ctrl >> idex_reg->br_op_in;
    control->do_branch >> idex_reg->do_br_in;
    control->do_jump >> idex_reg->do_jmp_in;
    decode->r1_reg_idx >> idex_reg->rd_reg1_idx_in;
    decode->r2_reg_idx >> idex_reg->rd_reg2_idx_in;
    decode->opcode >> idex_reg->opcode_in;
    control->mem_do_read_ctrl >> idex_reg->mem_do_read_in;

    ifid_reg->valid_out >> idex_reg->valid_in;

    // -----------------------------------------------------------------------
    // EX/MEM
    hzunit->hazardEXMEMClear >> exmem_reg->clear;
    hzunit->hazardEXMEMClear >> *mem_stalled_or->in[0];
    hzunit->hazardEXMEMEnable >> exmem_reg->enable;
    idex_reg->stalled_out >> *mem_stalled_or->in[1];
    mem_stalled_or->out >> exmem_reg->stalled_in;

    // Data
    idex_reg->pc_out >> exmem_reg->pc_in;
    idex_reg->pc4_out >> exmem_reg->pc4_in;
    reg2_fw_src->out >> exmem_reg->r2_in;
    alu->res >> exmem_reg->alures_in;

    // Control
    idex_reg->reg_wr_src_ctrl_out >> exmem_reg->reg_wr_src_ctrl_in;
    idex_reg->wr_reg_idx_out >> exmem_reg->wr_reg_idx_in;
    idex_reg->reg_do_write_out >> exmem_reg->reg_do_write_in;
    idex_reg->mem_do_write_out >> exmem_reg->mem_do_write_in;
    idex_reg->mem_do_read_out >> exmem_reg->mem_do_read_in;
    idex_reg->mem_op_out >> exmem_reg->mem_op_in;

    idex_reg->valid_out >> exmem_reg->valid_in;

    // -----------------------------------------------------------------------
    // MEM/WB

    0 >> memwb_reg->clear; // a more complete solution is to control all clear/enable signals by hzunit
    data_mem->data_invalid >> *memwb_stalled_or->in[0];
    exmem_reg->stalled_out >> *memwb_stalled_or->in[1];
    memwb_stalled_or->out >> memwb_reg->stalled_in;
    hzunit->hazardMEMWBEnable >> memwb_reg->enable;

    // Data
    exmem_reg->pc_out >> memwb_reg->pc_in;
    exmem_reg->pc4_out >> memwb_reg->pc4_in;
    exmem_reg->alures_out >> memwb_reg->alures_in;
    data_mem->data_out >> memwb_reg->mem_read_in;

    // Control
    exmem_reg->reg_wr_src_ctrl_out >> memwb_reg->reg_wr_src_ctrl_in;
    exmem_reg->wr_reg_idx_out >> memwb_reg->wr_reg_idx_in;
    exmem_reg->reg_do_write_out >> memwb_reg->reg_do_write_in;

    exmem_reg->valid_out >> memwb_reg->valid_in;

    //--------------- TODO: implement earlier branch resolving ----------
    // Forwarding unit
    idex_reg->rd_reg1_idx_out >> funit->id_reg1_idx;
    idex_reg->rd_reg2_idx_out >> funit->id_reg2_idx;

    exmem_reg->wr_reg_idx_out >> funit->mem_reg_wr_idx;
    exmem_reg->reg_do_write_out >> funit->mem_reg_wr_en;

    memwb_reg->wr_reg_idx_out >> funit->wb_reg_wr_idx;
    memwb_reg->reg_do_write_out >> funit->wb_reg_wr_en;

    //--------------- TODO: implement earlier branch resolving ----------
    // Hazard detection unit
    decode->r1_reg_idx >> hzunit->id_reg1_idx;
    decode->r2_reg_idx >> hzunit->id_reg2_idx;

    idex_reg->mem_do_read_out >> hzunit->ex_do_mem_read_en;
    idex_reg->wr_reg_idx_out >> hzunit->ex_reg_wr_idx;

    exmem_reg->reg_do_write_out >> hzunit->mem_do_reg_write;

    memwb_reg->reg_do_write_out >> hzunit->wb_do_reg_write;

    idex_reg->opcode_out >> hzunit->opcode;
    data_mem->data_invalid >> hzunit->mem_wait;
    alu->data_invalid >> hzunit->alu_wait;
  }

  // Design subcomponents
  SUBCOMPONENT(registerFile, TYPE(RegisterFile<XLEN, true>));
  SUBCOMPONENT(alu, TYPE(ALU<XLEN>));
  SUBCOMPONENT(control, Control);
  SUBCOMPONENT(immediate, TYPE(Immediate<XLEN>));
  SUBCOMPONENT(decode, TYPE(Decode<XLEN>));
  SUBCOMPONENT(branch, TYPE(Branch<XLEN>));
  SUBCOMPONENT(pc_4, Adder<XLEN>);
  SUBCOMPONENT(pc_br, TYPE(BranchDst<XLEN>));
  SUBCOMPONENT(uncompress, TYPE(Uncompress<XLEN>));

  // Registers
  SUBCOMPONENT(pc_reg, RegisterClEn<XLEN>);

  // Stage seperating registers
  SUBCOMPONENT(ifid_reg, TYPE(IFID<XLEN>));
  SUBCOMPONENT(idex_reg, TYPE(RV5S_IDEX<XLEN>));
  SUBCOMPONENT(exmem_reg, TYPE(RV5S_EXMEM<XLEN>));
  SUBCOMPONENT(memwb_reg, TYPE(RV5S_MEMWB<XLEN>));

  // Multiplexers
  SUBCOMPONENT(reg_wr_src, TYPE(EnumMultiplexer<RegWrSrc, XLEN>));
  SUBCOMPONENT(pc_src, TYPE(EnumMultiplexer<PcSrc, XLEN>));
  SUBCOMPONENT(alu_op1_src, TYPE(EnumMultiplexer<AluSrc1, XLEN>));
  SUBCOMPONENT(alu_op2_src, TYPE(EnumMultiplexer<AluSrc2, XLEN>));
  SUBCOMPONENT(reg1_fw_src, TYPE(EnumMultiplexer<ForwardingSrc, XLEN>));
  SUBCOMPONENT(reg2_fw_src, TYPE(EnumMultiplexer<ForwardingSrc, XLEN>));
  SUBCOMPONENT(reg1_br_fw_src, TYPE(EnumMultiplexer<ForwardingSrc, XLEN>));
  SUBCOMPONENT(reg2_br_fw_src, TYPE(EnumMultiplexer<ForwardingSrc, XLEN>));
  SUBCOMPONENT(pc_inc, TYPE(EnumMultiplexer<PcInc, XLEN>));

  // Memories
  SUBCOMPONENT(instr_mem, TYPE(ROM<XLEN, c_RVInstrWidth>));
  SUBCOMPONENT(data_mem, TYPE(RVMemoryStall<XLEN, XLEN>));

  // Forwarding & hazard detection units
  SUBCOMPONENT(funit, ForwardingEBUnit);
  SUBCOMPONENT(hzunit, HazardEBUnit);

  // Gates
  // True if branch instruction and branch taken
  SUBCOMPONENT(br_and, TYPE(And<1, 2>));
  // True if branch taken or jump instruction
  SUBCOMPONENT(controlflow_or, TYPE(Or<1, 2>));
  // True if no branch hazard
  SUBCOMPONENT(br_hz_not, TYPE(Not<1, 1>));
  // True if above and data prepared
  SUBCOMPONENT(controlflow_and, TYPE(And<1, 2>));
  // True if controlflow action or performing syscall finishing
  SUBCOMPONENT(efsc_or, TYPE(Or<1, 2>));
  // True if performing syscall finishing or stalling due to load-use hazard
  SUBCOMPONENT(efschz_or, TYPE(Or<1, 2>));

  SUBCOMPONENT(mem_stalled_or, TYPE(Or<1, 2>));
  SUBCOMPONENT(memwb_stalled_or, TYPE(Or<1, 2>));

  // Address spaces
  ADDRESSSPACEMM(m_memory);
  ADDRESSSPACE(m_regMem);

  SUBCOMPONENT(ecallChecker, EcallChecker);

  // Ripes interface compliance
  const ProcessorStructure &structure() const override { return m_structure; }
  unsigned int getPcForStage(StageIndex idx) const override {
    // clang-format off
    switch (idx.index()) {
    case IF: return pc_reg->out.uValue();
    case ID: return ifid_reg->pc_out.uValue();
    case EX: return idex_reg->pc_out.uValue();
    case MEM: return exmem_reg->pc_out.uValue();
    case WB: return memwb_reg->pc_out.uValue();
    default: assert(false && "Processor does not contain stage");
    }
    Q_UNREACHABLE();
    // clang-format on
  }
  AInt nextFetchedAddress() const override { return pc_src->out.uValue(); }
  QString stageName(StageIndex idx) const override {
    // clang-format off
    switch (idx.index()) {
    case IF: return "IF";
    case ID: return "ID";
    case EX: return "EX";
    case MEM: return "MEM";
    case WB: return "WB";
    default: assert(false && "Processor does not contain stage");
    }
    Q_UNREACHABLE();
    // clang-format on
  }
  StageInfo stageInfo(StageIndex stage) const override {
    bool stageValid = true;
    // Has the pipeline stage been filled?
    stageValid &= stage.index() <= m_cycleCount;

    // clang-format off
    // Has the stage been cleared?
    switch(stage.index()){
    case ID: stageValid &= ifid_reg->valid_out.uValue(); break;
    case EX: stageValid &= idex_reg->valid_out.uValue(); break;
    case MEM: stageValid &= exmem_reg->valid_out.uValue(); break;
    case WB: stageValid &= memwb_reg->valid_out.uValue(); break;
    default: case IF: break;
    }

    // Is the stage carrying a valid (executable) PC?
    switch(stage.index()){
    case ID: stageValid &= isExecutableAddress(ifid_reg->pc_out.uValue()); break;
    case EX: stageValid &= isExecutableAddress(idex_reg->pc_out.uValue()); break;
    case MEM: stageValid &= isExecutableAddress(exmem_reg->pc_out.uValue()); break;
    case WB: stageValid &= isExecutableAddress(memwb_reg->pc_out.uValue()); break;
    default: case IF: stageValid &= isExecutableAddress(pc_reg->out.uValue()); break;
    }

    // Are we currently clearing the pipeline due to a syscall exit? if such, all stages before the EX stage are invalid
    if(stage.index() < EX){
      stageValid &= !ecallChecker->isSysCallExiting();
    }
    // clang-format on

    // Gather stage state info
    StageInfo::State state = StageInfo ::State::None;
    switch (stage.index()) {
    case IF:
      break;
    case ID:
      if (m_cycleCount > ID && ifid_reg->valid_out.uValue() == 0) {
        state = StageInfo::State::Flushed;
      }
      break;
    case EX: {
      if (idex_reg->stalled_out.uValue() == 1) {
        state = StageInfo::State::Stalled;
      } else if (m_cycleCount > EX && idex_reg->valid_out.uValue() == 0) {
        state = StageInfo::State::Flushed;
      }
      break;
    }
    case MEM: {
      if (exmem_reg->stalled_out.uValue() == 1) {
        state = StageInfo::State::Stalled;
      } else if (m_cycleCount > MEM && exmem_reg->valid_out.uValue() == 0) {
        state = StageInfo::State::Flushed;
      }
      break;
    }
    case WB: {
      if (memwb_reg->stalled_out.uValue() == 1) {
        state = StageInfo::State::Stalled;
      } else if (m_cycleCount > WB && memwb_reg->valid_out.uValue() == 0) {
        state = StageInfo::State::Flushed;
      }
      break;
    }
    }

    return StageInfo({getPcForStage(stage), stageValid, state});
  }

  void setProgramCounter(AInt address) override {
    pc_reg->forceValue(0, address);
    propagateDesign();
  }
  void setPCInitialValue(AInt address) override {
    pc_reg->setInitValue(address);
  }
  AddressSpaceMM &getMemory() override { return *m_memory; }
  VInt getRegister(RegisterFileType, unsigned i) const override {
    return registerFile->getRegister(i);
  }
  void finalize(FinalizeReason fr) override {
    if ((fr & FinalizeReason::exitSyscall) &&
        !ecallChecker->isSysCallExiting()) {
      // An exit system call was executed. Record the cycle of the execution,
      // and enable the ecallChecker's system call exiting signal.
      m_syscallExitCycle = m_cycleCount;
    }
    ecallChecker->setSysCallExiting(ecallChecker->isSysCallExiting() ||
                                    (fr & FinalizeReason::exitSyscall));
  }
  const std::vector<StageIndex> breakpointTriggeringStages() const override {
    return {{0, IF}};
  }

  MemoryAccess dataMemAccess() const override {
    return memToAccessInfo(data_mem);
  }
  MemoryAccess instrMemAccess() const override {
    auto instrAccess = memToAccessInfo(instr_mem);
    instrAccess.type = MemoryAccess::Read;
    return instrAccess;
  }

  bool finished() const override {
    // The processor is finished when there are no more valid instructions in
    // the pipeline
    bool allStagesInvalid = true;
    for (int stage = IF; stage < STAGECOUNT; stage++) {
      allStagesInvalid &= !stageInfo({0, stage}).stage_valid;
      if (!allStagesInvalid)
        break;
    }
    return allStagesInvalid;
  }

  void setRegister(RegisterFileType, unsigned i, VInt v) override {
    setSynchronousValue(registerFile->_wr_mem, i, v);
  }

  void clockProcessor() override {
    // An instruction has been retired if the instruction in the WB stage is
    // valid and the PC is within the executable range of the program
    if (memwb_reg->valid_out.uValue() != 0 &&
        isExecutableAddress(memwb_reg->pc_out.uValue())) {
      m_instructionsRetired++;
    }

    Design::clock();
  }

  void reverse() override {
    if (m_syscallExitCycle != -1 && m_cycleCount == m_syscallExitCycle) {
      // We are about to undo an exit syscall instruction. In this case, the
      // syscall exiting sequence should be terminate
      ecallChecker->setSysCallExiting(false);
      m_syscallExitCycle = -1;
    }
    Design::reverse();
    if (memwb_reg->valid_out.uValue() != 0 &&
        isExecutableAddress(memwb_reg->pc_out.uValue())) {
      m_instructionsRetired--;
    }
  }

  void reset() override {
    ecallChecker->setSysCallExiting(false);
    Design::reset();
    m_syscallExitCycle = -1;
  }

  static ProcessorISAInfo supportsISA() {
    return ProcessorISAInfo{
        std::make_shared<ISAInfo<XLenToRVISA<XLEN>()>>(QStringList()),
        {"M", "C"},
        {"M"}};
  }
  const ISAInfoBase *implementsISA() const override {
    return m_enabledISA.get();
  }

  const std::set<RegisterFileType> registerFiles() const override {
    std::set<RegisterFileType> rfs;
    rfs.insert(RegisterFileType::GPR);

    if (implementsISA()->extensionEnabled("F")) {
      rfs.insert(RegisterFileType::FPR);
    }
    return rfs;
  }

private:
  /**
   * @brief m_syscallExitCycle
   * The variable will contain the cycle of which an exit system call was
   * executed. From this, we may determine when we roll back an exit system call
   * during rewinding.
   */
  long long m_syscallExitCycle = -1;
  std::shared_ptr<ISAInfoBase> m_enabledISA;
  ProcessorStructure m_structure = {{0, 5}};
};

} // namespace core
} // namespace vsrtl
