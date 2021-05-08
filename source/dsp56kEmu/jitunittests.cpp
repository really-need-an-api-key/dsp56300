#include "jitunittests.h"

#include "jitblock.h"
#include "jitops.h"

namespace dsp56k
{
	JitUnittests::JitUnittests()
	: mem(m_defaultMemoryValidator, 0x100)
	, dsp(mem, &peripherals, &peripherals)
	, m_checks({})
	{
		runTest(&JitUnittests::conversion_build, &JitUnittests::conversion_verify);
		runTest(&JitUnittests::signextend_build, &JitUnittests::signextend_verify);
		runTest(&JitUnittests::ccr_u_build, &JitUnittests::ccr_u_verify);

		runTest(&JitUnittests::abs_build, &JitUnittests::abs_verify);
		runTest(&JitUnittests::add_build, &JitUnittests::add_verify);
		runTest(&JitUnittests::addShortImmediate_build, &JitUnittests::addShortImmediate_verify);
		runTest(&JitUnittests::addLongImmediate_build, &JitUnittests::addLongImmediate_verify);
		runTest(&JitUnittests::addl_build, &JitUnittests::addl_verify);
		runTest(&JitUnittests::addr_build, &JitUnittests::addr_verify);
	}

	void JitUnittests::runTest(void( JitUnittests::* _build)(JitBlock&, JitOps&), void( JitUnittests::* _verify)())
	{
		asmjit::CodeHolder code;
		code.init(m_rt.environment());

		asmjit::x86::Assembler m_asm(&code);

		{
			JitBlock block(m_asm, dsp);

			JitOps ops(block);

			(this->*_build)(block, ops);
		}

		m_asm.ret();

		typedef void (*Func)();
		Func func;
		const auto err = m_rt.add(&func, &code);
		if(err)
		{
			const auto* const errString = asmjit::DebugUtils::errorAsString(err);
			LOG("JIT failed: " << err << " - " << errString);
			return;
		}

		func();

		(this->*_verify)();

		m_rt.release(&func);
	}

	void JitUnittests::conversion_build(JitBlock& _block, JitOps& _ops)
	{
		_block.asm_().bind(_block.asm_().newNamedLabel("test_conv"));

		dsp.regs().x.var = 0xffeedd112233;
		dsp.regs().y.var = 0x112233445566;

		const RegGP r0(_block.gpPool());
		const RegGP r1(_block.gpPool());

		_ops.XY0to56(r0, 0);
		_ops.XY1to56(r1, 0);

		_block.mem().mov(m_checks[0], r0);
		_block.mem().mov(m_checks[1], r1);
	}

	void JitUnittests::conversion_verify()
	{
		assert(m_checks[0] == 0x0000112233000000);
		assert(m_checks[1] == 0x00ffffeedd000000);
	}

	void JitUnittests::signextend_build(JitBlock& _block, JitOps& _ops)
	{
		m_checks[0] = 0xabcdef;
		m_checks[1] = 0x123456;
		m_checks[2] = 0xabcdef123456;
		m_checks[3] = 0x123456abcdef;
		m_checks[4] = 0xab123456abcdef;
		m_checks[5] = 0x12123456abcdef;

		const RegGP ra(_block.gpPool());
		const RegGP rb(_block.gpPool());

		_block.mem().mov(regPC, m_checks[0]);
		_block.mem().mov(regLC, m_checks[1]);
		_block.mem().mov(regSR, m_checks[2]);
		_block.mem().mov(regLA, m_checks[3]);
		_block.mem().mov(ra, m_checks[4]);
		_block.mem().mov(rb, m_checks[5]);

		_ops.signextend24to56(regPC);
		_ops.signextend24to56(regLC);
		_ops.signextend48to56(regSR);
		_ops.signextend48to56(regLA);
		_ops.signextend56to64(ra);
		_ops.signextend56to64(rb);

		_block.mem().mov(m_checks[0], regPC);
		_block.mem().mov(m_checks[1], regLC);
		_block.mem().mov(m_checks[2], regSR);
		_block.mem().mov(m_checks[3], regLA);
		_block.mem().mov(m_checks[4], ra);
		_block.mem().mov(m_checks[5], rb);
	}

