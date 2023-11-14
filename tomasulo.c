#include "header.h"
#include "stdbool.h"

Operation *ops = NULL;
Instruction *iq = NULL;

int op_count = 0;
int instr_count = 0;
int instr_proc = 0;
unsigned int cycles = 0;

RS *add_fp_rs;
RS *add_int_rs;
RS *mul_fp_rs;
RS *mul_int_rs;
RS *ld_rs;
RS *sd_rs;

static uint32_t mem[4096];

struct reg_status int_reg_status[NUM_INT_REGS];
struct reg_status float_reg_status[NUM_FLOAT_REGS];

//TODO. re-order buffer

void create_arch()
{
	int i;
	add_fp_rs = malloc (NUM_FLT_ADD_RS * sizeof (RS));
	add_int_rs = malloc (NUM_INT_ADD_RS * sizeof (RS));
	mul_fp_rs = malloc (NUM_FLT_MUL_RS * sizeof (RS));
	mul_int_rs = malloc (NUM_INT_MUL_RS * sizeof (RS));
	ld_rs = malloc (NUM_LD_RS * sizeof (RS));
	sd_rs = malloc (NUM_SD_RS * sizeof (RS));

	if(!add_fp_rs || !add_int_rs || !mul_fp_rs || !mul_int_rs || !ld_rs || !sd_rs){
		fatal("Error creating the architecture\n");
	}

    memset(add_fp_rs, 0, NUM_FLT_ADD_RS * sizeof (RS));
    memset(add_int_rs, 0, NUM_INT_ADD_RS * sizeof (RS));
    memset(mul_fp_rs, 0, NUM_FLT_MUL_RS * sizeof (RS));
    memset(mul_int_rs, 0, NUM_INT_MUL_RS * sizeof (RS));
    memset(ld_rs, 0, NUM_LD_RS * sizeof (RS));
    memset(sd_rs, 0, NUM_SD_RS * sizeof (RS));

    memset(int_reg_status, 0, sizeof(struct reg_status) * NUM_INT_REGS);
    memset(float_reg_status, 0, sizeof(struct reg_status) * NUM_FLOAT_REGS);

    memset(mem, 0, sizeof(mem));

	for (i = 0; i < MAX_RS; i++) {
		if (i < NUM_FLT_ADD_RS) {
			add_fp_rs[i].status = AVAILABLE;
            sprintf(add_fp_rs[i].name, "FLT_ADD%d", i);
		}

		if (i < NUM_INT_ADD_RS) {
			add_int_rs[i].status = AVAILABLE;
            sprintf(add_int_rs[i].name, "INT_ADD%d", i);
		}

		if (i < NUM_FLT_MUL_RS) {
			mul_fp_rs[i].status = AVAILABLE;
            sprintf(mul_fp_rs[i].name, "FLT_MUL%d", i);
		}

		if (i < NUM_INT_MUL_RS) {
			mul_int_rs[i].status = AVAILABLE;
            sprintf(mul_int_rs[i].name, "INT_MUL%d", i);
		}

		if (i < NUM_LD_RS) {
			ld_rs[i].status = AVAILABLE;
            sprintf(ld_rs[i].name, "LD%d", i);
		}

		if (i < NUM_SD_RS) {
			sd_rs[i].status = AVAILABLE;
            sprintf(sd_rs[i].name, "SD%d", i);
		}
	}
}

RS *dep_lookup(char *src)
{
    int i, reg_num;
    reg_num = atoi(&src[1]);

    /* the src should be register */
    if(src[0] != 'R' && src[0] != 'F')
        return NULL;

    printf("reg num %d\n", reg_num);
    if(src[0] == 'R') {
        return int_reg_status[reg_num].rs;
    } else {
        return float_reg_status[reg_num].rs;
    }

	return NULL;
}

