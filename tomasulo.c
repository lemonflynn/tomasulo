#include "header.h"
#include "stdbool.h"

#define ROB_NUM 5

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
RS *beq_rs;

static int mem[4096];
static int total_rs_num;

struct reg_status int_reg_status[NUM_INT_REGS];
struct reg_status float_reg_status[NUM_FLOAT_REGS];

//TODO. re-order buffer, is actually a circular buffer
struct ROB * reorder_buffer;
/*
head is the first busy instr, thus, first to complete,
tail is the last busy instr, new instruction would insert
bebind this.
*/
int rob_head, rob_tail;
int rob_flip;

char status_str[6][16] = {
    "unknow",
    "available",
    "busy",
    "result ready",
};

bool rob_empty()
{
    return (rob_flip == 0) && (rob_head == rob_tail);
}

bool rob_full()
{
    return (rob_flip == 1) && (rob_head == rob_tail);
}

void create_arch()
{
	int i;
	RS *iter_rs;

    total_rs_num = NUM_BEQ_RS + NUM_LD_RS + NUM_SD_RS + NUM_INT_ADD_RS + NUM_INT_MUL_RS + NUM_FLT_ADD_RS + NUM_FLT_MUL_RS;

    printf("we get %d reservation station\n", total_rs_num);

    all_rs = (RS *)malloc(total_rs_num * sizeof (RS));
    memset(all_rs, 0, total_rs_num * sizeof (RS));

    reorder_buffer = (struct ROB *) malloc(ROB_NUM * sizeof(struct ROB));
    memset(reorder_buffer, 0, ROB_NUM * sizeof(struct ROB));
    for(i = 0; i < ROB_NUM; i++)
        reorder_buffer[i].status = AVAILABLE;
    rob_head = rob_tail = 0;
    rob_flip = 0;

	if (!all_rs) {
		fatal("Error creating the architecture\n");
	}

    add_fp_rs = all_rs;
    add_int_rs = all_rs + NUM_FLT_ADD_RS;
    mul_fp_rs = add_int_rs + NUM_INT_ADD_RS;
    mul_int_rs = mul_fp_rs + NUM_FLT_MUL_RS;
    ld_rs = mul_int_rs + NUM_INT_MUL_RS;
    sd_rs = ld_rs + NUM_LD_RS;
    beq_rs = sd_rs + NUM_SD_RS;

    memset(int_reg_status, 0, sizeof(struct reg_status) * NUM_INT_REGS);
    memset(float_reg_status, 0, sizeof(struct reg_status) * NUM_FLOAT_REGS);

	for (i = 0; i < NUM_INT_REGS; i++)
        int_reg_status[i].rob = -1;

	for (i = 0; i < NUM_FLOAT_REGS; i++)
        float_reg_status[i].rob = -1;

    memset(mem, 0, sizeof(mem));

	for (i = 0; i < total_rs_num; i++) {
        iter_rs = all_rs + i;
        iter_rs->status = AVAILABLE;
    }

	for (i = 0; i < MAX_RS; i++) {
		if (i < NUM_FLT_ADD_RS)
            sprintf(add_fp_rs[i].name, "FLT_ADD%d", i);

		if (i < NUM_INT_ADD_RS)
            sprintf(add_int_rs[i].name, "INT_ADD%d", i);

		if (i < NUM_FLT_MUL_RS)
            sprintf(mul_fp_rs[i].name, "FLT_MUL%d", i);

		if (i < NUM_INT_MUL_RS)
            sprintf(mul_int_rs[i].name, "INT_MUL%d", i);

		if (i < NUM_LD_RS)
            sprintf(ld_rs[i].name, "LD%d", i);

		if (i < NUM_SD_RS)
            sprintf(sd_rs[i].name, "SD%d", i);

		if (i < NUM_BEQ_RS)
            sprintf(beq_rs[i].name, "B%d", i);
	}
}

bool src_is_reg(char *src)
{
    return (src[0] == 'R') || (src[0] == 'F');
}

