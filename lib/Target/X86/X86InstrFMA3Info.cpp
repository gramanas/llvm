//===-- X86InstrFMA3Info.cpp - X86 FMA3 Instruction Information -----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the implementation of the classes providing information
// about existing X86 FMA3 opcodes, classifying and grouping them.
//
//===----------------------------------------------------------------------===//

#include "X86InstrFMA3Info.h"
#include "X86InstrInfo.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/Threading.h"
#include <cassert>
#include <cstdint>

using namespace llvm;

#define FMA3GROUP(Name, Suf, Attrs) \
  { { X86::Name##132##Suf, X86::Name##213##Suf, X86::Name##231##Suf }, Attrs },

#define FMA3GROUP_MASKED(Name, Suf, Attrs) \
  FMA3GROUP(Name, Suf, Attrs) \
  FMA3GROUP(Name, Suf##k, Attrs | X86InstrFMA3Group::KMergeMasked) \
  FMA3GROUP(Name, Suf##kz, Attrs | X86InstrFMA3Group::KZeroMasked)

#define FMA3GROUP_PACKED_WIDTHS(Name, Suf, Attrs) \
  FMA3GROUP(Name, Suf##Ym, Attrs) \
  FMA3GROUP(Name, Suf##Yr, Attrs) \
  FMA3GROUP_MASKED(Name, Suf##Z128m, Attrs) \
  FMA3GROUP_MASKED(Name, Suf##Z128r, Attrs) \
  FMA3GROUP_MASKED(Name, Suf##Z256m, Attrs) \
  FMA3GROUP_MASKED(Name, Suf##Z256r, Attrs) \
  FMA3GROUP_MASKED(Name, Suf##Zm, Attrs) \
  FMA3GROUP_MASKED(Name, Suf##Zr, Attrs) \
  FMA3GROUP(Name, Suf##m, Attrs) \
  FMA3GROUP(Name, Suf##r, Attrs)

#define FMA3GROUP_PACKED(Name, Attrs) \
  FMA3GROUP_PACKED_WIDTHS(Name, PD, Attrs) \
  FMA3GROUP_PACKED_WIDTHS(Name, PS, Attrs)

#define FMA3GROUP_SCALAR_WIDTHS(Name, Suf, Attrs) \
  FMA3GROUP(Name, Suf##Zm, Attrs) \
  FMA3GROUP_MASKED(Name, Suf##Zm_Int, Attrs | X86InstrFMA3Group::Intrinsic) \
  FMA3GROUP(Name, Suf##Zr, Attrs) \
  FMA3GROUP_MASKED(Name, Suf##Zr_Int, Attrs | X86InstrFMA3Group::Intrinsic) \
  FMA3GROUP(Name, Suf##m, Attrs) \
  FMA3GROUP(Name, Suf##m_Int, Attrs | X86InstrFMA3Group::Intrinsic) \
  FMA3GROUP(Name, Suf##r, Attrs) \
  FMA3GROUP(Name, Suf##r_Int, Attrs | X86InstrFMA3Group::Intrinsic)

#define FMA3GROUP_SCALAR(Name, Attrs) \
  FMA3GROUP_SCALAR_WIDTHS(Name, SD, Attrs) \
  FMA3GROUP_SCALAR_WIDTHS(Name, SS, Attrs) \

#define FMA3GROUP_FULL(Name, Attrs) \
  FMA3GROUP_PACKED(Name, Attrs) \
  FMA3GROUP_SCALAR(Name, Attrs)

static const X86InstrFMA3Group Groups[] = {
  FMA3GROUP_FULL(VFMADD, 0)
  FMA3GROUP_PACKED(VFMADDSUB, 0)
  FMA3GROUP_FULL(VFMSUB, 0)
  FMA3GROUP_PACKED(VFMSUBADD, 0)
  FMA3GROUP_FULL(VFNMADD, 0)
  FMA3GROUP_FULL(VFNMSUB, 0)
};

#define FMA3GROUP_PACKED_AVX512_WIDTHS(Name, Type, Suf, Attrs) \
  FMA3GROUP_MASKED(Name, Type##Z128##Suf, Attrs) \
  FMA3GROUP_MASKED(Name, Type##Z256##Suf, Attrs) \
  FMA3GROUP_MASKED(Name, Type##Z##Suf, Attrs)

#define FMA3GROUP_PACKED_AVX512(Name, Suf, Attrs) \
  FMA3GROUP_PACKED_AVX512_WIDTHS(Name, PD, Suf, Attrs) \
  FMA3GROUP_PACKED_AVX512_WIDTHS(Name, PS, Suf, Attrs)

#define FMA3GROUP_PACKED_AVX512_ROUND(Name, Suf, Attrs) \
  FMA3GROUP_MASKED(Name, PDZ##Suf, Attrs) \
  FMA3GROUP_MASKED(Name, PSZ##Suf, Attrs)

#define FMA3GROUP_SCALAR_AVX512(Name, Suf, Attrs) \
  FMA3GROUP_MASKED(Name, SDZ##Suf, Attrs) \
  FMA3GROUP_MASKED(Name, SSZ##Suf, Attrs)

static const X86InstrFMA3Group BroadcastGroups[] = {
  FMA3GROUP_PACKED_AVX512(VFMADD, mb, 0)
  FMA3GROUP_PACKED_AVX512(VFMADDSUB, mb, 0)
  FMA3GROUP_PACKED_AVX512(VFMSUB, mb, 0)
  FMA3GROUP_PACKED_AVX512(VFMSUBADD, mb, 0)
  FMA3GROUP_PACKED_AVX512(VFNMADD, mb, 0)
  FMA3GROUP_PACKED_AVX512(VFNMSUB, mb, 0)
};

static const X86InstrFMA3Group RoundGroups[] = {
  FMA3GROUP_PACKED_AVX512_ROUND(VFMADD, rb, 0)
  FMA3GROUP_SCALAR_AVX512(VFMADD, rb_Int, X86InstrFMA3Group::Intrinsic)
  FMA3GROUP_PACKED_AVX512_ROUND(VFMADDSUB, rb, 0)
  FMA3GROUP_PACKED_AVX512_ROUND(VFMSUB, rb, 0)
  FMA3GROUP_SCALAR_AVX512(VFMSUB, rb_Int, X86InstrFMA3Group::Intrinsic)
  FMA3GROUP_PACKED_AVX512_ROUND(VFMSUBADD, rb, 0)
  FMA3GROUP_PACKED_AVX512_ROUND(VFNMADD, rb, 0)
  FMA3GROUP_SCALAR_AVX512(VFNMADD, rb_Int, X86InstrFMA3Group::Intrinsic)
  FMA3GROUP_PACKED_AVX512_ROUND(VFNMSUB, rb, 0)
  FMA3GROUP_SCALAR_AVX512(VFNMSUB, rb_Int, X86InstrFMA3Group::Intrinsic)
};

static void verifyTables() {
#ifndef NDEBUG
  static std::atomic<bool> TableChecked(false);
  if (!TableChecked.load(std::memory_order_relaxed)) {
    assert(std::is_sorted(std::begin(Groups), std::end(Groups)) &&
           std::is_sorted(std::begin(RoundGroups), std::end(RoundGroups)) &&
           std::is_sorted(std::begin(BroadcastGroups),
                          std::end(BroadcastGroups)) &&
           "FMA3 tables not sorted!");
    TableChecked.store(true, std::memory_order_relaxed);
  }
#endif
}

/// Returns a reference to a group of FMA3 opcodes to where the given
/// \p Opcode is included. If the given \p Opcode is not recognized as FMA3
/// and not included into any FMA3 group, then nullptr is returned.
const X86InstrFMA3Group *llvm::getFMA3Group(unsigned Opcode, uint64_t TSFlags) {

  // FMA3 instructions have a well defined encoding pattern we can exploit.
  uint8_t BaseOpcode = X86II::getBaseOpcodeFor(TSFlags);
  bool IsFMA3 = ((TSFlags & X86II::EncodingMask) == X86II::VEX ||
                 (TSFlags & X86II::EncodingMask) == X86II::EVEX) &&
                (TSFlags & X86II::OpMapMask) == X86II::T8 &&
                (TSFlags & X86II::OpPrefixMask) == X86II::PD &&
                ((BaseOpcode >= 0x96 && BaseOpcode <= 0x9F) ||
                 (BaseOpcode >= 0xA6 && BaseOpcode <= 0xAF) ||
                 (BaseOpcode >= 0xB6 && BaseOpcode <= 0xBF));
  if (!IsFMA3)
    return nullptr;

  verifyTables();

  ArrayRef<X86InstrFMA3Group> Table;
  if (TSFlags & X86II::EVEX_RC)
    Table = makeArrayRef(RoundGroups);
  else if (TSFlags & X86II::EVEX_B)
    Table = makeArrayRef(BroadcastGroups);
  else
    Table = makeArrayRef(Groups);

  // FMA 132 instructions have an opcode of 0x96-0x9F
  // FMA 213 instructions have an opcode of 0xA6-0xAF
  // FMA 231 instructions have an opcode of 0xB6-0xBF
  unsigned FormIndex = ((BaseOpcode - 0x90) >> 4) & 0x3;

  auto I = std::lower_bound(Table.begin(), Table.end(), Opcode,
                            [FormIndex](const X86InstrFMA3Group &Group,
                                        unsigned Opcode) {
                              return Group.Opcodes[FormIndex] < Opcode;
                            });
  assert(I != Table.end() && I->Opcodes[FormIndex] == Opcode &&
         "Couldn't find FMA3 opcode!");
  return I;
}