static void dump_rs_state(int stn_type, int no_stn)
{
	RS *rsrv_stn;
	int rs_no = 0;

	/*Find the reservation station requested */
	switch(stn_type){
		case FP_ADD: rsrv_stn = RES_STN(add,_fp);
            break;
		case FP_MUL: rsrv_stn = RES_STN(mul,_fp);
		    break;
		case INT_ADD: rsrv_stn = RES_STN(add,_int);
		    break;
		case INT_MUL: rsrv_stn = RES_STN(mul,_int);
		    break;
		case LD: rsrv_stn = RES_STN(ld,);
		    break;
		case SD: rsrv_stn = RES_STN(sd,);
		    break;

		default: fatal("Access to a non existed reservation station requested"); 
                break;
    }

	/*Cycle through_ Reservation Stations of the given type*/
	while(rs_no < no_stn){
        //| Name    | Busy    |  Addr.   | Op      | Vj      | Vk      | Qj      | Qk      |
        printf("| %s | %d | %d | %s | %d | %d | %s | %s |\n",
                rsrv_stn[rs_no].name,
                rsrv_stn[rs_no].status != AVAILABLE,
                ,
        rs_no++;
    }
}

static void update_rs_exec(int stn_type, int no_stn)
{
	RS *rsrv_stn;
	int rs_no = 0;

	/*Find the reservation station requested */
	switch(stn_type){
		case FP_ADD: rsrv_stn = RES_STN(add,_fp);
            break;
		case FP_MUL: rsrv_stn = RES_STN(mul,_fp);
		    break;
		case INT_ADD: rsrv_stn = RES_STN(add,_int);
		    break;
		case INT_MUL: rsrv_stn = RES_STN(mul,_int);
		    break;
		case LD: rsrv_stn = RES_STN(ld,);
		    break;
		case SD: rsrv_stn = RES_STN(sd,);
		    break;

		default: fatal("Access to a non existed reservation station requested");
                break;
    }

	/*Cycle through_ Reservation Stations of the given type*/
	while(rs_no < no_stn) {
		if(rsrv_stn[rs_no].status == BUSY) {
			if((rsrv_stn[rs_no].qj == NULL)&&(rsrv_stn[rs_no].qk == NULL)) {
				/*if the timer has not been set ,set it to the instruction latency*/
				if(rsrv_stn[rs_no].timer == 0) {
                    /* caculate the effective address */
                    if(stn_type == LD || stn_type == SD)
                        rsrv_stn[rs_no].addr += rsrv_stn[rs_no].vj;

					rsrv_stn[rs_no].timer = rsrv_stn[rs_no].instr->latency; /*TO DO Move the latency to RS ?*/
				} else {
                    /* start to execute */
					rsrv_stn[rs_no].timer--;
                }
			}
		}
	
		rs_no++;
	}
}

static void update_rs_write(int stn_type, int no_stn)
{
	RS *rsrv_stn;
	RS *rs_station;
	int rs_no = 0;
    int dst_reg_num = 0;

	/*Find the reservation station requested */
	switch(stn_type){
		case FP_ADD: rsrv_stn = RES_STN(add,_fp);
	        break;
		case FP_MUL: rsrv_stn = RES_STN(mul,_fp);
		    break;
		case INT_ADD: rsrv_stn = RES_STN(add,_int);
		    break;
		case INT_MUL: rsrv_stn = RES_STN(mul,_int);
		    break;
		case LD: rsrv_stn = RES_STN(ld,);
		    break;
		case SD: rsrv_stn = RES_STN(sd,);
		    break;
		default: fatal("Access to a non existed reservation station requested"); 
            break;
    }

	/*Cycle through the Reservation Stations of the given type*/
	while(rs_no < no_stn) {
        rs_station = &rsrv_stn[rs_no];
	    /*check if any RS is ready to move to write back stage*/
		if(rs_station.status == BUSY) {
			if((rs_station.qj == NULL)&&(rs_station.qk == NULL)) {
				/*if the functional unit has completed execution*/
				if(rs_station.timer == 0) {
					rs_station.status = RESULT_READY;
					rs_station.instr->exec_time = cycles;
				}
			}
	    /*Also check if any RS will complete write back this cycle*/
		} else if (rs_station.status == RESULT_READY) {
			/*update the write back time */
			rs_station.instr->write_time = cycles;
			
            dst_reg_num = atoi(&rs_station.instr->dest[1]);
            printf("dst_reg_num %d\n", dst_reg_num);

            /* update register file */
            switch(stn_type) {
		        case FP_ADD:
                    float_reg_status[dst_reg_num].reg_val.f_val = rs_station.vj.f_val +
                                                        rs_station.vk.f_val;
                    float_reg_status[dst_reg_num].rs = NULL;
                    break;
                case FP_MUL:
                    float_reg_status[dst_reg_num].reg_val.f_val = rs_station.vj.f_val *
                                                        rs_station.vk.f_val;
                    float_reg_status[dst_reg_num].rs = NULL;
                    break;
		        case INT_ADD:
                    int_reg_status[dst_reg_num].reg_val.i_val = rs_station.vj.i_val +
                                                        rs_station.vk.i_val;
                    int_reg_status[dst_reg_num].rs = NULL;
                    break;
		        case INT_MUL:
                    int_reg_status[dst_reg_num].reg_val.i_val = rs_station.vj.i_val *
                                                        rs_station.vk.i_val;
                    int_reg_status[dst_reg_num].rs = NULL;
                    break;
                /* LD SD only support integer at this point */
                case LD:
                    int_reg_status[dst_reg_num].i_val = mem[rs_station.addr];
                    int_reg_status[dst_reg_num].rs = NULL;
                    break;
                case SD:
                    //TODO is the reg value available to store ?
                    mem[rs_station.addr] = int_reg_status[dst_reg_num].i_val;
                    break;
                default:
                    printf("error instruction type \n");
                    break;
            }

            /* TODO, update related RS */

			/*Flust the instruction and reset the reservation station */
			rs_station.status = AVAILABLE;
			rs_station.instr = NULL;
			rs_station.qj = rs_station.qk = NULL;
		}
		rs_no++;
	}
}

void execute()
{
	/*Update the fields of all the reservations stations*/
    update_rs_exec(FP_ADD, NUM_FLT_ADD_RS);
    update_rs_exec(FP_MUL, NUM_FLT_MUL_RS);
    update_rs_exec(INT_ADD, NUM_INT_ADD_RS);
    update_rs_exec(INT_MUL, NUM_INT_MUL_RS);
    update_rs_exec(LD, NUM_LD_RS);
    update_rs_exec(SD, NUM_SD_RS);
}

void write_back()
{
    update_rs_write(FP_ADD, NUM_FLT_ADD_RS);
    update_rs_write(FP_MUL, NUM_FLT_MUL_RS);
    update_rs_write(INT_ADD, NUM_INT_ADD_RS);
    update_rs_write(INT_MUL, NUM_INT_MUL_RS);
    update_rs_write(LD, NUM_LD_RS);
    update_rs_write(SD, NUM_SD_RS);
}

void issue () {
	int i, rs_count, dst_reg;
	Instruction *curr;
	RS *rs_type, *rs;
    char * dst_reg_str;
    struct reg_status * reg = NULL;
	curr = &iq[instr_proc];

	/* Based on the opcode type, assign a common looping pointer and a counter */ 
	if (!(strcmp ("ADD", curr->opcd)) || !(strcmp ("SUB", curr->opcd))) {
		rs_count = NUM_INT_ADD_RS; rs_type = add_int_rs;
	}
	else if (!(strcmp ("ADDD", curr->opcd)) || !(strcmp ("SUBD", curr->opcd))) {
		rs_count = NUM_FLT_ADD_RS; rs_type = mul_int_rs;
	}
	else if (!(strcmp ("MUL", curr->opcd)) || !(strcmp ("DIV", curr->opcd))) {
		rs_count = NUM_INT_MUL_RS; rs_type = add_fp_rs;
	}
	else if (!(strcmp ("MULD", curr->opcd)) || !(strcmp ("DIVD", curr->opcd))) {
		rs_count = NUM_FLT_MUL_RS; rs_type = mul_fp_rs;
	}
	else if (!(strcmp ("LD", curr->opcd))) {
		rs_count = NUM_LD_RS; rs_type = ld_rs;
	}
	else if (!(strcmp ("SD", curr->opcd))) {
		rs_count = NUM_SD_RS; rs_type = sd_rs;
	}

	/* Grab the first available RS of the type selected in the previous step */
	for (i = 0; i < rs_count; i++)
		if (rs_type[i].status == AVAILABLE)
			break;

	/* If all the RS are occupied, simply return back without incrementing */ 
	/* the number of instructions executed. this causes the subsequent issue */
	/* call to process the same instruction for issuing */ 

	if (i >= rs_count) return;

	/* now, the instruction is ready to be issued. we can get started */
	rs = &rs_type[i];
	rs->status = BUSY;
	rs->instr = curr;

    dst_reg_str = &curr->dest[1];
    dst_reg = atoi(dst_reg_str);

    if(curr->type == FLOAT)
        reg = &float_reg_status[0];
    else
        reg = &int_reg_status[0];

    reg[dst_reg].rs = rs;

    // LD or ST inst
    // TODO, ST is different from LD, no dest reg ?
    if(curr->type == COMMON)
        rs->addr = atoi(curr->src1);
		rs->qj = dep_lookup (curr->src2);
        if(!rs->qj)
            rs->vj = int_reg_status[curr->src2].reg_val;
    } else {
		rs->qj = dep_lookup (curr->src1);
        rs->qk = dep_lookup (curr->src2);
        if(!rs->qj)
            rs->vj = reg[curr->src1].reg_val;
        if(!rs->qk)
            rs->vk = reg[curr->src2].reg_val;
    }

	curr->issue_time = cycles;
	instr_proc++;	
    printf("---%d\n", instr_proc);
}