	void JitUnittests::signextend_verify()
	{
		assert(m_checks[0] == 0xffffffffabcdef);
		assert(m_checks[1] == 0x00000000123456);

		assert(m_checks[2] == 0xffabcdef123456);
		assert(m_checks[3] == 0x00123456abcdef);

		assert(m_checks[4] == 0xffab123456abcdef);
		assert(m_checks[5] == 0x0012123456abcdef);
	}

	void JitUnittests::ccr_u_build(JitBlock& _block, JitOps& _ops)
	{
		dsp.reg.sr.var = 0;

		m_checks[0] = 0xee012233445566;
		m_checks[1] = 0xee412233445566;
		m_checks[2] = 0xee812233445566;
		m_checks[3] = 0xeec12233445566;

		for(auto i=0; i<4; ++i)
		{
			RegGP r(_block);

			_block.asm_().mov(r, m_checks[i]);
			_ops.ccr_u_update(r);
			_block.mem().mov(m_checks[i], regSR);
		}
	}

	void JitUnittests::ccr_u_verify()
	{
		assert((m_checks[0] & SR_U));
		assert(!(m_checks[1] & SR_U));
		assert(!(m_checks[2] & SR_U));
		assert((m_checks[3] & SR_U));
	}

	void JitUnittests::abs_build(JitBlock& _block, JitOps& _ops)
	{
		dsp.regs().a.var = 0x00ff112233445566;
		dsp.regs().b.var = 0x0000aabbccddeeff;

		_ops.op_Abs(0x000000);
		_ops.op_Abs(0xffffff);
	}

	void JitUnittests::abs_verify()
	{
		assert(dsp.regs().a == 0x00EEDDCCBBAA9A);
		assert(dsp.regs().b == 0x0000aabbccddeeff);
	}

	void JitUnittests::add_build(JitBlock& _block, JitOps& _ops)
	{
		dsp.regs().a.var = 0x0001e000000000;
		dsp.regs().b.var = 0xfffe2000000000;

		// add b,a
		_ops.emit(0, 0x200010);
	}

	void JitUnittests::add_verify()
	{
		assert(dsp.regs().a.var == 0);
		assert(dsp.sr_test(SR_C));
		assert(dsp.sr_test(SR_Z));
		assert(!dsp.sr_test(SR_V));
	}

	void JitUnittests::addShortImmediate_build(JitBlock& _block, JitOps& _ops)
	{
		dsp.reg.a.var = 0;

		// add #<32,a
		_ops.emit(0, 0x017280);
	}

	void JitUnittests::addShortImmediate_verify()
	{
		assert(dsp.regs().a.var == 0x00000032000000);
		assert(!dsp.sr_test(SR_C));
		assert(!dsp.sr_test(SR_Z));
		assert(!dsp.sr_test(SR_V));
	}

	void JitUnittests::addLongImmediate_build(JitBlock& _block, JitOps& _ops)
	{
		dsp.reg.a.var = 0;
		dsp.reg.pc.var = 0;

		// add #>32,a, two op add with immediate in extension word
		dsp.mem.set(MemArea_P, 1, 0x000032);
		_ops.emit(0, 0x0140c0);
	}

	void JitUnittests::addLongImmediate_verify()
	{
		assert(dsp.regs().a.var == 0x00000032000000);
		assert(!dsp.sr_test(SR_C));
		assert(!dsp.sr_test(SR_Z));
		assert(!dsp.sr_test(SR_V));
	}

	void JitUnittests::addl_build(JitBlock& _block, JitOps& _ops)
	{
		dsp.reg.a.var = 0x222222;
		dsp.reg.b.var = 0x333333;

		_ops.emit(0, 0x20001a);
	}

	void JitUnittests::addl_verify()
	{
		assert(dsp.reg.b.var == 0x888888);
		assert(!dsp.sr_test(SR_C));
		assert(!dsp.sr_test(SR_Z));
		assert(!dsp.sr_test(SR_V));
	}

	void JitUnittests::addr_build(JitBlock& _block, JitOps& _ops)
	{
		dsp.reg.a.var = 0x004edffe000000;
		dsp.reg.b.var = 0xff89fe13000000;
		dsp.setSR(0x0800d0);

		_ops.emit(0, 0x200002);	// addr b,a
	}

	void JitUnittests::addr_verify()
	{
		assert(dsp.reg.a.var == 0x0ffb16e12000000);
		assert(dsp.getSR().var == 0x0800c8);		
	}
}