int dep_lookup(char *src)
{
    int reg_idx;
    reg_idx = atoi(&src[1]);

    if(src[0] == 'R') {
        return int_reg_status[reg_idx].rob;
    } else {
        return float_reg_status[reg_idx].rob;
    }

	return -1;
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
		case BEQ:
            rsrv_stn = RES_STN(beq,);
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
			if((rsrv_stn[rs_no].qj == -1)&&(rsrv_stn[rs_no].qk == -1)) {
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
    struct ROB *rob_entry;
	int rs_no = 0;
    int i;

	/*Cycle through the Reservation Stations of the given type*/
	while(rs_no < no_stn) {
        rs_station = &rsrv_stn[rs_no];
	    /*check if any RS is ready to move to write back stage*/
		if(rs_station->status == BUSY) {
			if((rs_station->qj == -1)&&(rs_station->qk == -1)) {
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
			
            rob_entry = &reorder_buffer[rs_station->dst_rob];

            /* write to reorder buffer */
            switch(stn_type) {
		        case FP_ADD:
                    if(!strcmp ("ADDD", rs_station->instr->opcd))
                        rob_entry->val.f_val = rs_station->vj.f_val +
                                                        rs_station->vk.f_val;
                    else if (!strcmp ("SUBD", rs_station->instr->opcd))
                        rob_entry->val.f_val = rs_station->vj.f_val -
                                                        rs_station->vk.f_val;
                    else
                        printf("!! wrong instrucion\n");

                    break;
                case FP_MUL:
                    if(!strcmp ("MULD", rs_station->instr->opcd))
                        rob_entry->val.f_val = rs_station->vj.f_val *
                                                        rs_station->vk.f_val;
                    else if (!strcmp ("DIVD", rs_station->instr->opcd))
                        rob_entry->val.f_val = rs_station->vj.f_val /
                                                        rs_station->vk.f_val;
                    else
                        printf("!! wrong instrucion\n");

                    break;
		        case INT_ADD:
                    if(!strcmp ("ADD", rs_station->instr->opcd))
                        rob_entry->val.i_val = rs_station->vj.i_val +
                                                        rs_station->vk.i_val;
                    else if(!strcmp ("SUB", rs_station->instr->opcd))
                        rob_entry->val.i_val = rs_station->vj.i_val -
                                                        rs_station->vk.i_val;
                    else
                        printf("!! wrong instrucion\n");

                    break;
		        case INT_MUL:
                    if(!strcmp ("MUL", rs_station->instr->opcd))
                        rob_entry->val.i_val = rs_station->vj.i_val *
                                                        rs_station->vk.i_val;
                    else if(!strcmp ("DIV", rs_station->instr->opcd))
                        rob_entry->val.i_val = rs_station->vj.i_val /
                                                        rs_station->vk.i_val;
                    else
                        printf("!! wrong instrucion\n");

                    break;
                /* LD SD only support integer at this point */
                case LD:
                    rob_entry->val.i_val = mem[rs_station->addr];
                    break;
                case SD:
                    //TODO is the reg value available to store ? fix for ROB
                    rob_entry->addr = rs_station->addr;
                    //TODO is it safe to set f_val only ?
                    rob_entry->val.f_val = rs_station->vk.f_val;
                    break;
                case BEQ:
                    /* 1 means branch taken */
                    if(rs_station->vj.i_val == rs_station->vk.i_val)
                        rob_entry->val.i_val = 1;
                    else
                        rob_entry->val.i_val = 0;
                    break;
                default:
                    printf("error instruction type \n");
                    break;
            }

            rob_entry->status = RESULT_READY;

            /* TODO, update related RS */
            for(i = 0; i < total_rs_num; i++) {
                iter_rs = all_rs + i;
                /* skip current rs station */
                if(iter_rs == rs_station)
                    continue;

                if(iter_rs->status != BUSY)
                    continue;

                if(iter_rs->qj == rs_station->dst_rob) {
                    iter_rs->qj = -1;
                    if(rs_station->instr->type == FLOAT)
                        iter_rs->vj.f_val = rob_entry->val.f_val;
                    else if(rs_station->instr->type == INTEGER)
                        iter_rs->vj.i_val = rob_entry->val.i_val;
                    else if(stn_type == LD)
                        iter_rs->vj.i_val = rob_entry->val.i_val;
                }

                if(iter_rs->qk == rs_station->dst_rob) {
                    iter_rs->qk = -1;
                    if(rs_station->instr->type == FLOAT)
                        iter_rs->vk.f_val = rob_entry->val.f_val;
                    else if(rs_station->instr->type == INTEGER)
                        iter_rs->vk.i_val = rob_entry->val.i_val;
                    else if(stn_type == LD)
                        iter_rs->vk.i_val = rob_entry->val.i_val;
                    else if(stn_type == SD)
                        iter_rs->vk.f_val = rob_entry->val.f_val;
                }
            }

			/*Flush the instruction and reset the reservation station */
			rs_station->status = AVAILABLE;
			rs_station->instr = NULL;
			rs_station->timer = -1;
			rs_station->qj = rs_station->qk = -1;
			rs_station->dst_rob = -1;
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
    update_rs_exec(BEQ, NUM_BEQ_RS);
}

void write_back()
{
    update_rs_write(FP_ADD, NUM_FLT_ADD_RS);
    update_rs_write(FP_MUL, NUM_FLT_MUL_RS);
    update_rs_write(INT_ADD, NUM_INT_ADD_RS);
    update_rs_write(INT_MUL, NUM_INT_MUL_RS);
    update_rs_write(LD, NUM_LD_RS);
    update_rs_write(SD, NUM_SD_RS);
    update_rs_write(BEQ, NUM_BEQ_RS);
}

void issue () {
	int i, rs_count, dst_reg;
	Instruction *curr;
	RS *rs_type, *rs;
    char * dst_reg_str;
    struct reg_status * reg = NULL;
    int reg_idx0, reg_idx1, reg_idx2;
	curr = &iq[instr_proc];

	/* Based on the opcode type, assign a common looping pointer and a counter */ 
	if (!(strcmp ("ADD", curr->opcd)) || !(strcmp ("SUB", curr->opcd))) {
		rs_count = NUM_INT_ADD_RS; rs_type = add_int_rs;
	} else if (!(strcmp ("ADDD", curr->opcd)) || !(strcmp ("SUBD", curr->opcd))) {
		rs_count = NUM_FLT_ADD_RS; rs_type = add_fp_rs;
	} else if (!(strcmp ("MUL", curr->opcd)) || !(strcmp ("DIV", curr->opcd))) {
		rs_count = NUM_INT_MUL_RS; rs_type = mul_int_rs;
	} else if (!(strcmp ("MULD", curr->opcd)) || !(strcmp ("DIVD", curr->opcd))) {
		rs_count = NUM_FLT_MUL_RS; rs_type = mul_fp_rs;
	} else if (!(strcmp ("LD", curr->opcd))) {
		rs_count = NUM_LD_RS; rs_type = ld_rs;
	} else if (!(strcmp ("SD", curr->opcd))) {
		rs_count = NUM_SD_RS; rs_type = sd_rs;
	} else if (!(strcmp ("BEQ", curr->opcd))) {
		rs_count = NUM_BEQ_RS; rs_type = beq_rs;
    } else {
        printf("error: unknow opcode %s\n", curr->opcd);
    }

	/* Grab the first available RS of the type selected in the previous step */
	for (i = 0; i < rs_count; i++)
		if (rs_type[i].status == AVAILABLE)
			break;

	/* If all the RS are occupied, simply return back without incrementing */ 
	/* the number of instructions executed. this causes the subsequent issue */
	/* call to process the same instruction for issuing */ 

	if (i >= rs_count || rob_full()) {
        printf("structure hazard !\n");
        return;
    }

    dst_reg_str = &curr->dest[1];
    dst_reg = atoi(dst_reg_str);

    if(curr->type == FLOAT)
        reg = &float_reg_status[0];
    else
        reg = &int_reg_status[0];

    /* BEQ and SD instruction will not modify dst_reg */
    if(!strcmp(curr->opcd, "SD") ||
       !strcmp(curr->opcd, "BEQ")) {
        reg[dst_reg].rob = reg[dst_reg].rob;
    } else {
        reg[dst_reg].rob = rob_tail;
    }

	/* now, the instruction is ready to be issued. we can get started */
	rs = &rs_type[i];
	rs->status = BUSY;
	rs->instr = curr;
	rs->timer = -1;
    rs->qj = rs->qk = -1;

    rs->dst_rob = rob_tail;
    reorder_buffer[rob_tail].instr = curr;
    reorder_buffer[rob_tail].status = BUSY;
    reorder_buffer[rob_tail].val.i_val = 0;

    // store pc in addr
    if(!strcmp(curr->opcd, "BEQ"))
        reorder_buffer[rob_tail].addr = atoi(curr->src2);

    if(rob_tail == ROB_NUM - 1) {
        /* let's flip this flag */
        rob_flip ^= 1;
        rob_tail = 0;
    } else {
        rob_tail++;
    }

    reg_idx0 = atoi(&curr->dest[1]);
    reg_idx1 = atoi(&curr->src1[1]);
    reg_idx2 = atoi(&curr->src2[1]);
    // LD or ST inst
    // TODO, ST is different from LD, no dest reg ?
    if(curr->type == COMMON) {
        rs->addr = atoi(curr->src1);
		rs->qj = dep_lookup (curr->src2);
        //TODO, use int reg or float reg ?
        if(rs->qj == -1)
            rs->vj.i_val = int_reg_status[reg_idx2].reg_val.i_val;

        if(!strcmp(curr->opcd, "SD")){
            /* this is actually source reg for store instruction */
            rs->qk = dep_lookup(curr->dest);
            if(rs->qk == -1) {
                if(curr->dest[0] == 'R')
                    rs->vk.i_val = int_reg_status[reg_idx0].reg_val.i_val;
                else
                    rs->vk.f_val = float_reg_status[reg_idx0].reg_val.f_val;
            }
        }

        /* let's predict branch not taken */
        if(!strcmp(curr->opcd, "BEQ")){
            rs->qj = dep_lookup(curr->dest);
            if(rs->qj == -1)
                rs->vj.i_val = reg[reg_idx0].reg_val.i_val;

            rs->qk = dep_lookup(curr->src1);
            if(rs->qk == -1)
                rs->vk.i_val = reg[reg_idx1].reg_val.i_val;
        }
    } else {
        if(src_is_reg(curr->src1)) {
		    rs->qj = dep_lookup(curr->src1);
            if(rs->qj == -1)
                rs->vj.i_val = reg[reg_idx1].reg_val.i_val;
        } else {
            rs->vj.i_val = atoi(curr->src1);
        }

        if(src_is_reg(curr->src2)) {
            rs->qk = dep_lookup (curr->src2);
            if(rs->qk == -1)
                rs->vk.i_val = reg[reg_idx2].reg_val.i_val;
        } else {
            rs->vk.i_val = atoi(curr->src2);
        }
    }

	curr->issue_time = cycles;
	instr_proc++;	
}

void commit()
{
    int i, dst_reg_idx = 0;
    struct ROB *rob_entry;
    RS * iter_rs;

    /* no instr to commit */
    if(rob_empty())
        return;

    rob_entry = &reorder_buffer[rob_head];
    /* first instr not ready to graduate */
    if(rob_entry->status != RESULT_READY)
        return;

    /* update arch state */
    dst_reg_idx = atoi(&rob_entry->instr->dest[1]);
    if(rob_entry->instr->type == FLOAT) {
        float_reg_status[dst_reg_idx].reg_val.f_val = rob_entry->val.f_val;
        float_reg_status[dst_reg_idx].rob = -1;
        printf("F%d %f\n", dst_reg_idx, rob_entry->val.f_val);
    } else if(rob_entry->instr->type == INTEGER) {
        int_reg_status[dst_reg_idx].reg_val.i_val = rob_entry->val.i_val;
        int_reg_status[dst_reg_idx].rob = -1;
        printf("R%d %d\n", dst_reg_idx, rob_entry->val.i_val);
    } else {
        if(!strcmp(rob_entry->instr->opcd, "SD")) {
            mem[rob_entry->addr] = rob_entry->val.f_val;
        } else if(!strcmp(rob_entry->instr->opcd, "BEQ")) {
            /* branch is taken, let's flush rob and jump */
            if(rob_entry->val.i_val) {
                printf("branch taken, flush rs and rob! jump to inst %d\n", rob_entry->addr);
                instr_proc = rob_entry->addr;

                for (i = 0; i < total_rs_num; i++) {
                    iter_rs = all_rs + i;
                    iter_rs->status = AVAILABLE;
                }

                memset(reorder_buffer, 0, ROB_NUM * sizeof(struct ROB));
                for(i = 0; i < ROB_NUM; i++)
                    reorder_buffer[i].status = AVAILABLE;
                rob_head = rob_tail = 0;
                rob_flip = 0;

                return;
            }
        } else {
            int_reg_status[dst_reg_idx].reg_val.i_val = rob_entry->val.i_val;
            int_reg_status[dst_reg_idx].rob = -1;
            printf("R%d %d\n", dst_reg_idx, rob_entry->val.i_val);
            printf("what to do at this point?\n");
        }
    }

    rob_entry->instr = NULL;
    rob_entry->status = AVAILABLE;
    rob_entry->val.i_val = 0;

    if(rob_head == ROB_NUM - 1) {
        /* let's flip this flag */
        rob_flip ^= 1;
        rob_head = 0;
    } else {
        rob_head++;
    }
}

bool instr_finished()
{
    int i;
    for(i = 0; i < instr_count; i++)
        if(iq[i].write_time == 0)
            return false;

    return rob_empty();
}

static void dump_rs_state()
{
    RS *iter_rs;
	int rs_no = 0;

	/*Cycle through_ Reservation Stations of the given type*/
	while(rs_no < total_rs_num){
        iter_rs = all_rs + rs_no;
        rs_no++;
        if(iter_rs->status == AVAILABLE)
            continue;
        //| Name    | Busy    |  Addr.   | Op      | Vj      | Vk      | Qj      | Qk      |
        if(iter_rs->instr->type == FLOAT) {
            printf("| %8s | %6d | %6d | %6s | %.4f | %.4f | %2d | %2d | %3d |\n",
                iter_rs->name,
                iter_rs->status != AVAILABLE,
                iter_rs->addr,
                iter_rs->instr->opcd,
                iter_rs->vj.f_val,
                iter_rs->vk.f_val,
                iter_rs->qj,
                iter_rs->qk,
                iter_rs->dst_rob);
        } else if (iter_rs->instr->type == INTEGER){
            printf("| %8s | %6d | %6d | %6s | %6d | %6d | %2d | %2d | %3d |\n",
                iter_rs->name,
                iter_rs->status != AVAILABLE,
                iter_rs->addr,
                iter_rs->instr->opcd,
                iter_rs->vj.i_val,
                iter_rs->vk.i_val,
                iter_rs->qj,
                iter_rs->qk,
                iter_rs->dst_rob);
        } else if(!strcmp(iter_rs->instr->opcd, "SD")) {
            printf("| %8s | %6d | %6d | %6s | %6d | %.4f | %2d | %2d | %3d |\n",
                iter_rs->name,
                iter_rs->status != AVAILABLE,
                iter_rs->addr,
                iter_rs->instr->opcd,
                iter_rs->vj.i_val,
                iter_rs->vk.f_val,
                iter_rs->qj,
                iter_rs->qk,
                iter_rs->dst_rob);
        } else if(!strcmp(iter_rs->instr->opcd, "BEQ")) {
            printf("| %8s | %6d | %6d | %6s | %6d | %6d | %2d | %2d | %3d |\n",
                iter_rs->name,
                iter_rs->status != AVAILABLE,
                iter_rs->addr,
                iter_rs->instr->opcd,
                iter_rs->vj.i_val,
                iter_rs->vk.i_val,
                iter_rs->qj,
                iter_rs->qk,
                iter_rs->dst_rob);
        }
    }
}

static void dump_state()
{
    int i;
    Instruction *inst;
    struct ROB* rob_entry;

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

    printf("** Reorder buffer \n");
    printf("|----------+--------+--------+--------+--------+--------+---------|\n");
    printf("| Entry | Busy |     Instruction    |     state    | Dest | Value |\n");
    for(i = 0; i < ROB_NUM; i++) {
        rob_entry = &reorder_buffer[i];
        if(rob_entry->status == AVAILABLE) {
            printf("| %5d |  No  |                    |              |      |       |\n", i);
            continue;
        }

        printf("| %5d | Yes  | %4s %3s %4s %4s | %12s |",
            i,
            rob_entry->instr->opcd,
            rob_entry->instr->dest,
            rob_entry->instr->src1,
            rob_entry->instr->src2 ? rob_entry->instr->src2:"",
            status_str[rob_entry->status]);

        if(!strcmp(rob_entry->instr->opcd, "SD") ||
            !strcmp(rob_entry->instr->opcd, "BEQ"))
            printf(" %3d |", rob_entry->addr);
        else
            printf("  %s  |", rob_entry->instr->dest);

        if(rob_entry->status != RESULT_READY)
            printf("   -   |\n");
        else {
            if(rob_entry->instr->type == INTEGER)
                printf("%5d  |\n", rob_entry->val.i_val);
            else
                printf(" %.3f |\n", rob_entry->val.f_val);
        }
    }
    printf("|----------+--------+--------+--------+--------+--------+---------|\n");

    printf("** Reservation station\n");
    printf("|----------+--------+--------+--------+--------+--------+----+----+-----|\n");
    printf("| Name     | Busy   | Addr.  | Op     | Vj     | Vk     | Qj | Qk | Dst |\n");
    printf("|----------+--------+--------+--------+--------+--------+----+----+-----|\n");
    dump_rs_state();
    printf("|----------+--------+--------+--------+--------+--------+----+----+-----|\n");

    printf("** Register result status (Qi)\n");
    printf("|");
    for(i = 0; i < NUM_INT_REGS; i++) {
        if(int_reg_status[i].rob == -1)
            continue;
        printf(" R%d: %d |", i, int_reg_status[i].rob);
    }
    printf("\n");
    printf("|");
    for(i = 0; i < NUM_FLOAT_REGS; i++) {
        if(float_reg_status[i].rob == -1)
            continue;
        printf(" F%d: %d |", i, float_reg_status[i].rob);
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
    int_reg_status[3].reg_val.i_val = 234;
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

        commit();

        getchar();
	}

	finish ();

	return 0;
}
