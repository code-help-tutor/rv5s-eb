WeChat: cstutorcs
QQ: 749389476
Email: tutorcs@163.com
#pragma once

#include "../riscv.h"

#include "VSRTL/core/vsrtl_component.h"

namespace vsrtl {
namespace core {
using namespace Ripes;

class ForwardingEBUnit : public Component {
public:
  ForwardingEBUnit(const std::string &name, SimComponent *parent)
      : Component(name, parent) {
    alu_reg1_forwarding_ctrl << [=] {
      const auto idx = id_reg1_idx.uValue();
      if (idx == 0) {
        return ForwardingSrc::IdStage;
      } else if (idx == mem_reg_wr_idx.uValue() && mem_reg_wr_en.uValue()) {
        return ForwardingSrc::MemStage;
      } else if (idx == wb_reg_wr_idx.uValue() && wb_reg_wr_en.uValue()) {
        return ForwardingSrc::WbStage;
      } else {
        return ForwardingSrc::IdStage;
      }
    };

    alu_reg2_forwarding_ctrl << [=] {
      const auto idx = id_reg2_idx.uValue();
      if (idx == 0) {
        return ForwardingSrc::IdStage;
      } else if (idx == mem_reg_wr_idx.uValue() && mem_reg_wr_en.uValue()) {
        return ForwardingSrc::MemStage;
      } else if (idx == wb_reg_wr_idx.uValue() && wb_reg_wr_en.uValue()) {
        return ForwardingSrc::WbStage;
      } else {
        return ForwardingSrc::IdStage;
      }
    };

    //--------------- TODO: implement earlier branch resolving ----------
    // Hint: forwarding logic like above
    // you can add/remove ports
    br_reg1_forwarding_ctrl << [=] {
      const auto idx = id_br_reg1_idx.uValue();
      if (idx == 0) {
        return ForwardingSrc::IdStage;
      } else if (idx == mem_reg_wr_idx.uValue() && mem_reg_wr_en.uValue()) {
        return ForwardingSrc::MemStage;
      } else if (idx == wb_reg_wr_idx.uValue() && wb_reg_wr_en.uValue()) {
        return ForwardingSrc::WbStage;
      } else {
        return ForwardingSrc::IdStage;
      }
    };

    br_reg2_forwarding_ctrl << [=] {
      const auto idx = id_br_reg2_idx.uValue();
      if (idx == 0) {
        return ForwardingSrc::IdStage;
      } else if (idx == mem_reg_wr_idx.uValue() && mem_reg_wr_en.uValue()) {
        return ForwardingSrc::MemStage;
      } else if (idx == wb_reg_wr_idx.uValue() && wb_reg_wr_en.uValue()) {
        return ForwardingSrc::WbStage;
      } else {
        return ForwardingSrc::IdStage;
      }
    };
  }
  }

  INPUTPORT(id_reg1_idx, c_RVRegsBits);
  INPUTPORT(id_reg2_idx, c_RVRegsBits);

  INPUTPORT(id_br_reg1_idx, c_RVRegsBits);
  INPUTPORT(id_br_reg2_idx, c_RVRegsBits);

  INPUTPORT(mem_reg_wr_idx, c_RVRegsBits);
  INPUTPORT(mem_reg_wr_en, 1);

  INPUTPORT(wb_reg_wr_idx, c_RVRegsBits);
  INPUTPORT(wb_reg_wr_en, 1);

  OUTPUTPORT_ENUM(alu_reg1_forwarding_ctrl, ForwardingSrc);
  OUTPUTPORT_ENUM(alu_reg2_forwarding_ctrl, ForwardingSrc);
  OUTPUTPORT_ENUM(br_reg1_forwarding_ctrl, ForwardingSrc);
  OUTPUTPORT_ENUM(br_reg2_forwarding_ctrl, ForwardingSrc);
};
} // namespace core
} // namespace vsrtl
