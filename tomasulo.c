#include "header.h"
#include "stdbool.h"

Operation *ops = NULL;
Instruction *iq = NULL;

int op_count = 0;
int instr_count = 0;
int instr_proc = 0;
unsigned int cycles = 0;

RS *all_rs;
RS *add_fp_rs;
RS *add_int_rs;
RS *mul_fp_rs;
RS *mul_int_rs;
RS *ld_rs;
RS *sd_rs;

static int mem[4096];
static int total_rs_num;

struct reg_status int_reg_status[NUM_INT_REGS];
struct reg_status float_reg_status[NUM_FLOAT_REGS];

//TODO. re-order buffer

void create_arch()
{
	int i;

    total_rs_num = NUM_LD_RS + NUM_SD_RS + NUM_INT_ADD_RS + NUM_INT_MUL_RS + NUM_FLT_ADD_RS + NUM_FLT_MUL_RS;

    printf("we get %d reservation station\n", total_rs_num);

    all_rs = (RS *)malloc(total_rs_num * sizeof (RS));
    memset(all_rs, 0, total_rs_num * sizeof (RS));

	if (!all_rs) {
		fatal("Error creating the architecture\n");
	}

    add_fp_rs = all_rs;
    add_int_rs = all_rs + NUM_FLT_ADD_RS;
    mul_fp_rs = add_int_rs + NUM_INT_ADD_RS;
    mul_int_rs = mul_fp_rs + NUM_FLT_MUL_RS;
    ld_rs = mul_int_rs + NUM_INT_MUL_RS;
    sd_rs = ld_rs + NUM_LD_RS;

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

bool src_is_reg(char *src)
{
    return (src[0] == 'R') || (src[0] == 'F');
}

RS *dep_lookup(char *src)
{
    int reg_idx;
    reg_idx = atoi(&src[1]);

    if(src[0] == 'R') {
        return int_reg_status[reg_idx].rs;
    } else {
        return float_reg_status[reg_idx].rs;
    }

	return NULL;
}

static RS * find_rs(int stn_type)
{
	RS *rsrv_stn = NULL;
	/*Find the reservation station requested */
	switch(stn_type){
		case FP_ADD:
            rsrv_stn = RES_STN(add,_fp);
            break;
		case FP_MUL:
            rsrv_stn = RES_STN(mul,_fp);
		    break;
		case INT_ADD:
            rsrv_stn = RES_STN(add,_int);
		    break;
		case INT_MUL:
            rsrv_stn = RES_STN(mul,_int);
		    break;
		case LD:
            rsrv_stn = RES_STN(ld,);
		    break;
		case SD:
            rsrv_stn = RES_STN(sd,);
		    break;
		default:
            fatal("Access to a non existed reservation station requested");
            break;
    }
    return rsrv_stn;
}

static void update_rs_exec(int stn_type, int no_stn)
{
	RS *rsrv_stn = find_rs(stn_type);
	int rs_no = 0;

	/*Cycle through_ Reservation Stations of the given type*/
	while(rs_no < no_stn) {
		if(rsrv_stn[rs_no].status == BUSY) {
            /* inst ready to execute ? */
			if((rsrv_stn[rs_no].qj == NULL)&&(rsrv_stn[rs_no].qk == NULL)) {
				/*if the timer has not been set ,set it to the instruction latency*/
				if(rsrv_stn[rs_no].timer == -1) {
                    /* caculate the effective address */
                    if(stn_type == LD || stn_type == SD)
                        rsrv_stn[rs_no].addr += rsrv_stn[rs_no].vj.i_val;

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
	RS *rsrv_stn = find_rs(stn_type);
	RS *rs_station, *iter_rs;
	int rs_no = 0;
    int dst_reg_idx = 0;
    int i;

	/*Cycle through the Reservation Stations of the given type*/
	while(rs_no < no_stn) {
        rs_station = &rsrv_stn[rs_no];
	    /*check if any RS is ready to move to write back stage*/
		if(rs_station->status == BUSY) {
			if((rs_station->qj == NULL)&&(rs_station->qk == NULL)) {
				/*if the functional unit has completed execution*/
				if(rs_station->timer == 0) {
					rs_station->status = RESULT_READY;
					rs_station->instr->exec_time = cycles;
				}
			}
	    /*Also check if any RS will complete write back this cycle*/
		} else if (rs_station->status == RESULT_READY) {
			/*update the write back time */
			rs_station->instr->write_time = cycles;
			
            dst_reg_idx = atoi(&rs_station->instr->dest[1]);

            /* update register file */
            switch(stn_type) {
		        case FP_ADD:
                    if(!strcmp ("ADDD", rs_station->instr->opcd))
                        float_reg_status[dst_reg_idx].reg_val.f_val = rs_station->vj.f_val +
                                                        rs_station->vk.f_val;
                    else if (!strcmp ("SUBD", rs_station->instr->opcd))
                        float_reg_status[dst_reg_idx].reg_val.f_val = rs_station->vj.f_val -
                                                        rs_station->vk.f_val;
                    else
                        printf("!! wrong instrucion\n");

                    float_reg_status[dst_reg_idx].rs = NULL;
                    break;
                case FP_MUL:
                    if(!strcmp ("MULD", rs_station->instr->opcd))
                        float_reg_status[dst_reg_idx].reg_val.f_val = rs_station->vj.f_val *
                                                        rs_station->vk.f_val;
                    else if (!strcmp ("DIVD", rs_station->instr->opcd))
                        float_reg_status[dst_reg_idx].reg_val.f_val = rs_station->vj.f_val /
                                                        rs_station->vk.f_val;
                    else
                        printf("!! wrong instrucion\n");

                    float_reg_status[dst_reg_idx].rs = NULL;
                    break;
		        case INT_ADD:
                    if(!strcmp ("ADD", rs_station->instr->opcd))
                        int_reg_status[dst_reg_idx].reg_val.i_val = rs_station->vj.i_val +
                                                        rs_station->vk.i_val;
                    else if(!strcmp ("SUB", rs_station->instr->opcd))
                        int_reg_status[dst_reg_idx].reg_val.i_val = rs_station->vj.i_val -
                                                        rs_station->vk.i_val;
                    else
                        printf("!! wrong instrucion\n");

                    int_reg_status[dst_reg_idx].rs = NULL;
                    break;
		        case INT_MUL:
                    if(!strcmp ("MUL", rs_station->instr->opcd))
                        int_reg_status[dst_reg_idx].reg_val.i_val = rs_station->vj.i_val *
                                                        rs_station->vk.i_val;
                    else if(!strcmp ("DIV", rs_station->instr->opcd))
                        int_reg_status[dst_reg_idx].reg_val.i_val = rs_station->vj.i_val /
                                                        rs_station->vk.i_val;
                    else
                        printf("!! wrong instrucion\n");

                    int_reg_status[dst_reg_idx].rs = NULL;
                    break;
                /* LD SD only support integer at this point */
                case LD:
                    int_reg_status[dst_reg_idx].reg_val.i_val = mem[rs_station->addr];
                    int_reg_status[dst_reg_idx].rs = NULL;
                    break;
                case SD:
                    //TODO is the reg value available to store ?
                    mem[rs_station->addr] = int_reg_status[dst_reg_idx].reg_val.i_val;
                    break;
                default:
                    printf("error instruction type \n");
                    break;
            }

            /* TODO, update related RS */
            for(i = 0; i < total_rs_num; i++) {
                iter_rs = all_rs + i;
                /* skip current rs station */
                if(iter_rs  == rs_station)
                    continue;

                if(iter_rs->status != BUSY)
                    continue;

                if(iter_rs->qj == rs_station) {
                    iter_rs->qj = NULL;
                    if(rs_station->instr->type == FLOAT)
                        iter_rs->vj.f_val = float_reg_status[dst_reg_idx].reg_val.f_val;
                    else if(rs_station->instr->type == INTEGER)
                        iter_rs->vj.i_val = int_reg_status[dst_reg_idx].reg_val.i_val;
                    else if(stn_type == LD)
                        iter_rs->vj.i_val = int_reg_status[dst_reg_idx].reg_val.i_val;
                }

                if(iter_rs->qk == rs_station) {
                    iter_rs->qk = NULL;
                    if(rs_station->instr->type == FLOAT)
                        iter_rs->vk.f_val = float_reg_status[dst_reg_idx].reg_val.f_val;
                    else if(rs_station->instr->type == INTEGER)
                        iter_rs->vk.i_val = int_reg_status[dst_reg_idx].reg_val.i_val;
                    else if(stn_type == LD)
                        iter_rs->vk.i_val = int_reg_status[dst_reg_idx].reg_val.i_val;
                }
            }

			/*Flush the instruction and reset the reservation station */
			rs_station->status = AVAILABLE;
			rs_station->instr = NULL;
			rs_station->timer = -1;
			rs_station->qj = rs_station->qk = NULL;
			rs_station->vj.i_val = rs_station->vk.i_val = 0;
		}
		rs_no++;
	}
}

void execute()
{
	/*Update the fields of all the reservations stations*/
    // TODO, really need to call 6 times ?
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
    int reg_idx1, reg_idx2;
	curr = &iq[instr_proc];

	/* Based on the opcode type, assign a common looping pointer and a counter */ 
	if (!(strcmp ("ADD", curr->opcd)) || !(strcmp ("SUB", curr->opcd))) {
		rs_count = NUM_INT_ADD_RS; rs_type = add_int_rs;
	}
	else if (!(strcmp ("ADDD", curr->opcd)) || !(strcmp ("SUBD", curr->opcd))) {
		rs_count = NUM_FLT_ADD_RS; rs_type = add_fp_rs;
	}
	else if (!(strcmp ("MUL", curr->opcd)) || !(strcmp ("DIV", curr->opcd))) {
		rs_count = NUM_INT_MUL_RS; rs_type = mul_int_rs;
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
	rs->timer = -1;

    dst_reg_str = &curr->dest[1];
    dst_reg = atoi(dst_reg_str);

    if(curr->type == FLOAT)
        reg = &float_reg_status[0];
    else
        reg = &int_reg_status[0];

    reg[dst_reg].rs = rs;

    reg_idx1 = atoi(&curr->src1[1]);
    reg_idx2 = atoi(&curr->src2[1]);
    // LD or ST inst
    // TODO, ST is different from LD, no dest reg ?
    if(curr->type == COMMON) {
        rs->addr = atoi(curr->src1);
		rs->qj = dep_lookup (curr->src2);
        //TODO, use int reg or float reg ?
        if(!rs->qj)
            rs->vj.i_val = int_reg_status[reg_idx2].reg_val.i_val;
    } else {
        if(src_is_reg(curr->src1)) {
		    rs->qj = dep_lookup (curr->src1);
            if(!rs->qj)
                rs->vj.i_val = reg[reg_idx1].reg_val.i_val;
        } else {
            rs->vj.i_val = atoi(curr->src1);
        }

        if(src_is_reg(curr->src2)) {
            rs->qk = dep_lookup (curr->src2);
            if(!rs->qk)
                rs->vk.i_val = reg[reg_idx2].reg_val.i_val;
        } else {
            rs->vk.i_val = atoi(curr->src2);
        }
    }

	curr->issue_time = cycles;
	instr_proc++;	
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

static void dump_rs_state(int stn_type, int no_stn)
{
	RS *rsrv_stn = find_rs(stn_type);
    RS *iter_rs;
	int rs_no = 0;

	/*Cycle through_ Reservation Stations of the given type*/
	while(rs_no < no_stn){
        iter_rs = rsrv_stn + rs_no;
        rs_no++;
        if(iter_rs->status == AVAILABLE)
            continue;
        //| Name    | Busy    |  Addr.   | Op      | Vj      | Vk      | Qj      | Qk      |
        if(iter_rs->instr->type == FLOAT) {
            printf("| %8s | %6d | %6d | %6s | %.4f | %.4f | %6s | %6s |\n",
                iter_rs->name,
                iter_rs->status != AVAILABLE,
                iter_rs->addr,
                (iter_rs->instr != NULL) ? iter_rs->instr->opcd : "-",
                iter_rs->vj.f_val,
                iter_rs->vk.f_val,
                (iter_rs->qj != NULL) ? iter_rs->qj->name : "-",
                (iter_rs->qk != NULL) ? iter_rs->qk->name : "-");
        } else {
            printf("| %8s | %6d | %6d | %6s | %6d | %6d | %6s | %6s |\n",
                iter_rs->name,
                iter_rs->status != AVAILABLE,
                iter_rs->addr,
                (iter_rs->instr != NULL) ? iter_rs->instr->opcd : "-",
                iter_rs->vj.i_val,
                iter_rs->vk.i_val,
                (iter_rs->qj != NULL) ? iter_rs->qj->name : "-",
                (iter_rs->qk != NULL) ? iter_rs->qk->name : "-");
        }
    }
}

static void dump_state()
{
    int i;
    Instruction *inst;

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
    printf("|----------+--------+--------+--------+--------+--------+--------+--------|\n");
    printf("| Name     | Busy   | Addr.  | Op     | Vj     | Vk     | Qj     | Qk     |\n");
    printf("|----------+--------+--------+--------+--------+--------+--------+--------|\n");
    dump_rs_state(FP_ADD, NUM_FLT_ADD_RS);
    dump_rs_state(FP_MUL, NUM_FLT_MUL_RS);
    dump_rs_state(INT_ADD, NUM_INT_ADD_RS);
    dump_rs_state(INT_MUL, NUM_INT_MUL_RS);
    dump_rs_state(LD, NUM_LD_RS);
    dump_rs_state(SD, NUM_SD_RS);
    printf("|----------+--------+--------+--------+--------+--------+--------+--------|\n");

    printf("** Register result status (Qi)\n");
    printf("|");
    for(i = 0; i < NUM_INT_REGS; i++) {
        if(int_reg_status[i].rs == NULL)
            continue;
        printf(" R%d: %s |", i, int_reg_status[i].rs->name);
    }
    printf("\n");
    printf("|");
    for(i = 0; i < NUM_FLOAT_REGS; i++) {
        if(float_reg_status[i].rs == NULL)
            continue;
        printf(" F%d: %s |", i, float_reg_status[i].rs->name);
    }
    printf("\n");

    printf("** Register file\n");
    printf("|");
    for(i = 0; i < NUM_INT_REGS; i++) {
        if(int_reg_status[i].reg_val.i_val != 0)
            printf(" R%d: %d |", i, int_reg_status[i].reg_val.i_val);
    }
    printf("\n");
    printf("|");
    for(i = 0; i < NUM_FLOAT_REGS; i++) {
        if(float_reg_status[i].reg_val.f_val != 0)
            printf(" F%d: %f |", i, float_reg_status[i].reg_val.f_val);
    }
    printf("\n");
}

void init_machine_state()
{
    int_reg_status[0].reg_val.i_val = 1;
    int_reg_status[4].reg_val.i_val = 999;

    float_reg_status[2].reg_val.f_val = 0.2;
    float_reg_status[3].reg_val.f_val = 0.8;
    float_reg_status[4].reg_val.f_val = 2.7;
}

int main (int argc, char **argv) {
	parse_args (argc, argv);

    // parse instruction defination
	parse_file (inst_defn_file, 'd');

    // parse sample code to be simulate, queue the instructions
	parse_file (inst_trace_file, 't');

	create_arch();

    init_machine_state();

	while (!instr_finished()) {

        dump_state();

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
