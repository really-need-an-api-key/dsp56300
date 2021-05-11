#include "jit.h"

#include "dsp.h"
#include "jitblock.h"
#include "jitops.h"

#include "asmjit/core/jitruntime.h"
#include "asmjit/x86/x86assembler.h"

using namespace asmjit;
using namespace x86;

/*
	The idea of this JIT is to keep all relevant DSP registers in X64 registers. Register allocation looks like this:

	RAX	= func ret val                             |               XMM00 = DSP AGU 0 [0,M,N,R]
	RBX							*                  |               XMM01 = DSP AGU 1 [0,M,N,R]
	RCX	= func arg 0 (Microsoft)                   |               XMM02 = DSP AGU 2 [0,M,N,R]
	RDX	= func arg 1 / temp			               |               XMM03 = DSP AGU 3 [0,M,N,R]
	RBP							*                  |               XMM04 = DSP AGU 4 [0,M,N,R]
	RSI	= temp						               |               XMM05 = DSP AGU 5 [0,M,N,R]
	RDI	= func arg 0 (linux)    				   |               XMM06 = DSP AGU 6 [0,M,N,R]
	RSP							*	               |               XMM07 = DSP AGU 7 [0,M,N,R]
	R8  = func arg 2 / DSP Status Register		   |               XMM08 = DSP A
	R9  = func arg 3 / DSP Program Counter		   |               XMM09 = DSP B
	R10 = DSP Loop Counter			               |               XMM10 = DSP X
	R11 = DSP Loop Address			               |               XMM11 = DSP Y
	R12	= temp                  *                  |               XMM12 = last modified ALU for lazy SR updates
	R13	= temp                  *                  |               XMM13 = temp
	R14	= temp                  *                  |               XMM14 = temp
	R15	= temp                  *                  |               XMM15 = temp

	* = callee-save = we need to restore the previous register state before returning
*/

namespace dsp56k
{
	Jit::Jit(DSP& _dsp) : m_dsp(_dsp)
	{
		m_code.init(m_rt.environment());

		m_asm = new Assembler(&m_code);

		{
			JitBlock block(*m_asm, m_dsp);

			JitOps ops(block);

			m_dsp.regs().a.var = 0x00ff112233445566;

			ops.op_Abs(0);

			/*
			regs.getR(rax, 0);
			regs.getN(rcx, 0);
			regs.getM(rdx, 0);

			regs.getR(Jitmem::ptr(*m_asm, m_dsp.regs().r[7]), 0);
			regs.getN(Jitmem::ptr(*m_asm, m_dsp.regs().n[7]), 0);
			regs.getM(Jitmem::ptr(*m_asm, m_dsp.regs().m[7]), 0);
			*/
		}

		m_asm->ret();

		typedef void (*Func)(TWord op);

		Func func;

		const auto err = m_rt.add(&func, &m_code);

		if(err)
		{
			const auto* const errString = DebugUtils::errorAsString(err);
			LOG("JIT failed: " << err << " - " << errString);
			return;
		}

		func(0x123456);
	}

	Jit::~Jit()
	{
		delete m_asm;
	}
}
