#include "arm_pseudo_code_func.h"


int arm_op_exec_arm_bic_imm(struct TargetCore *core,  arm_bic_imm_input_type *in, arm_bic_imm_output_type *out)
{
	int ret = 0;
	uint32 result;
	uint32 *status = cpu_get_status(core);
	out->next_address = core->pc + INST_ARM_SIZE;
	out->passed = ConditionPassed(in->cond, *status);
	if (out->passed != FALSE) {
		result = (in->Rn.regData & ~(in->imm32));
		if (in->Rd.regId != CpuRegId_PC) {
			cpu_set_reg(core, in->Rd.regId, result);
			if (in->S != 0) {
				CPU_STATUS_BIT_UPDATE(status, CPU_STATUS_BITPOS_C, out->status_flag.carry);
				CPU_STATUS_BIT_UPDATE(status, CPU_STATUS_BITPOS_Z, (result == 0));
				CPU_STATUS_BIT_UPDATE(status, CPU_STATUS_BITPOS_N, ((result & (1U << 31U)) != 0));
			}
			out->Rd.regData = result;
		}
		else {
			ret = ALUWritePC(&out->next_address, status, result);
			out->Rd.regData = out->next_address;
		}
	}
	out->status = *status;
	return ret;
}


int arm_op_exec_arm_orr_imm(struct TargetCore *core,  arm_orr_imm_input_type *in, arm_orr_imm_output_type *out)
{
	int ret = 0;
	uint32 result;
	uint32 *status = cpu_get_status(core);
	out->next_address = core->pc + INST_ARM_SIZE;
	out->passed = ConditionPassed(in->cond, *status);
	if (out->passed != FALSE) {
		result = (in->Rn.regData | (in->imm32));
		if (in->Rd.regId != CpuRegId_PC) {
			cpu_set_reg(core, in->Rd.regId, result);
			if (in->S != 0) {
				CPU_STATUS_BIT_UPDATE(status, CPU_STATUS_BITPOS_C, out->status_flag.carry);
				CPU_STATUS_BIT_UPDATE(status, CPU_STATUS_BITPOS_Z, (result == 0));
				CPU_STATUS_BIT_UPDATE(status, CPU_STATUS_BITPOS_N, ((result & (1U << 31U)) != 0));
			}
			out->Rd.regData = result;
		}
		else {
			ret = ALUWritePC(&out->next_address, status, result);
			out->Rd.regData = out->next_address;
		}
	}
	out->status = *status;
	return ret;
}


int arm_op_exec_arm_and_imm(struct TargetCore *core,  arm_and_imm_input_type *in, arm_and_imm_output_type *out)
{
	int ret = 0;
	uint32 result;
	uint32 *status = cpu_get_status(core);
	out->next_address = core->pc + INST_ARM_SIZE;
	out->passed = ConditionPassed(in->cond, *status);
	if (out->passed != FALSE) {
		result = (in->Rn.regData & (in->imm32));
		if (in->Rd.regId != CpuRegId_PC) {
			cpu_set_reg(core, in->Rd.regId, result);
			if (in->S != 0) {
				CPU_STATUS_BIT_UPDATE(status, CPU_STATUS_BITPOS_C, out->status_flag.carry);
				CPU_STATUS_BIT_UPDATE(status, CPU_STATUS_BITPOS_Z, (result == 0));
				CPU_STATUS_BIT_UPDATE(status, CPU_STATUS_BITPOS_N, ((result & (1U << 31U)) != 0));
			}
			out->Rd.regData = result;
		}
		else {
			ret = ALUWritePC(&out->next_address, status, result);
			out->Rd.regData = out->next_address;
		}
	}
	out->status = *status;
	return ret;
}



int arm_op_exec_arm_bfc(struct TargetCore *core,  arm_bfc_input_type *in, arm_bfc_output_type *out)
{
	int ret = 0;
	uint32 *status = cpu_get_status(core);
	out->next_address = core->pc + INST_ARM_SIZE;
	out->passed = ConditionPassed(in->cond, *status);
	if (out->passed != FALSE) {
		if (in->msbit >= in->lsbit) {
			//R[d]<msbit:lsbit> = Replicate(‘0’, msbit-lsbit+1);
			int i;
			uint32 value = 0xFFFFFFFF;
			for (i = in->lsbit; i <= in->msbit; i++) {
				value = ( value & ~(1U << i) );
			}
			out->Rd.regData = (out->Rd.regData & value);
			cpu_set_reg(core, out->Rd.regId, out->Rd.regData);
		}
		else {
			// Other bits of R[d] are unchanged
			//UNPREDICTABLE;
			ret = -1;
		}
	}
	out->status = *status;
	return ret;
}


int arm_op_exec_arm_utx(struct TargetCore *core,  arm_utx_input_type *in, arm_utx_output_type *out)
{
	uint32 *status = cpu_get_status(core);
	out->next_address = core->pc + INST_ARM_SIZE;
	out->passed = ConditionPassed(in->cond, *status);
	if (out->passed != FALSE) {
		//rotated = ROR(R[m], rotation);
		uint32 rotated = ROR(32, in->Rm.regData, in->rotate);
		switch (in->size) {
		case 1:
			//R[d] = ZeroExtend(rotated<7:0>, 32);
			out->Rd.regData = (rotated & 0xFF);
			break;
		case 2:
			//R[d] = ZeroExtend(rotated<15:0>, 32);
			out->Rd.regData = (rotated & 0xFFFF);
			break;
		default:
			return -1;
			break;
		}
		cpu_set_reg(core, out->Rd.regId, out->Rd.regData);
	}
	out->status = *status;
	return 0;
}