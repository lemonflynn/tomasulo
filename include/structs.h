
enum instr_type { UNDEFINED, INTEGER, FLOAT , COMMON };

enum rs_status { UNKNOWN, AVAILABLE, BUSY, RESULT_READY };

typedef struct {
	char *name;
	int num_ops;
	int latency;
	enum instr_type type;
} Operation;

typedef struct {
	char *opcd;
	char *dest;
	char *src1;
	char *src2;
	int num_ops;
	int latency;
	struct resrv_stn *cur;
	unsigned int issue_time;
	unsigned int exec_time;
	unsigned int write_time;
	enum instr_type type;
} Instruction;

struct resrv_stn {
    char name[32];
	enum rs_status status;
	Instruction *instr;
	int qj;
	int qk;
    union {
        int i_val;
        float f_val;
    } vj;
    union {
        int i_val;
        float f_val;
    } vk;
    int addr;
	int timer;
    int dst_rob;
};

typedef struct resrv_stn RS;

struct reg_status{
    int rob;
    union {
        int i_val;
        float f_val;
    } reg_val;
};

struct ROB {
    Instruction *instr;
    enum rs_status status;
    union {
        int i_val;
        float f_val;
    } val;
};