bool instr_finished()
{
    static int last_cycle = 0;
    int i;
    for(i = 0; i < instr_count; i++)
        if(iq[i].write_time == 0)
            return false;

    /* let's wait last instruction finish write back */
    if(!last_cycle) {
        last_cycle = 1;
        return false;
    }

    return true;
}


int main (int argc, char **argv) {
    int i;
    Instruction *inst;

	parse_args (argc, argv);

    // parse instruction defination
	parse_file (inst_defn_file, 'd');

    // parse sample code to be simulate, queue the instructions
	parse_file (inst_trace_file, 't');

	create_arch();

	while (!instr_finished()) {

        printf("CK %d, PC %d\n", cycles, instr_proc);
        printf("#+BEGIN_SRC\n");
        for(i = 0; i < instr_count; i++) {
	        inst = &iq[i];
            if(i == instr_proc)
                printf("> %d %6s %3s %6s %6s\n", i, inst->opcd, inst->dest, inst->src1, inst->src2 ? inst->src2:"");
            else
                printf("  %d %6s %3s %6s %6s\n", i, inst->opcd, inst->dest, inst->src1, inst->src2 ? inst->src2:"");
        }

        printf("#+END_SRC\n");
        printf("** Instruction status\n");
        printf("|------------------------+---------+------------+---------|\n");
        printf("| Instruction            | Issue   | Exec comp  | Write   |\n");
        printf("|------------------------+---------+------------+---------|\n");
        for(i = 0; i < instr_proc; i++) {
            inst = &iq[i];
            printf("|%6s %3s %6s %6s| %7d | %10d | %7d |\n",
                    inst->opcd, inst->dest, inst->src1, inst->src2 ? inst->src2:"",
                    inst->issue_time, inst->exec_time, inst->write_time);
        }
        printf("|------------------------+---------+------------+---------|\n");

        printf("** Reservation station\n");
        printf("|---------+---------+----------+---------+---------+---------+---------+---------|\n");
        printf("| Name    | Busy    |   Addr.  | Op      | Vj      | Vk      | Qj      | Qk      |\n");
        printf("|---------+---------+----------+---------+---------+---------+---------+---------|\n");
        printf("|---------+---------+----------+---------+---------+---------+---------+---------|\n");

        printf("** Register result status (Qi)\n");
        printf("|");
        for(i = 0; i < NUM_INT_REGS; i++) {
            if(int_reg_status[i].rs == NULL)
                continue;
            printf(" R%d: %s |", i, int_reg_status[i].rs->name);
        }
        printf("\n");

		cycles++;

        if(instr_proc < instr_count)
		    issue ();

		execute ();
		
		write_back();
        getchar();
	}

	finish ();

	return 0;
}
