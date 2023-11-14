
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
	struct resrv_stn *qj;
	struct resrv_stn *qk;
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
};

struct reg_status{
    RS * rs;
    union {
        int i_val;
        float f_val;
    } reg_val;
};

typedef struct resrv_stn RS;
