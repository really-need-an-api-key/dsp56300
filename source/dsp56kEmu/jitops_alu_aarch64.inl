#pragma once

#include "jitops.h"
#include "jittypes.h"
#include "asmjit/core/operand.h"

namespace dsp56k
{
	void JitOps::XY0to56(const JitReg64& _dst, int _xy) const
	{
		const auto src = m_dspRegs.getXY(_xy, JitDspRegs::Read);

		// We might be able to save one op by using SBFM but unfortunately the docs are really bad
		m_asm.lsl(_dst, src, asmjit::Imm(40));
		m_asm.asr(_dst, _dst, asmjit::Imm(8));
		m_asm.lsr(_dst, _dst, asmjit::Imm(8));
	}

	void JitOps::XY1to56(const JitReg64& _dst, int _xy) const
	{
		m_dspRegs.getXY(_dst, _xy);
		m_asm.shr(_dst, asmjit::Imm(24));	// remove LSWord
		signed24To56(_dst);
	}

	inline void JitOps::alu_abs(const JitRegGP& _r)
	{
		m_asm.cmp(_r, asmjit::Imm(0));
		m_asm.cneg(_r, _r, asmjit::arm::Cond::kLT);			// negate if < 0
	}
	
	inline void JitOps::alu_and(const TWord ab, RegGP& _v)
	{
		m_asm.shl(_v, asmjit::Imm(24));

		AluRef alu(m_block, ab);

		{
			const RegGP r(m_block);
			m_asm.and_(r, alu.get(), _v.get());
			ccr_update_ifZero(CCRB_Z);
		}

		{
			m_asm.orr(_v, _v, asmjit::Imm(0xff000000ffffff));
			m_asm.and_(alu, alu, _v.get());
		}

		_v.release();

		// S L E U N Z V C
		// v - - - * * * -
		ccr_n_update_by47(alu);
		ccr_clear(CCR_V);
	}

	inline void JitOps::alu_asl(const TWord _abSrc, const TWord _abDst, const ShiftReg& _v)
	{
		AluReg alu(m_block, _abDst, false, _abDst != _abSrc);
		if (_abDst != _abSrc)
			m_asm.mov(alu.get(), m_dspRegs.getALU(_abSrc, JitDspRegs::Read));

		m_asm.lsl(alu, alu, asmjit::Imm(8));		// we want to hit the 64 bit boundary to make use of the native carry flag so pre-shift by 8 bit (56 => 64)

		m_asm.lsl(alu, alu, _v.get());				// now do the real shift

		ccr_update_ifCarry(CCRB_C);					// copy the host carry flag to the DSP carry flag

		// Overflow: Set if Bit 55 is changed any time during the shift operation, cleared otherwise.
		// The easiest way to check this is to shift back and compare if the initial alu value is identical ot the backshifted one
		{
			AluReg oldAlu(m_block, _abSrc, true);
			m_asm.lsl(oldAlu, oldAlu, asmjit::Imm(8));
			m_asm.asr(alu, alu, _v.get());
			m_asm.cmp(alu, oldAlu.get());
		}

		ccr_update_ifNotZero(CCRB_V);

		m_asm.lsl(alu, alu, _v.get());					// one more time
		m_asm.lsr(alu, alu, asmjit::Imm(8));			// correction

		ccr_dirty(_abDst, alu, static_cast<CCRMask>(CCR_E | CCR_N | CCR_U | CCR_Z));
	}
	
	inline void JitOps::alu_asr(const TWord _abSrc, const TWord _abDst, const ShiftReg& _v)
	{
		AluRef alu(m_block, _abDst, _abDst == _abSrc, true);
		if (_abDst != _abSrc)
			m_asm.mov(alu.get(), m_dspRegs.getALU(_abSrc, JitDspRegs::Read));

		m_asm.lsl(alu, alu, asmjit::Imm(8));
		m_asm.asr(alu, alu, _v.get());
		m_asm.asr(alu, alu, asmjit::Imm(8));
		ccr_update_ifCarry(CCRB_C);					// copy the host carry flag to the DSP carry flag
		m_dspRegs.mask56(alu);

		ccr_clear(CCR_V);
		ccr_dirty(_abDst, alu, static_cast<CCRMask>(CCR_E | CCR_N | CCR_U | CCR_Z));
	}

	inline void JitOps::alu_bclr(const JitReg64& _dst, const TWord _bit)
	{
		m_asm.ands(asmjit::a64::regs::xzr, _dst, asmjit::Imm(1ull << _bit));
		ccr_update_ifNotZero(CCRB_C);
		m_asm.and_(_dst, _dst, asmjit::Imm(~(1ull << _bit)));
	}

	inline void JitOps::alu_bset(const JitReg64& _dst, const TWord _bit)
	{
		m_asm.ands(asmjit::a64::regs::xzr, _dst, asmjit::Imm(1ull << _bit));
		ccr_update_ifNotZero(CCRB_C);
		m_asm.orr(_dst, _dst, asmjit::Imm(1ull << _bit));
	}

	inline void JitOps::alu_bchg(const JitReg64& _dst, const TWord _bit)
	{
		m_asm.ands(asmjit::a64::regs::xzr, _dst, asmjit::Imm(1ull << _bit));
		ccr_update_ifNotZero(CCRB_C);
		m_asm.eor(_dst, _dst, asmjit::Imm(1ull << _bit));
	}

	inline void JitOps::alu_lsl(TWord ab, int _shiftAmount)
	{
		const RegGP d(m_block);
		getALU1(d, ab);
		m_asm.shl(r32(d.get()), _shiftAmount + 8);	// + 8 to use native carry flag
		ccr_update_ifCarry(CCRB_C);
		m_asm.shr(r32(d.get()), 8);				// revert shift by 8
		ccr_update_ifZero(CCRB_Z);
		m_asm.bt(r32(d.get()), asmjit::Imm(23));
		ccr_update_ifCarry(CCRB_N);
		ccr_clear(CCR_V);
		setALU1(ab, r32(d.get()));
	}

	inline void JitOps::alu_lsr(TWord ab, int _shiftAmount)
	{
		const RegGP d(m_block);
		getALU1(d, ab);
		m_asm.shr(r32(d.get()), _shiftAmount);
		ccr_update_ifCarry(CCRB_C);
		m_asm.cmp(r32(d.get()), asmjit::Imm(0));
		ccr_update_ifZero(CCRB_Z);
		m_asm.bt(r32(d.get()), asmjit::Imm(23));
		ccr_update_ifCarry(CCRB_N);
		ccr_clear(CCR_V);
		setALU1(ab, r32(d.get()));
	}
}
