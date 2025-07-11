#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "ctd.h"
#include "arena.c"




typedef enum Ex_Type Ex_Type;
enum Ex_Type {
    TYPE_EX,
    TYPE_ADD1,
    TYPE_ADD2,
    TYPE_MUL1,
    TYPE_MUL2,
    TYPE_MUL3,
    TYPE_MUL4,
    TYPE_MUL5,
    TYPE_MUL6,
    TYPE_MUL7,
    TYPE_MUL8,
    TYPE_MUL9,
    TYPE_MUL10,

    TYPE_DIV1,
    TYPE_DIV2,
    TYPE_DIV3,
    TYPE_DIV4,
    TYPE_DIV5,
    TYPE_DIV6,
    TYPE_DIV7,
    TYPE_DIV8,
    TYPE_DIV9,
    TYPE_DIV10,
    TYPE_DIV11,
    TYPE_DIV12,
    TYPE_DIV13,
    TYPE_DIV14,
    TYPE_DIV15,
    TYPE_DIV16,
    TYPE_DIV17,
    TYPE_DIV18,
    TYPE_DIV19,
    TYPE_DIV20,
    TYPE_DIV21,
    TYPE_DIV22,
    TYPE_DIV23,
    TYPE_DIV24,
    TYPE_DIV25,
    TYPE_DIV26,
    TYPE_DIV27,
    TYPE_DIV28,
    TYPE_DIV29,
    TYPE_DIV30,
    TYPE_DIV31,
    TYPE_DIV32,
    TYPE_DIV33,
    TYPE_DIV34,
    TYPE_DIV35,
    TYPE_DIV36,
    TYPE_DIV37,
    TYPE_DIV38,
    TYPE_DIV39,
    TYPE_DIV40,
};

typedef enum Stage Stage;
enum Stage {
    IF,
    ID,
    EX,
    MEM,
    WB,
    FIN,
    INACTIVE,
    STALL,
};


typedef struct Fp_Reg Fp_Reg;
struct Fp_Reg {
    float val;
    bool  read;
    bool  write;
    int   i_queue_row;
};

typedef struct I_Reg I_Reg;
struct I_Reg {
    int  val;
    bool read;
    bool write;
    int  i_queue_row;
};



#define I_REGS 32
#define FP_REGS 32
typedef struct Registers Registers;
struct Registers {
    I_Reg i[I_REGS];
    Fp_Reg  fp[FP_REGS];
};


typedef struct Block Block;
struct Block {
    int mem_address;
    int   val;
    bool recent;
};

#define CACHE_SET 8
#define CACHE_WAY 2
#define CACHE_MISS_COST 3
typedef struct Cache Cache;
struct Cache {
    Block data[CACHE_SET][CACHE_WAY];
    int row;
    int col;
};


typedef struct Label_Info Label_Info;
struct Label_Info {
    char *str;
    int row;
};


#define LABEL_INFO_ARRAY_CAP 32
typedef struct Label_Info_Array Label_Info_Array;
struct Label_Info_Array {
    Label_Info *data;
    int size;
    int cap;
};

typedef enum Word_Type Word_Type;
enum Word_Type {

    LOAD_IMM,
    LOAD_D,
    LOAD_W,
    STORE_D,
    STORE_W,

    ADD,
    ADD_D,
    ADD_IMM,
    SUB_D,
    SUB,
    MUL_D,
    DIV_D,

    UNCONDITIONAL_JUMP,
    BNE,
    BEQ,

    LABEL,
    JUMP,
    I_REG,
    F_REG,
    OFFSET,
    I_NUM,
    F_NUM,

    EMPTY,
};


typedef struct Word Word;
struct Word {
    Word_Type type;
    char *str;

    union {
        float f_arg[2];
        int   i_arg[2];
    };
};

typedef struct Words Words;
struct Words {
    Word words[5];
    char *instr_str;
    Stage wanted_stage;
    Ex_Type ex_type;
    Stage current_stage;
    int data;
    int table_row;
    bool processed;
    bool stall;
    int row;

};

int     original_instruction_count;
Words * original_instruction_lines;

typedef struct Stages Stages;
struct Stages {
    Stage stage;
    bool active;
};

typedef struct I_Queue I_Queue;
struct I_Queue {
    int size;
    int cap;
    Words *lines;
    int row;
    int processed_lines;
};

I_Queue *create_i_queue(Arena *arena, Words *lines, int line_size, int cap) {
    I_Queue *queue = push_struct(arena->head, I_Queue);
    queue->size = line_size;
    queue->cap = cap;
    queue->lines = push_array(arena->head, Words, cap);
    for (int i = 0; i <line_size; ++i) {
        queue->lines[i] = lines[i];
    }
    for (int i = line_size; i <cap; ++i) {
        queue->lines[i].table_row = -1;
    }
    return queue;
}

void insert_i_queue(Arena *arena, I_Queue **queue, Words *line) {
    if ((*queue)->size == (*queue)->cap) {
        (*queue) =  create_i_queue(arena, (*queue)->lines, (*queue)->size,  (*queue)->size *  2);
    }
    (*queue)->lines[(*queue)->size] = *line;
    ++(*queue)->size;
}

#define I_QUEUE_DEF_CAP 64
#define BACKUP_COUNT 4
typedef struct Backup_Data Backup_Data;
struct Backup_Data {
    Registers regs;
    I_Reg mem[19];
    bool active;
    int restore_pc;
    int i_queue_size;
    int i_queue_row;
};

typedef struct Cpu Cpu;
struct Cpu {

    Backup_Data backup_data[BACKUP_COUNT];
    bool curr_stage_active[5];
    bool next_stage_active[5];

    Registers regs;
    Label_Info_Array labels;
    Cache l1;
    int address_count;
    I_Reg mem[19];
    int  pc;

    int processed_lines;
    int total_lines;
    int cycles;

    int fp_add_sub_cycles;
    int fp_mull_cycles;
    int fp_div_cycles;

    int int_cycles;

    bool *predict_taken;

};

void init_cpu(  Arena *arena, Cpu *cpu) {


    cpu->curr_stage_active[0] = false;
    cpu->curr_stage_active[1] = false;
    cpu->curr_stage_active[2] = true;
    cpu->curr_stage_active[3] = false;
    cpu->curr_stage_active[4] = false;


    cpu->next_stage_active[0] = false;
    cpu->next_stage_active[1] = false;
    cpu->next_stage_active[2] = true;
    cpu->next_stage_active[3] = false;
    cpu->next_stage_active[4] = false;

    //PIPELINE Cycles
    //Pipelined FP adder for FP adds and subtracts -2cycles
    //Pipelined FP multiplier for FP multiplies -10cycles
    //Pipelined FP divider for FP divides -40cycles
    //Integer Unit/s for Loads, Stores, Integer Adds and Subtracts -1cycle
    //32 FP registers and 32 integer registers. 
    //All registers will be initialized to 0 at the beginning.
    //Full forwarding is possible and it is possible to write and read the register in the same cycle.


    for (int i = 0;  i <CACHE_SET; ++i) {
        for (int j = 0; j <CACHE_WAY; ++j) {
            cpu->l1.data[i][j].recent = false;
            cpu->l1.data[i][j].mem_address = -1;
        }
    }

    cpu->fp_add_sub_cycles  = 2;
    cpu->fp_mull_cycles = 10;
    cpu->fp_div_cycles  = 40;
    cpu->int_cycles     = 1;

    for (int i = 0; i < I_REGS; ++i) {
        cpu->regs.i[i].val = 0;
        cpu->regs.i[i].read = true;
        cpu->regs.i[i].write = true;
        cpu->regs.i[i].i_queue_row = -1;
    }
    for (int i = 0; i < FP_REGS; ++i) {
        cpu->regs.fp[i].val = 0;
        cpu->regs.fp[i].read = true;
        cpu->regs.fp[i].write = true;
        cpu->regs.fp[i].i_queue_row = -1;
    }

    cpu->labels.size =0 ;
    cpu->labels.cap =LABEL_INFO_ARRAY_CAP;
    cpu->labels.data = push_array(arena->head, Label_Info, cpu->labels.cap);

    cpu->pc = 0;
    cpu->address_count = 19;
    cpu->mem[0].val  = 45;
    cpu->mem[1].val  = 12;
    cpu->mem[2].val  = 0;
    cpu->mem[3].val  = 92;
    cpu->mem[4].val  = 10;
    cpu->mem[5].val  = 135;
    cpu->mem[6].val  = 254;
    cpu->mem[7].val  = 127;
    cpu->mem[8].val  = 18;
    cpu->mem[9].val  = 4;
    cpu->mem[10].val = 55;
    cpu->mem[11].val = 8;
    cpu->mem[12].val = 2;
    cpu->mem[13].val = 98;
    cpu->mem[14].val = 13;
    cpu->mem[15].val = 5;
    cpu->mem[16].val = 233;
    cpu->mem[17].val = 158;
    cpu->mem[18].val = 167;
    for (int i = 0; i <cpu->address_count; ++i) {
        cpu->mem[i].write = true;
        cpu->mem[i].read = true;
        cpu->mem[i].i_queue_row = -1;
    }
}






void insert_label( Arena *arena, Label_Info_Array **array, char *str, int row) {
    if ((*array)->size +1 > (*array)->cap) {
        Label_Info_Array *new_array = push_struct(arena->head, Label_Info_Array);
        new_array->data             = push_array(arena->head, Label_Info, (*array)->cap * 2);
        new_array->cap  = (*array)->cap * 2;
        new_array->size = (*array)->size;
        for (int i = 0; i <(*array)->size; ++i) {
            new_array->data[i] = (*array)->data[i];
        }
        (*array) = new_array;
    }
    (*array)->data[(*array)->size].str   = str;
    (*array)->data[(*array)->size].row   = row;
    ++(*array)->size;
}





#define FILE_INFO_ROW_CAP 32
typedef struct Mip_File_Info Mip_File_Info;
struct Mip_File_Info {
    Words* line;
    int rows;
    Label_Info *label_info;
};


void init_file_info(Mip_File_Info *file_info, int row_cap, Arena *arena) {
    file_info->line = insert_arena(arena->head, (sizeof(Words)  * row_cap));
    file_info->rows = row_cap;
    for (int i = 0; i <row_cap; ++i) {
        for (int j = 0; j <5; ++j) {
            file_info->line[i].wanted_stage = IF;
            file_info->line[i].current_stage = INACTIVE;
        }
    }
}


void copy_str(char *dest, char * src, int len) {
    for (int i = 0; i <len; ++i) {
        dest[i] = src[i];
    }
}

void copy_str_pad(char *dest, char * src, int len, char pad) {
    int copy_len = strlen(src);
    if (copy_len > len) copy_len = len;
    for (int i = 0; i <copy_len; ++i) {
        if (src[i] == -62 || src[i] == -96) {
            dest[i] = ' ';
        } else dest[i] = src[i];
    }
    for (int i = copy_len; i <len; ++i) {
        dest[i] = pad;
    }
}


void get_file_info(  Arena *arena, Arena *scratch, Cpu *cpu, Mip_File_Info *file_info, char *name) {
    Label_Info_Array * label_array = &cpu->labels;
    FILE *file = fopen(name, "r");
    fseek(file, 0, SEEK_END);
    int file_size = ftell(file) + 1;
    fseek(file, 0, SEEK_SET);
    char *buff = push_array(scratch->head, char, file_size);
    int rows_parsed = 0;
    while (fgets(buff, file_size, file) != 0) {
        Words *line = &file_info->line[rows_parsed];

        int fget_line_len = 0;
        while (1) {
            if (buff[fget_line_len] == '\n' || buff[fget_line_len] == '\0' || buff[fget_line_len] == EOF) break;
            ++fget_line_len;
        }
        if (!fget_line_len) break;
        char *instr_str = push_array(arena->head, char*, fget_line_len);
        copy_str(instr_str, buff, fget_line_len);
        line->instr_str = instr_str;
        line->row = rows_parsed;


        int word_index = 0;
        int buff_index = 0;
        while (  buff[buff_index] == -96  || buff[buff_index] == -62 || buff[buff_index] == ' ' || buff[buff_index] == '\t') ++buff_index;
        int process_index = buff_index;

        while (buff[buff_index] != ' ' && buff[buff_index] != '\n' && buff[buff_index] != -96  && buff[buff_index] != -62  && buff[buff_index] != '\0' && buff[buff_index] != '\t') {
            if (buff[buff_index] == ':') {
                line->words[word_index].type = LABEL;
                int sub_str_len = buff_index - process_index;
                char *sub_str = push_array(arena->head, char, sub_str_len + 1);
                copy_str(sub_str, &buff[process_index], sub_str_len);
                sub_str[sub_str_len] = '\0';
                line->words[word_index].type = LABEL;
                line->words[word_index].str = sub_str;
                insert_label(arena, &label_array, sub_str, rows_parsed);
                ++buff_index;
                break;
            }
            ++buff_index;
        }

        if (!line->words[word_index].str) {
            line->words[word_index].type = EMPTY;
            buff_index = 0;
        }
        while (  buff[buff_index] == -96  || buff[buff_index] == -62 || buff[buff_index] == ' ' || buff[buff_index] == '\t') ++buff_index;
        process_index = buff_index;
        ++word_index;

        while ( buff[buff_index] != -96  && buff[buff_index] != -62  && buff[buff_index] != ' ' && buff[buff_index] != '\n' && buff[buff_index] != '\0' && buff[buff_index] != '\t') ++buff_index;

        int sub_str_len = buff_index - process_index;
        char *sub_str =  push_array(arena->head, char, sub_str_len +1);
        copy_str(sub_str, &buff[process_index], sub_str_len);
        sub_str[sub_str_len] = '\0';
        if (!strcmp(sub_str, "LI")) {
            line->words[word_index].type    = LOAD_IMM;
            line->words[word_index].str     = sub_str;
            line->words[word_index +1].type = I_REG;
            line->words[word_index +2].type = I_NUM;
            line->words[word_index +3].type = EMPTY;
        }else if (!strcmp(sub_str, "L.D")) {
            line->words[word_index].type    = LOAD_D;
            //-1 means not started fetch from mem
            line->words[word_index].i_arg[0] = -1;
            line->words[word_index].str     = sub_str;
            line->words[word_index +1].type = F_REG;
            line->words[word_index +2].type = OFFSET;
            line->words[word_index +3].type = EMPTY;
        } else if (!strcmp(sub_str, "LW")) {
            line->words[word_index].i_arg[0] = -1;
            line->words[word_index].type    = LOAD_W;
            line->words[word_index].str     = sub_str;
            line->words[word_index +1].type = I_REG;
            line->words[word_index +2].type = OFFSET;
            line->words[word_index +3].type = EMPTY;

        }else if (!strcmp(sub_str, "DIV.D")) {
            line->words[word_index].type    = DIV_D;
            line->words[word_index].i_arg[0] = cpu->fp_div_cycles;
            line->words[word_index].str     = sub_str;
            line->words[word_index +1].type = F_REG;
            line->words[word_index +2].type = F_REG;
            line->words[word_index +3].type = F_REG;
            line->ex_type                   = TYPE_DIV1;
        }else if (!strcmp(sub_str, "MUL.D")) {
            line->words[word_index].type    = MUL_D;
            line->words[word_index].i_arg[0] = cpu->fp_mull_cycles;
            line->words[word_index].str     = sub_str;
            line->words[word_index +1].type = F_REG;
            line->words[word_index +2].type = F_REG;
            line->words[word_index +3].type = F_REG;
            line->ex_type                   = TYPE_MUL1;
        }else if (!strcmp(sub_str, "SUB")) {
            line->words[word_index].type    = SUB;
            line->words[word_index].str     = sub_str;
            line->words[word_index +1].type = I_REG;
            line->words[word_index +2].type = I_REG;
            line->words[word_index +3].type = I_REG;
            line->ex_type                   = TYPE_EX;
        }else if (!strcmp(sub_str, "SUB.D")) {
            line->words[word_index].type    = SUB_D;
            line->words[word_index].i_arg[0] = cpu->fp_add_sub_cycles;
            line->words[word_index].str     = sub_str;
            line->words[word_index +1].type = F_REG;
            line->words[word_index +2].type = F_REG;
            line->words[word_index +3].type = F_REG;
            line->ex_type                   = TYPE_ADD1;
        }else if (!strcmp(sub_str, "ADD")) {
            line->words[word_index].type    = ADD;
            line->words[word_index].str     = sub_str;
            line->words[word_index +1].type = I_REG;
            line->words[word_index +2].type = I_REG;
            line->words[word_index +3].type = I_REG;
            line->ex_type                   = TYPE_EX;
        }else if (!strcmp(sub_str, "ADD.D")) {
            line->words[word_index].type    = ADD_D;
            line->words[word_index].i_arg[0] = cpu->fp_add_sub_cycles;
            line->words[word_index].str     = sub_str;
            line->words[word_index +1].type = F_REG;
            line->words[word_index +2].type = F_REG;
            line->words[word_index +3].type = F_REG;
            line->ex_type                   = TYPE_ADD1;
        } else if (!strcmp(sub_str, "S.D")) {
            line->words[word_index].i_arg[0] = -1;
            line->words[word_index].type    = STORE_D;
            line->words[word_index].str     = sub_str;
            line->words[word_index +1].type = F_REG;
            line->words[word_index +2].type = OFFSET;
            line->words[word_index +3].type = EMPTY;
        } else if (!strcmp(sub_str, "SW")) {
            line->words[word_index].i_arg[0] = -1;
            line->words[word_index].type    = STORE_W;
            line->words[word_index].str     = sub_str;
            line->words[word_index +1].type = I_REG;
            line->words[word_index +2].type = OFFSET;
            line->words[word_index +3].type = EMPTY;
        }else if (!strcmp(sub_str, "ADDI")) {
            line->words[word_index].type    = ADD_IMM;
            line->words[word_index].str     = sub_str;
            line->words[word_index +1].type = I_REG;
            line->words[word_index +2].type = I_REG;
            line->words[word_index +3].type = I_NUM;
            line->ex_type                   = TYPE_ADD1;

        } else if (!strcmp(sub_str, "J")) {
            line->words[word_index].type    = UNCONDITIONAL_JUMP;
            line->words[word_index].str     = sub_str;
            line->words[word_index +1].type = JUMP;
            line->words[word_index +2].type = EMPTY;
            line->words[word_index +3].type = EMPTY;
        } else if (!strcmp(sub_str, "BEQ")) {
            line->words[word_index].type    = BEQ;
            line->words[word_index].str     = sub_str;
            line->words[word_index +1].type = I_REG;
            line->words[word_index +2].type = I_REG;
            line->words[word_index +3].type = JUMP;
        } else if (!strcmp(sub_str, "BNE")) {
            line->words[word_index].type    = BNE;
            line->words[word_index].str     = sub_str;
            line->words[word_index +1].type = I_REG;
            line->words[word_index +2].type = I_REG;
            line->words[word_index +3].type = JUMP;
        } else {


            line->words[word_index].type    = EMPTY;
            line->words[word_index].str     = sub_str;
            line->words[word_index +1].type = EMPTY;
            line->words[word_index +2].type = EMPTY;
            line->words[word_index +3].type = EMPTY;
        }


        while (  buff[buff_index] == -96  || buff[buff_index] == -62 || buff[buff_index] == ' ' || buff[buff_index] == '\t') ++buff_index;
        process_index = buff_index;
        ++word_index;

        while (word_index < 5 && line->words[word_index].type != EMPTY) {

            while (  buff[buff_index] != -96  && buff[buff_index] != -62  && buff[buff_index]  != ' ' && buff[buff_index] != '\n' && buff[buff_index] != '\0' && buff[buff_index] != '\t' 
                    && buff[buff_index] != ',' ) {
                ++buff_index;
            }


            int sub_str_len = buff_index - process_index;
            char *sub_str =  push_array(arena->head, char, sub_str_len +1);
            copy_str(sub_str, &buff[process_index], sub_str_len);
            sub_str[sub_str_len] = '\0';
            switch (line->words[word_index].type) {
                case I_REG:{
                               //remove $
                               ++sub_str;
                               line->words[word_index].str = sub_str;
                               line->words[word_index].i_arg[0] = atoi(sub_str);
                           }break;
                case F_REG: {
                                //remove F
                                ++sub_str;
                                line->words[word_index].str = sub_str;
                                line->words[word_index].i_arg[0] = atoi(sub_str);
                            }break;
                case OFFSET: {
                                 int null_offset = 0;
                                 char *start_sub_str = sub_str;
                                 line->words[word_index].str = sub_str;
                                 while (sub_str[null_offset] != '(') ++null_offset;
                                 sub_str[null_offset] = '\0';
                                 line->words[word_index].i_arg[0] = atoi(sub_str);
                                 start_sub_str[null_offset]  = '(';
                                 sub_str = sub_str + null_offset +1;
                                 bool base_mem = true;
                                 if (sub_str[0] == '$')  {
                                     base_mem = false;
                                     ++sub_str;
                                 }
                                 buff_index =  1;
                                 while (sub_str[buff_index] != ')') ++buff_index;
                                 sub_str[buff_index] = '\0';
                                 //-1 means mem offset, -2 means int offset
                                 if (base_mem) {
                                     line->words[word_index +1].i_arg[0] =  -1;
                                 } else {
                                     line->words[word_index +1].i_arg[0] =  -2;
                                 }
                                 line->words[word_index].i_arg[1] = atoi(sub_str);
                                 sub_str[buff_index] = ')';
                                 sub_str[buff_index +1] = '\0';

                             }break;
                case I_NUM: {
                                line->words[word_index].str = sub_str;
                                line->words[word_index].i_arg[0] = atoi(sub_str);
                            }break;
                case JUMP: {

                               line->words[word_index].str = sub_str;

                                line->words[word_index].i_arg[0] = -1;
                               bool is_digit = ((sub_str[0] >= 48 && sub_str[0] <=57) || sub_str[0] == 45);
                               if (is_digit) {
                                   int offset = atoi(sub_str);
                                   line->words[word_index].i_arg[0] = offset + rows_parsed ;
                               }
                           }break;
                default: {
                         }break;
            }




            while (  buff[buff_index] == -96  || buff[buff_index] == -62 || buff[buff_index] == ' ' || buff[buff_index] == '\t' || buff[buff_index] == ',') ++buff_index;
            process_index = buff_index;
            ++word_index;
        }


        rows_parsed++;
    }

    cpu->predict_taken = push_array(arena->head, bool*, rows_parsed);
    //patch in label location
    for (int row = 0; row <rows_parsed; ++row) {
        Words *line = &file_info->line[row];
        for (int word_index = 0; word_index < 5; ++word_index) {
            bool patch_label = ( line->words[word_index].type == JUMP &&  line->words[word_index].i_arg[0] == -1);
            if (patch_label) {
                for (int label_index = 0; label_index <label_array->size; ++label_index) {
                    if (!strcmp(label_array->data[label_index].str, line->words[word_index].str)) {
                        int label_loc = label_array->data[label_index].row;
                        line->words[word_index].i_arg[0] = label_loc;
                        break;
                    }
                }
                break;
            }
        }
        cpu->predict_taken[row] = true;
    }

    fclose(file);
}


int get_lines(char *name) {
    FILE *file = fopen(name, "r");
    int lines = 0;

    char c = '\n';
    char prev_c = '\n';
    c = fgetc(file);
    while (c  != EOF) {
        if (prev_c == '\n') {
            if (c != '\n') ++lines;

        }

        prev_c = c;
        c = fgetc(file);
    }
    fclose(file);
    return lines;
}


typedef struct Log_Cell Log_Cell;
struct Log_Cell {
    Stage stage;
    Ex_Type ex_type;

};

#define LOG_TABLE_ROW 32
#define LOG_TABLE_COL 64
typedef struct Log_Table Log_Table;
struct Log_Table {
    Log_Cell *cells;

    char** str;
    int *instr_row;
    bool *fin;


    int row_cap;
    int col_cap;
    int row_size;
};

void init_log_table(Arena *arena, Log_Table *table, int rows, int cols) {
    table->row_cap = rows;
    table->col_cap = cols;
    table->row_size = 0;
    table->str = push_array(arena->head, char*, (rows *2));
    table->instr_row = push_array(arena->head, int, rows);
    table->fin = push_array(arena->head, bool, rows);
    table->cells = push_array(arena->head, Log_Cell, (rows * cols));
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col <cols; ++col) {
            table->cells[(row * cols) + col].stage = INACTIVE;
        }
        table->instr_row[row] = -1;
    }
}

void log_table(Arena *arena, Log_Table **table, Words *line, Stage stage,  int col, int queue_row) {

    bool expand_row =  (*table)->row_size == (*table)->row_cap;
    bool expand_col =  col >= (*table)->col_cap;
    if (expand_row || expand_col) {
        int new_col = (*table)->col_cap;
        while ( col >= new_col) {
            new_col *= 2;
        }
        int new_row = (*table)->row_size;
        if (expand_row) new_row *= 2;
        Log_Table *new_table = push_struct(arena->head, Log_Table);
        init_log_table(arena, new_table, new_row, new_col);

        for (int row = 0; row < (*table)->row_size; ++row) {
            new_table->str[row] = (*table)->str[row];
            new_table->instr_row[row] = (*table)->instr_row[row];
            new_table->fin[row] = (*table)->fin[row];

            for (int col  =0 ; col <(*table)->col_cap; ++col) {
                new_table->cells[ (new_table->col_cap * row) + col] = (*table)->cells[ ((*table)->col_cap * row) + col];
            }
        }

        new_table->row_size = (*table)->row_size;
        (*table) = new_table;

    }

    int matching_row = -1;
    if (line->table_row != -1 && !(*table)->fin[line->table_row])  matching_row =   line->table_row;

    if (matching_row == -1)  {
        matching_row  = (*table)->row_size;
        line->table_row = matching_row;
        (*table)->instr_row[matching_row] = line->row; 
        (*table)->str[matching_row] = line->instr_str; 
        (*table)->fin[matching_row] = false; 
        (*table)->row_size++;
    }

    (*table)->cells[ (matching_row * (*table)->col_cap) + col].stage = stage; 
    (*table)->cells[ (matching_row * (*table)->col_cap) + col].ex_type = line->ex_type; 
    if (stage == WB) {
        (*table)->fin[matching_row] = true;
    }
}


bool check_next_stage_available(Cpu *cpu, Words *line) {
    switch (line->current_stage ) {
        case INACTIVE: {
                           if (!cpu->curr_stage_active[0]) {
                               cpu->curr_stage_active[0] = true;
                               return true;
                           }
                           else return  false;
                       }break;
        case IF: {
                     if (!cpu->curr_stage_active[1]) return true;
                     else return false;
                 }break;

        case ID: {
                     if (!cpu->curr_stage_active[2]) return true;
                     else return false;
                 }break;
        case EX: {
                     return true;
                 }break;
        case MEM: {

                      if (!cpu->curr_stage_active[4]) return true;
                      else return false;
                  }break;
        case WB: {
                     return true;
                 }break;
        default:{
                    return false;
                }break;
    }
}

bool cache_check(Cpu *cpu, int mem_address) {
    Cache *cache = &cpu->l1;
    int index = mem_address % CACHE_SET;
    if (cache->data[index][0].mem_address == mem_address ||
            cache->data[index][1].mem_address == mem_address) return true;
    else return false;

}

void insert_cache(Cpu *cpu, int mem_address) {
    Cache *cache = &cpu->l1;
    int index = mem_address % CACHE_SET;
    if (cache->data[index][0].recent == 0 ) {
        cache->data[index][0].mem_address = mem_address;
        cache->data[index][0].val = cpu->mem[mem_address].val;
        cache->data[index][0].recent = 1;
    } else if (cache->data[index][1].recent == 0 ) {
        cache->data[index][1].mem_address = mem_address;
        cache->data[index][1].val = cpu->mem[mem_address].val;
        cache->data[index][1].recent = 1;
    } else {
        cache->data[index][0].mem_address = mem_address;
        cache->data[index][0].val = cpu->mem[mem_address].val;
        cache->data[index][0].recent = 1;
        cache->data[index][1].recent = 0;
    }

}

bool hazards_check(Cpu *cpu, Words *line) {
    switch (line->words[1].type) {
        case LOAD_IMM: {
                           int i_reg = line->words[2].i_arg[0];
                           if (cpu->regs.i[i_reg].write ) {
                               return false;
                           } else return true;
                       }break;
        case LOAD_D: {
                         int f_reg =  line->words[2].i_arg[0];
                         int offset  = line->words[3].i_arg[0];
                         int base  = line->words[3].i_arg[1];

                         if (line->words[4].i_arg[0] == -1) {
                             int mem_address =  (offset + base) % cpu->address_count;
                             if (cpu->mem[mem_address].read && cpu->regs.fp[f_reg].write) return false;
                             else return true;
                         } else {
                             int i_reg = (offset + base) % I_REGS;
                             int mem_address = cpu->regs.i[i_reg].val % cpu->address_count;
                             if (cpu->mem[mem_address].read && cpu->regs.i[i_reg].read && cpu->regs.fp[f_reg].write) return false;
                             else return true;
                         }
                     }break;
        case LOAD_W: {

                         int dest_i_reg =  line->words[2].i_arg[0];
                         int offset  = line->words[3].i_arg[0];
                         int base  = line->words[3].i_arg[1];

                         if (line->words[4].i_arg[0] == -1) {
                             int mem_address =  (offset + base) % cpu->address_count;
                             if (cpu->mem[mem_address].read && cpu->regs.i[dest_i_reg].write) return false;
                             else return true;
                         } else {
                             int i_reg = (offset + base) % I_REGS;
                             int mem_address = cpu->regs.i[i_reg].val % cpu->address_count;
                             if (cpu->mem[mem_address].read && cpu->regs.i[i_reg].read && cpu->regs.i[dest_i_reg].write) return false;
                             else return true;
                         }






                     }break;
        case STORE_D: {

                          int f_reg =  line->words[2].i_arg[0];
                          int offset  = line->words[3].i_arg[0];
                          int base  = line->words[3].i_arg[1];

                          if (line->words[4].i_arg[0] == -1) {
                              int mem_address =  (offset + base) % cpu->address_count;
                              if (cpu->mem[mem_address].write && cpu->regs.fp[f_reg].read) return false;
                              else return true;
                          } else {
                              int i_reg = (offset + base) % I_REGS;
                              int mem_address = cpu->regs.i[i_reg].val % cpu->address_count;
                              if (cpu->mem[mem_address].write && cpu->regs.i[i_reg].read && cpu->regs.fp[f_reg].read) return false;
                              else return true;
                          }



                      }break;
        case STORE_W: {

                          int dest_i_reg =  line->words[2].i_arg[0];
                          int offset  = line->words[3].i_arg[0];
                          int base  = line->words[3].i_arg[1];

                          if (line->words[4].i_arg[0] == -1) {
                              int mem_address =  (offset + base) % cpu->address_count;
                              if (cpu->mem[mem_address].write && cpu->regs.i[dest_i_reg].read) return false;
                              else return true;
                          } else {
                              int i_reg = (offset + base) % I_REGS;
                              int mem_address = cpu->regs.i[i_reg].val % cpu->address_count;
                              if (cpu->mem[mem_address].write && cpu->regs.i[i_reg].read && cpu->regs.i[dest_i_reg].read) return false;
                              else return true;
                          }
                      }break;
        case SUB:
        case ADD: {
                      int dest_i_reg = line->words[2].i_arg[0];
                      int arg1_i_reg = line->words[3].i_arg[0];
                      int arg2_i_reg = line->words[4].i_arg[0];

                      if (cpu->regs.i[dest_i_reg].write && 
                              cpu->regs.i[arg1_i_reg].read &&
                              cpu->regs.i[arg2_i_reg].read) return false;
                      else return true;
                  }break;
        case ADD_IMM: {

                          int dest_i_reg = line->words[2].i_arg[0];
                          int arg1_i_reg = line->words[3].i_arg[0];

                          if (cpu->regs.i[dest_i_reg].write && 
                                  cpu->regs.i[arg1_i_reg].read) return false;
                          else return true;
                      }break;
        case SUB_D:
        case MUL_D:
        case DIV_D:
        case ADD_D: {
                        int dest_reg = line->words[2].i_arg[0];
                        int arg1_reg = line->words[3].i_arg[0];
                        int arg2_reg = line->words[4].i_arg[0];

                        if (cpu->regs.fp[dest_reg].write && 
                                cpu->regs.fp[arg1_reg].read &&
                                cpu->regs.fp[arg2_reg].read) return false;
                        else return true;
                    }break;
        case UNCONDITIONAL_JUMP: {
                                     return false;
                                 }break;
        case BEQ:
        case BNE: {
                      int i_reg1 = line->words[2].i_arg[0];
                      int i_reg2 = line->words[3].i_arg[0];
                      if (cpu->regs.i[i_reg1].read && cpu->regs.i[i_reg2].read) return false;
                      else  return true;
                  }break;
        default: {
                     return false;
                 }break;
    }
    return false;
}


int create_backup(Arena *arena, Cpu *cpu, Words *line, I_Queue **i_queue) {
    int save_index = 0;
    for (int i = 0; i <BACKUP_COUNT; ++i) {
        if (!cpu->backup_data[i].active) {
            cpu->backup_data[i].active = true;
            save_index = i;
            break;
        }
    }

    cpu->backup_data[save_index].i_queue_size =  (*i_queue)->size;
    cpu->backup_data[save_index].i_queue_row =  (*i_queue)->row;

    cpu->backup_data[save_index].restore_pc = cpu->pc;

    for (int i = 0; i <I_REGS; ++i) {
        cpu->backup_data[save_index].regs.i[i].write =  cpu->regs.i[i].write;
        cpu->backup_data[save_index].regs.i[i].read =  cpu->regs.i[i].read;
        cpu->backup_data[save_index].regs.i[i].val =  cpu->regs.i[i].val;
        cpu->backup_data[save_index].regs.i[i].i_queue_row =  cpu->regs.i[i].i_queue_row;
    }

    for (int i = 0; i <FP_REGS; ++i) {
        cpu->backup_data[save_index].regs.fp[i].write = cpu->regs.fp[i].write;
        cpu->backup_data[save_index].regs.fp[i].read  = cpu->regs.fp[i].read;
        cpu->backup_data[save_index].regs.fp[i].val   = cpu->regs.fp[i].val;
        cpu->backup_data[save_index].regs.fp[i].i_queue_row =  cpu->regs.fp[i].i_queue_row;
    }

    for (int i = 0; i <19; ++i) {
        cpu->backup_data[save_index].mem[i].write = cpu->mem[i].write;
        cpu->backup_data[save_index].mem[i].read  = cpu->mem[i].read;
        cpu->backup_data[save_index].mem[i].val   = cpu->mem[i].val;
        cpu->backup_data[save_index].mem[i].i_queue_row =  cpu->mem[i].i_queue_row;
    }

    return save_index;
}

void restore_backup(Cpu *cpu, int save_index, I_Queue **i_queue) {

    (*i_queue)->row = cpu->backup_data[save_index].i_queue_row;
    (*i_queue)->size = (*i_queue)->row + 1;

    cpu->backup_data[save_index].active = false;
    cpu->pc = (*i_queue)->lines[(*i_queue)->row].row + 1; 

    for (int i = 0; i <I_REGS; ++i) {

        if (cpu->backup_data[save_index].regs.i[i].i_queue_row  < (*i_queue)->row) continue;
        cpu->regs.i[i].write  = cpu->backup_data[save_index].regs.i[i].write ;
        cpu->regs.i[i].read  = cpu->backup_data[save_index].regs.i[i].read ;
        cpu->regs.i[i].val  = cpu->backup_data[save_index].regs.i[i].val ;
    }

    for (int i = 0; i <FP_REGS; ++i) {
        if (cpu->backup_data[save_index].regs.fp[i].i_queue_row  < (*i_queue)->row) continue;
        cpu->regs.fp[i].write  = cpu->backup_data[save_index].regs.fp[i].write ;
        cpu->regs.fp[i].read  = cpu->backup_data[save_index].regs.fp[i].read  ;
        cpu->regs.fp[i].val  = cpu->backup_data[save_index].regs.fp[i].val   ;
    }

    for (int i = 0; i <19; ++i) {
        if (cpu->backup_data[save_index].mem[i].i_queue_row  < (*i_queue)->row) continue;
        cpu->mem[i].write  = cpu->backup_data[save_index].mem[i].write ;
        cpu->mem[i].read  = cpu->backup_data[save_index].mem[i].read  ;
        cpu->mem[i].val  = cpu->backup_data[save_index].mem[i].val   ;
    }

    for (int i =0; i <5; ++i) {
        cpu->curr_stage_active[i] = false;
    }
    for (int i = 0; i <(*i_queue)->size; ++i) {
        if ((*i_queue)->lines[i].current_stage == IF) {
            cpu->curr_stage_active[0] = true;
        }  else if ((*i_queue)->lines[i].current_stage == ID) {
            cpu->curr_stage_active[1] = true;
        }  else if ((*i_queue)->lines[i].current_stage == EX) {
            cpu->curr_stage_active[2] = true;
        }  else if ((*i_queue)->lines[i].current_stage == MEM) {
            cpu->curr_stage_active[3] = true;
        }  else if ((*i_queue)->lines[i].current_stage == WB) {
            cpu->curr_stage_active[4] = true;

        }
    }

}

void execute_instruction(Arena *arena, Log_Table **table, I_Queue **i_queue, Cpu *cpu, Words *line) {


    switch (line->current_stage) {
        case INACTIVE: {
                           if (check_next_stage_available(cpu, line)) {
                               line->current_stage = IF;
                               line->stall = false;
                               line->processed = false;
                               goto IF_EXECUTE;
                           }else break;
                       }

        case IF: {


                     if (check_next_stage_available(cpu, line) && line->processed) {
                         cpu->curr_stage_active[0] = false;
                         cpu->curr_stage_active[1] = true;
                         line->current_stage = ID;
                         line->processed = false;
                         line->stall = false;
                         goto ID_EXECUTE;
                     }


                     line->stall = true;
IF_EXECUTE:
                     switch (line->words[1].type) {
                         case BEQ:
                         case BNE: {

                                       if (!line->processed) {



                                           line->words[4].i_arg[1] = create_backup(arena, cpu, line, i_queue);
                                           if (cpu->predict_taken[line->row]) {
                                               int jump_row =  line->words[4].i_arg[0];
                                               (*i_queue)->size = (*i_queue)->row + 1;
                                               cpu->pc = jump_row;
                                           }
                                           line->processed = true;

                                       }
                                   }break;
                         case UNCONDITIONAL_JUMP:{
                                                     int jump_row = line->words[2].i_arg[0];
                                                     (*i_queue)->size = (*i_queue)->row + 1;
                                                     cpu->pc = jump_row;
                                                     line->processed = true;
                                                 }break;
                         default: {
                                      if (!line->processed) {
                                          line->processed = true;
                                      }
                                  }break;
                     }
                     if (!line->stall) log_table(arena, table, line, line->current_stage, cpu->cycles, (*i_queue)->row);
                     else log_table(arena, table, line, STALL, cpu->cycles, (*i_queue)->row);

                 }break;
        case ID: {

                     if (line->processed) {
                         line->current_stage = EX;
                         line->ex_type = TYPE_EX;
                         cpu->curr_stage_active[1] = false;
                         cpu->curr_stage_active[2] = true;
                         line->processed = false;
                         line->stall = false;
                         goto EX_EXECUTE;
                     } 


                     line->stall = true;
ID_EXECUTE:
                     bool hazard = hazards_check(cpu, line);
                     switch (line->words[1].type) {

                         case LOAD_IMM: {
                                            int i_reg = line->words[2].i_arg[0];
                                            if (!hazard) {
                                                cpu->regs.i[i_reg].write = false;
                                                cpu->regs.i[i_reg].read = false;
                                                cpu->regs.i[i_reg].i_queue_row = (*i_queue)->row;
                                                line->processed = true;
                                            }

                                        }break;
                         case LOAD_D: {

                                          int f_reg =  line->words[2].i_arg[0];
                                          int offset  = line->words[3].i_arg[0];
                                          int base  = line->words[3].i_arg[1];
                                          if (!hazard) {
                                              line->processed = true;
                                              cpu->regs.fp[f_reg].write = false;
                                              cpu->regs.fp[f_reg].read = false;
                                              cpu->regs.fp[f_reg].i_queue_row = (*i_queue)->row;
                                              if (line->words[4].i_arg[0] == -1) {
                                                  int mem_address =  (offset + base) % cpu->address_count;
                                                  cpu->mem[mem_address].write = false;
                                                  cpu->mem[mem_address].read = true;
                                                  cpu->mem[mem_address].i_queue_row = (*i_queue)->row;
                                              } else {
                                                  int i_reg = (offset + base) % I_REGS;
                                                  cpu->regs.i[i_reg].write = false;
                                                  cpu->regs.i[i_reg].read =  true;
                                                  cpu->regs.i[i_reg].i_queue_row = (*i_queue)->row;

                                                  int mem_address = cpu->regs.i[i_reg].val % cpu->address_count;
                                                  cpu->mem[mem_address].write = false;
                                                  cpu->mem[mem_address].read = true;
                                                  cpu->mem[mem_address].i_queue_row = (*i_queue)->row;
                                              }
                                          }
                                      }break;
                         case LOAD_W: {

                                          int dest_i_reg =  line->words[2].i_arg[0];
                                          int offset  = line->words[3].i_arg[0];
                                          int base  = line->words[3].i_arg[1];
                                          if (!hazard) {
                                              line->processed = true;
                                              cpu->regs.i[dest_i_reg].write = false;
                                              cpu->regs.i[dest_i_reg].read = false;
                                              cpu->regs.i[dest_i_reg].i_queue_row = (*i_queue)->row;

                                              if (line->words[4].i_arg[0] == -1) {
                                                  int mem_address =  (offset + base) % cpu->address_count;
                                                  cpu->mem[mem_address].write = false;
                                                  cpu->mem[mem_address].read = true;
                                                  cpu->mem[mem_address].i_queue_row = (*i_queue)->row;

                                              } else {
                                                  int i_reg = (offset + base) % I_REGS;
                                                  cpu->regs.i[i_reg].write = false;
                                                  cpu->regs.i[i_reg].read =  true;
                                                  cpu->regs.i[i_reg].i_queue_row = (*i_queue)->row;

                                                  int mem_address = cpu->regs.i[i_reg].val % cpu->address_count;
                                                  cpu->mem[mem_address].write = false;
                                                  cpu->mem[mem_address].read = true;

                                                  cpu->mem[mem_address].i_queue_row = (*i_queue)->row;
                                              }
                                          }




                                      }break;
                         case STORE_D: {
                                           int f_reg =  line->words[2].i_arg[0];
                                           int offset  = line->words[3].i_arg[0];
                                           int base  = line->words[3].i_arg[1];
                                           if (!hazard) {
                                               line->processed = true;
                                               cpu->regs.fp[f_reg].write = false;
                                               cpu->regs.fp[f_reg].read = true;
                                               cpu->regs.fp[f_reg].i_queue_row = (*i_queue)->row;
                                               if (line->words[4].i_arg[0] == -1) {
                                                   int mem_address =  (offset + base) % cpu->address_count;
                                                   cpu->mem[mem_address].write = false;
                                                   cpu->mem[mem_address].read = false;
                                                   cpu->mem[mem_address].i_queue_row = (*i_queue)->row;

                                               } else {
                                                   int i_reg = (offset + base) % I_REGS;
                                                   cpu->regs.i[i_reg].write = false;
                                                   cpu->regs.i[i_reg].read =  false;
                                                   cpu->regs.i[i_reg].i_queue_row = (*i_queue)->row;
                                                   int mem_address = cpu->regs.i[i_reg].val % cpu->address_count;
                                                   cpu->mem[mem_address].write = false;
                                                   cpu->mem[mem_address].read = false;
                                                   cpu->mem[mem_address].i_queue_row = (*i_queue)->row;
                                               }
                                           }




                                       }break;
                         case STORE_W: {

                                           int dest_i_reg =  line->words[2].i_arg[0];
                                           int offset  = line->words[3].i_arg[0];
                                           int base  = line->words[3].i_arg[1];
                                           if (!hazard) {
                                               line->processed = true;
                                               cpu->regs.i[dest_i_reg].write = false;
                                               cpu->regs.i[dest_i_reg].read = true;
                                               cpu->regs.i[dest_i_reg].i_queue_row = (*i_queue)->row;
                                               if (line->words[4].i_arg[0] == -1) {
                                                   int mem_address =  (offset + base) % cpu->address_count;
                                                   cpu->mem[mem_address].write = false;
                                                   cpu->mem[mem_address].read = false;
                                                   cpu->mem[mem_address].i_queue_row = (*i_queue)->row;
                                               } else {
                                                   int i_reg = (offset + base) % I_REGS;
                                                   cpu->regs.i[i_reg].write = false;
                                                   cpu->regs.i[i_reg].read =  false;
                                                   cpu->regs.i[i_reg].i_queue_row = (*i_queue)->row;

                                                   int mem_address = cpu->regs.i[i_reg].val % cpu->address_count;
                                                   cpu->mem[mem_address].write = false;
                                                   cpu->mem[mem_address].read = false;
                                                   cpu->mem[mem_address].i_queue_row = (*i_queue)->row;
                                               }
                                           }
                                       }break;
                         case SUB:
                         case ADD: {

                                       int dest_i_reg = line->words[2].i_arg[0];
                                       int arg1_i_reg = line->words[3].i_arg[0];
                                       int arg2_i_reg = line->words[4].i_arg[0];
                                       if (!hazard) {

                                           line->processed = true;

                                           cpu->regs.i[arg1_i_reg].read =  true ;
                                           cpu->regs.i[arg1_i_reg].write = false ;
                                           cpu->regs.i[arg1_i_reg].i_queue_row = (*i_queue)->row ;

                                           cpu->regs.i[arg2_i_reg].read = true;
                                           cpu->regs.i[arg2_i_reg].write = false;
                                           cpu->regs.i[arg2_i_reg].i_queue_row = (*i_queue)->row;

                                           cpu->regs.i[dest_i_reg].read =  false ;
                                           cpu->regs.i[dest_i_reg].write = false ;
                                           cpu->regs.i[dest_i_reg].i_queue_row = (*i_queue)->row ;

                                       }


                                   }break;
                         case ADD_IMM: {

                                           int dest_i_reg = line->words[2].i_arg[0];
                                           int arg1_i_reg = line->words[3].i_arg[0];
                                           if (!hazard) {
                                               line->processed = true;

                                               cpu->regs.i[arg1_i_reg].read =  true ;
                                               cpu->regs.i[arg1_i_reg].write = false ;
                                               cpu->regs.i[arg1_i_reg].i_queue_row = (*i_queue)->row ;

                                               cpu->regs.i[dest_i_reg].read =  false ;
                                               cpu->regs.i[dest_i_reg].write = false ;
                                               cpu->regs.i[dest_i_reg].i_queue_row = (*i_queue)->row ;
                                           }

                                       }break;
                         case SUB_D:
                         case MUL_D:
                         case DIV_D:
                         case ADD_D: {
                                         int dest_reg = line->words[2].i_arg[0];
                                         int arg1_reg = line->words[3].i_arg[0];
                                         int arg2_reg = line->words[4].i_arg[0];
                                         if (!hazard) {
                                             line->processed = true;
                                             //set dest last in case equal to arg
                                             cpu->regs.fp[arg1_reg].read =  true ;
                                             cpu->regs.fp[arg1_reg].write = false ;
                                             cpu->regs.fp[arg1_reg].i_queue_row = (*i_queue)->row ;

                                             cpu->regs.fp[arg2_reg].read =  true ;
                                             cpu->regs.fp[arg2_reg].write = false ;
                                             cpu->regs.fp[arg2_reg].i_queue_row = (*i_queue)->row ;

                                             cpu->regs.fp[dest_reg].read =  false ;
                                             cpu->regs.fp[dest_reg].write = false ;
                                             cpu->regs.fp[dest_reg].i_queue_row = (*i_queue)->row ;

                                         }


                                     }break;
                         case UNCONDITIONAL_JUMP: {
                                                      line->processed = true;
                                                  }break;

                         case BEQ:
                         case BNE: {


                                       if (!hazard) {
                                           int i_reg1 = line->words[2].i_arg[0];
                                           int i_reg2 = line->words[3].i_arg[0];
                                           cpu->regs.i[i_reg1].write = false; 
                                           cpu->regs.i[i_reg2].write = false;;
                                           cpu->regs.i[i_reg2].i_queue_row = (*i_queue)->row;;
                                           line->processed = true;
                                       }
                                   }break;
                         default: {
                                  }break;
                     }


                     if (!line->stall) log_table(arena, table, line, line->current_stage, cpu->cycles, (*i_queue)->row);
                     else log_table(arena, table, line, STALL, cpu->cycles, (*i_queue)->row);

                 }break;

        case EX: {

                     if (line->processed && check_next_stage_available(cpu, line)) {
                         line->processed = false;
                         line->stall  = false;
                         line->current_stage = MEM;
                         cpu->curr_stage_active[2] = false;
                         cpu->curr_stage_active[3] = true;
                         goto MEM_EXECUTE;
                     } 


                     line->stall = true;
EX_EXECUTE:
                     switch (line->words[1].type) {
                         case LOAD_IMM: {
                                            if (!line->processed) {
                                                int i_reg = line->words[2].i_arg[0];
                                                int imm_val = line->words[3].i_arg[0];
                                                cpu->regs.i[i_reg].val = imm_val;
                                                line->processed = true;
                                            }
                                        }break;
                         case LOAD_D: {
                                          if (!line->processed) {
                                              line->processed = true;
                                          }
                                      }break;
                         case LOAD_W: {

                                          if (!line->processed) {
                                              line->processed = true;
                                          }
                                      }break;
                         case STORE_D: {
                                           if (!line->processed) {
                                               line->processed = true;
                                           }
                                       }break;
                         case STORE_W: {

                                           if (!line->processed) {
                                               line->processed = true;
                                           }
                                       }break;
                         case ADD: {
                                       if (!line->processed) {
                                           int dest_i_reg = line->words[2].i_arg[0];
                                           int arg1_i_reg = line->words[3].i_arg[0];
                                           int arg2_i_reg = line->words[4].i_arg[0];
                                           cpu->regs.i[dest_i_reg].val = cpu->regs.i[arg1_i_reg].val +  cpu->regs.i[arg2_i_reg].val;
                                           line->processed = true;
                                       }
                                   }break;
                         case SUB: {

                                       if (!line->processed) {
                                           int dest_i_reg = line->words[2].i_arg[0];
                                           int arg1_i_reg = line->words[3].i_arg[0];
                                           int arg2_i_reg = line->words[4].i_arg[0];
                                           cpu->regs.i[dest_i_reg].val = cpu->regs.i[arg1_i_reg].val -  cpu->regs.i[arg2_i_reg].val;
                                           line->processed = true;
                                       }
                                   }break;

                         case ADD_IMM: {


                                           if (!line->processed) {
                                               int dest_i_reg = line->words[2].i_arg[0];
                                               int arg1_i_reg = line->words[3].i_arg[0];
                                               int imm = line->words[4].i_arg[0];
                                               cpu->regs.i[dest_i_reg].val = cpu->regs.i[arg1_i_reg].val +  imm;
                                               line->processed = true;
                                           }
                                       }break;
                         case ADD_D: {
                                         if (!line->processed) {

                                             line->stall = false;
                                             int dest_reg = line->words[2].i_arg[0];
                                             int arg1_reg = line->words[3].i_arg[0];
                                             int arg2_reg = line->words[4].i_arg[0];
                                             int cycles_remain = line->words[1].i_arg[0];

                                             switch (cycles_remain) {
                                                 case 1: {
                                                             line->ex_type = TYPE_ADD1;
                                                         }break;
                                                 case 2: {
                                                             line->ex_type = TYPE_ADD2;
                                                         }break;
                                                 default:break;
                                             }

                                             line->words[1].i_arg[0] -=1;
                                             if (cycles_remain == 1) {
                                                 cpu->regs.fp[dest_reg].val = cpu->regs.fp[arg1_reg].val + cpu->regs.fp[arg2_reg].val;
                                                 line->processed = true;
                                             }
                                         }
                                     }break;
                         case SUB_D: {

                                         if (!line->processed) {

                                             line->stall = false;
                                             int dest_reg = line->words[2].i_arg[0];
                                             int arg1_reg = line->words[3].i_arg[0];
                                             int arg2_reg = line->words[4].i_arg[0];
                                             int cycles_remain = line->words[1].i_arg[0];

                                             switch (cycles_remain) {
                                                 case 1: {
                                                             line->ex_type = TYPE_ADD1;
                                                         }break;
                                                 case 2: {
                                                             line->ex_type = TYPE_ADD2;
                                                         }break;
                                                 default:break;
                                             }

                                             line->words[1].i_arg[0] -=1;
                                             if (cycles_remain == 1) {
                                                 cpu->regs.fp[dest_reg].val = cpu->regs.fp[arg1_reg].val - cpu->regs.fp[arg2_reg].val;
                                                 line->processed = true;
                                             }
                                         }


                                     }break;
                         case MUL_D: {

                                         if (!line->processed) {

                                             line->stall = false;
                                             int dest_reg = line->words[2].i_arg[0];
                                             int arg1_reg = line->words[3].i_arg[0];
                                             int arg2_reg = line->words[4].i_arg[0];
                                             int cycles_remain = line->words[1].i_arg[0];

                                             switch (cycles_remain) {
                                                 case 1: {
                                                             line->ex_type = TYPE_MUL1;
                                                         }break;
                                                 case 2: {
                                                             line->ex_type = TYPE_MUL2;
                                                         }break;
                                                 case 3: {
                                                             line->ex_type = TYPE_MUL3;
                                                         }break;
                                                 case 4: {
                                                             line->ex_type = TYPE_MUL4;
                                                         }break;
                                                 case 5: {
                                                             line->ex_type = TYPE_MUL5;
                                                         }break;
                                                 case 6: {
                                                             line->ex_type = TYPE_MUL6;
                                                         }break;
                                                 case 7: {
                                                             line->ex_type = TYPE_MUL7;
                                                         }break;
                                                 case 8: {
                                                             line->ex_type = TYPE_MUL8;
                                                         }break;
                                                 case 9: {
                                                             line->ex_type = TYPE_MUL9;
                                                         }break;
                                                 case 10: {
                                                              line->ex_type = TYPE_MUL10;
                                                          }break;
                                                 default:break;
                                             }

                                             line->words[1].i_arg[0] -=1;
                                             if (cycles_remain == 1) {
                                                 cpu->regs.fp[dest_reg].val = cpu->regs.fp[arg1_reg].val * cpu->regs.fp[arg2_reg].val;
                                                 line->processed = true;
                                             }
                                         }



                                     }break;
                         case DIV_D: {

                                         line->stall = false;
                                         int dest_reg = line->words[2].i_arg[0];
                                         int arg1_reg = line->words[3].i_arg[0];
                                         int arg2_reg = line->words[4].i_arg[0];
                                         int cycles_remain = line->words[1].i_arg[0];

                                         switch (cycles_remain) {
                                             case 1: {
                                                         line->ex_type = TYPE_DIV1;
                                                     }break;
                                             case 2: {
                                                         line->ex_type = TYPE_DIV2;
                                                     }break;
                                             case 3: {
                                                         line->ex_type = TYPE_DIV3;
                                                     }break;
                                             case 4: {
                                                         line->ex_type = TYPE_DIV4;
                                                     }break;
                                             case 5: {
                                                         line->ex_type = TYPE_DIV5;
                                                     }break;
                                             case 6: {
                                                         line->ex_type = TYPE_DIV6;
                                                     }break;
                                             case 7: {
                                                         line->ex_type = TYPE_DIV7;
                                                     }break;
                                             case 8: {
                                                         line->ex_type = TYPE_DIV8;
                                                     }break;
                                             case 9: {
                                                         line->ex_type = TYPE_DIV9;
                                                     }break;
                                             case 10: {
                                                          line->ex_type = TYPE_DIV10;
                                                      }break;
                                             case 11: {
                                                          line->ex_type = TYPE_DIV11;
                                                      }break;
                                             case 12: {
                                                          line->ex_type = TYPE_DIV12;
                                                      }break;
                                             case 13: {
                                                          line->ex_type = TYPE_DIV13;
                                                      }break;
                                             case 14: {
                                                          line->ex_type = TYPE_DIV14;
                                                      }break;
                                             case 15: {
                                                          line->ex_type = TYPE_DIV15;
                                                      }break;
                                             case 16: {
                                                          line->ex_type = TYPE_DIV16;
                                                      }break;
                                             case 17: {
                                                          line->ex_type = TYPE_DIV17;
                                                      }break;
                                             case 18: {
                                                          line->ex_type = TYPE_DIV18;
                                                      }break;
                                             case 19: {
                                                          line->ex_type = TYPE_DIV19;
                                                      }break;
                                             case 20: {
                                                          line->ex_type = TYPE_DIV20;
                                                      }break;
                                             case 21: {
                                                          line->ex_type = TYPE_DIV21;
                                                      }break;
                                             case 22: {
                                                          line->ex_type = TYPE_DIV22;
                                                      }break;
                                             case 23: {
                                                          line->ex_type = TYPE_DIV23;
                                                      }break;
                                             case 24: {
                                                          line->ex_type = TYPE_DIV24;
                                                      }break;
                                             case 25: {
                                                          line->ex_type = TYPE_DIV25;
                                                      }break;
                                             case 26: {
                                                          line->ex_type = TYPE_DIV26;
                                                      }break;
                                             case 27: {
                                                          line->ex_type = TYPE_DIV27;
                                                      }break;
                                             case 28: {
                                                          line->ex_type = TYPE_DIV28;
                                                      }break;
                                             case 29: {
                                                          line->ex_type = TYPE_DIV29;
                                                      }break;
                                             case 30: {
                                                          line->ex_type = TYPE_DIV30;
                                                      }break;
                                             case 31: {
                                                          line->ex_type = TYPE_DIV31;
                                                      }break;
                                             case 32: {
                                                          line->ex_type = TYPE_DIV32;
                                                      }break;
                                             case 33: {
                                                          line->ex_type = TYPE_DIV33;
                                                      }break;
                                             case 34: {
                                                          line->ex_type = TYPE_DIV34;
                                                      }break;
                                             case 35: {
                                                          line->ex_type = TYPE_DIV35;
                                                      }break;
                                             case 36: {
                                                          line->ex_type = TYPE_DIV36;
                                                      }break;
                                             case 37: {
                                                          line->ex_type = TYPE_DIV37;
                                                      }break;
                                             case 38: {
                                                          line->ex_type = TYPE_DIV38;
                                                      }break;
                                             case 39: {
                                                          line->ex_type = TYPE_DIV39;
                                                      }break;
                                             case 40: {
                                                          line->ex_type = TYPE_DIV40;
                                                      }break;

                                             default:break;

                                         }

                                         line->words[1].i_arg[0] -=1;
                                         if (cycles_remain == 1) {
                                             cpu->regs.fp[dest_reg].val = cpu->regs.fp[arg1_reg].val / cpu->regs.fp[arg2_reg].val;
                                             line->processed = true;
                                         }
                                     }break;

                         case UNCONDITIONAL_JUMP: {
                                                      line->processed = true;
                                                  }break;
                         case BNE: {


                                       if (!line->processed) {
                                           int i_reg1 = line->words[2].i_arg[0];
                                           int i_reg2 = line->words[3].i_arg[0];

                                           int backup_index = line->words[4].i_arg[1];
                                           bool taken =  cpu->regs.i[i_reg1].val !=  cpu->regs.i[i_reg2].val;
                                           if (taken !=   cpu->predict_taken[line->row]) {
                                               restore_backup(cpu, line->words[4].i_arg[1], i_queue);
                                               cpu->predict_taken[line->row] = !cpu->predict_taken[line->row];
                                           }
                                           cpu->backup_data[ line->words[4].i_arg[1]].active = false;
                                           cpu->regs.i[i_reg1].write = true;
                                           cpu->regs.i[i_reg2].write = true;
                                           cpu->regs.i[i_reg1].read = true;
                                           cpu->regs.i[i_reg2].read = true;
                                           line->processed = true;
                                       }




                                   }break;
                         case BEQ: {
                                       if (!line->processed) {
                                           int i_reg1 = line->words[2].i_arg[0];
                                           int i_reg2 = line->words[3].i_arg[0];

                                           int backup_index = line->words[4].i_arg[1];
                                           bool taken =  cpu->regs.i[i_reg1].val ==  cpu->regs.i[i_reg2].val;
                                           if (taken !=   cpu->predict_taken[line->row]) {
                                               restore_backup(cpu, line->words[4].i_arg[1], i_queue);
                                               cpu->predict_taken[line->row] = !cpu->predict_taken[line->row];
                                           }
                                           cpu->backup_data[ line->words[4].i_arg[1]].active = false;
                                           cpu->regs.i[i_reg1].write = true;
                                           cpu->regs.i[i_reg2].write = true;
                                           cpu->regs.i[i_reg1].read = true;
                                           cpu->regs.i[i_reg2].read = true;
                                           line->processed = true;
                                       }
                                   }break;
                         default:break;
                     }


                     if (!line->stall) log_table(arena, table, line, line->current_stage, cpu->cycles, (*i_queue)->row);
                     else log_table(arena, table, line, STALL, cpu->cycles, (*i_queue)->row);

                 } break;
        case MEM: {

                      if (line->processed && check_next_stage_available(cpu, line)) {
                          line->processed = false;
                          line->stall = false;
                          line->current_stage = WB;
                          cpu->curr_stage_active[3] = false;
                          cpu->curr_stage_active[4] = true;
                          goto WB_EXECUTE;
                      }

                      line->stall  = true;
MEM_EXECUTE:
                      switch (line->words[1].type) {
                          case LOAD_IMM: {
                                             if (!line->processed) {
                                                 int i_reg = line->words[2].i_arg[0];
                                                 int imm_val = line->words[3].i_arg[0];
                                                 cpu->regs.i[i_reg].val = imm_val;
                                                 cpu->regs.i[i_reg].write = true;
                                                 cpu->regs.i[i_reg].read = true;
                                                 line->processed = true;
                                             }

                                         }break;
                          case LOAD_D: {
                                           if (!line->processed) {
                                               int f_reg =  line->words[2].i_arg[0];
                                               int offset  = line->words[3].i_arg[0];
                                               int base  = line->words[3].i_arg[1];
                                               int mem_address = -1;
                                               if (line->words[4].i_arg[0] == -1) {
                                                   mem_address =  (offset + base) % cpu->address_count;

                                               } else {
                                                   int i_reg = (offset + base) % I_REGS;
                                                   mem_address = cpu->regs.i[i_reg].val % cpu->address_count;
                                               }

                                               if (!cache_check(cpu, mem_address)) {
                                                   insert_cache(cpu, mem_address);
                                                   line->words[1].i_arg[0] = 3;
                                               }

                                               if (line->words[1].i_arg[0] > 0) --line->words[1].i_arg[0];

                                               if (0 >= line->words[1].i_arg[0]) {
                                                   cpu->regs.fp[f_reg].val = cpu->mem[mem_address].val;
                                                   line->processed = true;
                                               }
                                           }
                                       }break;
                          case LOAD_W: {

                                           if (!line->processed) {
                                               int dest_i_reg =  line->words[2].i_arg[0];
                                               int offset  = line->words[3].i_arg[0];
                                               int base  = line->words[3].i_arg[1];
                                               int mem_address = -1;
                                               if (line->words[4].i_arg[0] == -1) {
                                                   mem_address =  (offset + base) % cpu->address_count;

                                               } else {
                                                   int i_reg = (offset + base) % I_REGS;
                                                   mem_address = cpu->regs.i[i_reg].val % cpu->address_count;
                                               }

                                               if (!cache_check(cpu, mem_address)) {
                                                   insert_cache(cpu, mem_address);
                                                   line->words[1].i_arg[0] = 3;
                                               }

                                               if (line->words[1].i_arg[0] > 0) --line->words[1].i_arg[0];

                                               if (0 >= line->words[1].i_arg[0]) {
                                                   cpu->regs.i[dest_i_reg].val = cpu->mem[mem_address].val;
                                                   line->processed = true;
                                               }
                                           }
                                       }break;

                          case STORE_D: {
                                            if (!line->processed) {
                                                int f_reg =  line->words[2].i_arg[0];
                                                int offset  = line->words[3].i_arg[0];
                                                int base  = line->words[3].i_arg[1];
                                                int mem_address = -1;
                                                if (line->words[4].i_arg[0] == -1) {
                                                    mem_address =  (offset + base) % cpu->address_count;

                                                } else {
                                                    int i_reg = (offset + base) % I_REGS;
                                                    mem_address = cpu->regs.i[i_reg].val % cpu->address_count;
                                                }

                                                if (!cache_check(cpu, mem_address)) {
                                                    insert_cache(cpu, mem_address);
                                                    line->words[1].i_arg[0] = 3;
                                                }

                                                if (line->words[1].i_arg[0] > 0) --line->words[1].i_arg[0];

                                                if (0 >= line->words[1].i_arg[0]) {
                                                    cpu->mem[mem_address].val = cpu->regs.fp[f_reg].val ;
                                                    line->processed = true;
                                                }
                                            }
                                        }break;
                          case STORE_W: {
                                            if (!line->processed) {
                                                int dest_i_reg =  line->words[2].i_arg[0];
                                                int offset  = line->words[3].i_arg[0];
                                                int base  = line->words[3].i_arg[1];
                                                int mem_address = -1;
                                                if (line->words[4].i_arg[0] == -1) {
                                                    mem_address =  (offset + base) % cpu->address_count;

                                                } else {
                                                    int i_reg = (offset + base) % I_REGS;
                                                    mem_address = cpu->regs.i[i_reg].val % cpu->address_count;
                                                }

                                                if (!cache_check(cpu, mem_address)) {
                                                    insert_cache(cpu, mem_address);
                                                    line->words[1].i_arg[0] = 3;
                                                }

                                                if (line->words[1].i_arg[0] > 0) --line->words[1].i_arg[0];

                                                if (0 >= line->words[1].i_arg[0]) {
                                                    cpu->mem[mem_address].val = cpu->regs.i[dest_i_reg].val ;
                                                    line->processed = true;
                                                }
                                            }
                                        }break;
                          case SUB:
                          case ADD: {
                                        if (!line->processed) {
                                            int dest_i_reg = line->words[2].i_arg[0];
                                            int arg1_i_reg = line->words[3].i_arg[0];
                                            int arg2_i_reg = line->words[4].i_arg[0];
                                            cpu->regs.i[dest_i_reg].write = true;
                                            cpu->regs.i[dest_i_reg].read = true;
                                            cpu->regs.i[arg1_i_reg].write = true;
                                            cpu->regs.i[arg1_i_reg].read = true;
                                            cpu->regs.i[arg2_i_reg].write = true;
                                            cpu->regs.i[arg2_i_reg].read = true;
                                            line->processed = true;
                                        }
                                    }break;
                          case ADD_IMM: {
                                            if (!line->processed) {
                                                int dest_i_reg = line->words[2].i_arg[0];
                                                int arg1_i_reg = line->words[3].i_arg[0];
                                                cpu->regs.i[dest_i_reg].write = true;
                                                cpu->regs.i[dest_i_reg].read = true;
                                                cpu->regs.i[arg1_i_reg].write = true;
                                                cpu->regs.i[arg1_i_reg].read = true;
                                                line->processed = true;
                                            }
                                        }break;
                          case SUB_D:
                          case MUL_D:
                          case DIV_D:
                          case ADD_D: {
                                          int dest_reg = line->words[2].i_arg[0];
                                          int arg1_reg = line->words[3].i_arg[0];
                                          int arg2_reg = line->words[4].i_arg[0];
                                          cpu->regs.fp[dest_reg].write = true;
                                          cpu->regs.fp[dest_reg].read = true;
                                          cpu->regs.fp[arg1_reg].write = true;
                                          cpu->regs.fp[arg1_reg].read = true;
                                          cpu->regs.fp[arg2_reg].write = true;
                                          cpu->regs.fp[arg2_reg].read = true;
                                          line->processed = true;
                                      }break;
                          case UNCONDITIONAL_JUMP:
                          case BEQ:
                          case BNE: {
                                        line->processed = true;
                                    }break;
                          default:break;
                      }

                      if (!line->stall) log_table(arena, table, line, line->current_stage, cpu->cycles, (*i_queue)->row);
                      else log_table(arena, table, line, STALL, cpu->cycles, (*i_queue)->row);
                  }break;
        case WB: {

                     if (line->processed) {
                         line->stall = false;
                         line->current_stage = FIN;
                         cpu->curr_stage_active[4] =  false;
                         goto FIN_EXECUTE;
                     }

                     line->stall = true;
WB_EXECUTE:
                     switch (line->words[1].type) {
                         case LOAD_IMM: {
                                            if (!line->processed) {
                                                line->processed = true;
                                            }
                                        }break;
                         case LOAD_D: {

                                          if (!line->processed) {

                                              int f_reg =  line->words[2].i_arg[0];
                                              int offset  = line->words[3].i_arg[0];
                                              int base  = line->words[3].i_arg[1];
                                              cpu->regs.fp[f_reg].write = true;
                                              cpu->regs.fp[f_reg].read = true;
                                              if (line->words[4].i_arg[0] == -1) {
                                                  int mem_address =  (offset + base) % cpu->address_count;
                                                  cpu->mem[mem_address].write = true;
                                                  cpu->mem[mem_address].read = true;
                                              } else {
                                                  int i_reg = (offset + base) % I_REGS;
                                                  cpu->regs.i[i_reg].write = true;
                                                  cpu->regs.i[i_reg].read =  true;
                                                  int mem_address = cpu->regs.i[i_reg].val % cpu->address_count;
                                                  cpu->mem[mem_address].write = true;
                                                  cpu->mem[mem_address].read = true;
                                              }

                                              line->processed = true;
                                          }
                                      }break;

                         case LOAD_W: {

                                          if (!line->processed) {
                                              int dest_i_reg =  line->words[2].i_arg[0];
                                              int offset  = line->words[3].i_arg[0];
                                              int base  = line->words[3].i_arg[1];
                                              cpu->regs.i[dest_i_reg].write = true;
                                              cpu->regs.i[dest_i_reg].read = true;
                                              if (line->words[4].i_arg[0] == -1) {
                                                  int mem_address =  (offset + base) % cpu->address_count;
                                                  cpu->mem[mem_address].write = true;
                                                  cpu->mem[mem_address].read = true;
                                              } else {
                                                  int i_reg = (offset + base) % I_REGS;
                                                  cpu->regs.i[i_reg].write = true;
                                                  cpu->regs.i[i_reg].read =  true;
                                                  int mem_address = cpu->regs.i[i_reg].val % cpu->address_count;
                                                  cpu->mem[mem_address].write = true;
                                                  cpu->mem[mem_address].read = true;
                                              }

                                              line->processed = true;

                                          }


                                      }break;
                         case STORE_D: {

                                           if (!line->processed) {
                                               int f_reg =  line->words[2].i_arg[0];
                                               int offset  = line->words[3].i_arg[0];
                                               int base  = line->words[3].i_arg[1];
                                               cpu->regs.fp[f_reg].write = true;
                                               cpu->regs.fp[f_reg].read = true;
                                               if (line->words[4].i_arg[0] == -1) {
                                                   int mem_address =  (offset + base) % cpu->address_count;
                                                   cpu->mem[mem_address].write = true;
                                                   cpu->mem[mem_address].read = true;
                                               } else {
                                                   int i_reg = (offset + base) % I_REGS;
                                                   cpu->regs.i[i_reg].write = true;
                                                   cpu->regs.i[i_reg].read =  true;
                                                   int mem_address = cpu->regs.i[i_reg].val % cpu->address_count;
                                                   cpu->mem[mem_address].write = true;
                                                   cpu->mem[mem_address].read = true;
                                               }

                                               line->processed = true;
                                           }
                                       }break;
                         case STORE_W: {

                                           if (!line->processed) {
                                               int dest_i_reg =  line->words[2].i_arg[0];
                                               int offset  = line->words[3].i_arg[0];
                                               int base  = line->words[3].i_arg[1];
                                               cpu->regs.i[dest_i_reg].write = true;
                                               cpu->regs.i[dest_i_reg].read = true;
                                               if (line->words[4].i_arg[0] == -1) {
                                                   int mem_address =  (offset + base) % cpu->address_count;
                                                   cpu->mem[mem_address].write = true;
                                                   cpu->mem[mem_address].read = true;
                                               } else {
                                                   int i_reg = (offset + base) % I_REGS;
                                                   cpu->regs.i[i_reg].write = true;
                                                   cpu->regs.i[i_reg].read =  true;
                                                   int mem_address = cpu->regs.i[i_reg].val % cpu->address_count;
                                                   cpu->mem[mem_address].write = true;
                                                   cpu->mem[mem_address].read = true;
                                               }

                                               line->processed = true;
                                           }




                                       }break;
                         case SUB:
                         case ADD: 
                         case ADD_IMM:
                         case ADD_D:
                         case SUB_D:
                         case MUL_D:
                         case DIV_D:
                         case UNCONDITIONAL_JUMP:
                         case BEQ:
                         case BNE: {
                                       if (!line->processed) {
                                           line->processed = true;
                                       }
                                   }break;
                         default:break;
                     }





                     if (!line->stall) log_table(arena, table, line, line->current_stage, cpu->cycles, (*i_queue)->row);
                     else log_table(arena, table, line, STALL, cpu->cycles, (*i_queue)->row);
                 }break;
        default: {
                 }

    }
FIN_EXECUTE:
    return;
}





Log_Table * process_instructions(Arena *arena, Cpu *cpu, Mip_File_Info *mip_file_info) {
    Log_Table *table = push_struct(arena->head, Log_Table);
    init_log_table(arena, table, LOG_TABLE_ROW, LOG_TABLE_COL);

    cpu->total_lines = mip_file_info->rows;
    I_Queue *i_queue  = create_i_queue(arena, 0, 0, 2 * mip_file_info->rows);
    i_queue->processed_lines = 0;
    for (int i = 0; i <original_instruction_count; ++i) {
        original_instruction_lines[i].table_row = -1;
    }

    while (1) {

        if (cpu->pc < original_instruction_count) {
            insert_i_queue(arena, &i_queue, &original_instruction_lines[cpu->pc]);
            ++cpu->pc;
        }
        Words *line = &i_queue->lines[i_queue->row];

        if (line->current_stage == FIN) {
            ++i_queue->processed_lines;
        }  else execute_instruction(arena, &table, &i_queue, cpu, line);

        ++i_queue->row;

        if (i_queue->row >=  i_queue->size) {
            if (i_queue->processed_lines == i_queue->size) break;
            i_queue->processed_lines = 0;
            ++cpu->cycles;
            i_queue->row = 0;
        }
    }


    return table;
}


typedef struct Char_Buffer Char_Buffer ;
struct Char_Buffer {
    char *data;
    int size;
    int cap;
};

void init_buffer(Arena *arena, Char_Buffer *buff, int cap) {
    buff->cap = cap;
    buff->size = 0;
    buff->data = push_array(arena->head, char, cap);
}

void write_buffer_char(Arena *arena , Char_Buffer **buffer, char c) {
    if ((*buffer)->size == (*buffer)->cap) {
        Char_Buffer *new_buffer = push_struct(arena->head,  Char_Buffer);
        init_buffer(arena, new_buffer, (*buffer)->cap * 2);
        new_buffer->size = (*buffer)->size;
        for (int i = 0; i <(*buffer)->size; ++i) {
            new_buffer->data[i] = (*buffer)->data[i];
        }
        (*buffer) = new_buffer;
    }
    (*buffer)->data[(*buffer)->size++] = c;
}


void write_buffer_str(Arena *arena, Char_Buffer **buffer, char * str, int len) {
    for (int i = 0; i <len; ++i) {
        write_buffer_char(arena, buffer, str[i]);
    } 
}





void write_table_file(Arena *arena, Cpu *cpu, Log_Table *table, char *file_name) {
    Char_Buffer *buffer = push_struct(arena->head, Char_Buffer);
    init_buffer(arena, buffer, 512);

    FILE *file = fopen(file_name, "w");
    int instr_len = 0;
    for (int i = 0; i <table->row_size; ++i) {
        int len =  strlen(table->str[i]);
        int tab_add = 0;
        for (int c =0; c < strlen(table->str[i]); ++c) {
            if (table->str[i][c] == '\t') {
                tab_add += 3;
                table->str[i][c] = ' ';
            }
        }
        len += tab_add;
        if ( len > instr_len) instr_len = len;
    }
    char *instr  = push_array(arena->head, char*, instr_len + 1);
#define stage_len 8
    char stage[stage_len];
    char col_div = '|';


    char *instr_header = "Instructions";
    copy_str_pad(instr, instr_header, instr_len, ' ');
    write_buffer_str(arena, &buffer, instr, instr_len);
    write_buffer_char(arena, &buffer, col_div);
    for (int col = 0; col <cpu->cycles; ++col) {
        int digits = 0;
        snprintf(stage, stage_len, "%d", col);
        while (digits < stage_len) {
            if (stage[digits] == '\0') {
                stage[digits] = ' ';
                break;
            }
            ++digits;
        }
        for (int i = digits; i < stage_len; ++i) {
            stage[i] = ' ';
        }
        write_buffer_str(arena, &buffer, stage, stage_len);
        write_buffer_char(arena, &buffer, col_div);
    }
    write_buffer_char(arena, &buffer, '\n');

    char *if_str = "IF";
    char *id_str = "ID";
    char *ex_str = "EX";
    char *mem_str = "MEM";
    char *wb_str = "WB";
    char *stall_str = "STALL";

    char *add1 = "A1";
    char *add2 = "A2";
    char *mull1 = "M1";
    char *mull2 = "M2";
    char *mull3 = "M3";
    char *mull4 = "M4";
    char *mull5 = "M5";
    char *mull6 = "M6";
    char *mull7 = "M7";
    char *mull8 = "M8";
    char *mull9 = "M9";
    char *mull10 ="M10";

    char *div1  = "D1";
    char *div2  = "D2";
    char *div3  = "D3";
    char *div4  = "D4";
    char *div5  = "D5";
    char *div6  = "D6";
    char *div7  = "D7";
    char *div8  = "D8";
    char *div9  = "D9";
    char *div10 = "D10";

    char *div11 = "D11";
    char *div12 = "D12";
    char *div13 = "D13";
    char *div14 = "D14";
    char *div15 = "D15";
    char *div16 = "D16";
    char *div17 = "D17";
    char *div18 = "D18";
    char *div19 = "D19";
    char *div20 = "D20";

    char *div21 = "D21";
    char *div22 = "D22";
    char *div23 = "D23";
    char *div24 = "D24";
    char *div25 = "D25";
    char *div26 = "D26";
    char *div27 = "D27";
    char *div28 = "D28";
    char *div29 = "D29";
    char *div30 = "D30";

    char *div31 = "D31";
    char *div32 = "D32";
    char *div33 = "D33";
    char *div34 = "D34";
    char *div35 = "D35";
    char *div36 = "D36";
    char *div37 = "D37";
    char *div38 = "D38";
    char *div39 = "D39";
    char *div40 = "D40";

    for (int row = 0; row < table->row_size; ++row) {
        char *curr_instr = table->str[row];
        copy_str_pad(instr, curr_instr, instr_len, ' ');
        write_buffer_str(arena, &buffer, instr, instr_len);
        write_buffer_char(arena, &buffer, col_div);

        for (int col = 0; col <cpu->cycles; ++col) {
            switch (table->cells[(table->col_cap * row) + col].stage) {
                case IF: {
                             copy_str_pad(stage, if_str, stage_len, ' ');
                         }break;
                case ID: {

                             copy_str_pad(stage, id_str, stage_len, ' ');
                         }break;
                case EX: {
                             Ex_Type ex_type = table->cells[(table->col_cap * row) + col].ex_type;
                             switch (ex_type) 
                             {
                                 case TYPE_EX: {
                                                   copy_str_pad(stage, ex_str, stage_len, ' ');
                                               }break;
                                 case TYPE_ADD1: {
                                                     copy_str_pad(stage, add1, stage_len, ' ');
                                                 }break;
                                 case TYPE_ADD2: {
                                                     copy_str_pad(stage, add2, stage_len, ' ');
                                                 }break;


                                 case TYPE_MUL1: {
                                                     copy_str_pad(stage, mull1, stage_len, ' ');
                                                 }break;
                                 case TYPE_MUL2: {
                                                     copy_str_pad(stage, mull2, stage_len, ' ');
                                                 }break;
                                 case TYPE_MUL3: {
                                                     copy_str_pad(stage, mull3, stage_len, ' ');
                                                 }break;
                                 case TYPE_MUL4: {
                                                     copy_str_pad(stage, mull4, stage_len, ' ');
                                                 }break;
                                 case TYPE_MUL5: {
                                                     copy_str_pad(stage, mull5, stage_len, ' ');
                                                 }break;
                                 case TYPE_MUL6: {
                                                     copy_str_pad(stage, mull6, stage_len, ' ');
                                                 }break;
                                 case TYPE_MUL7: {
                                                     copy_str_pad(stage, mull7, stage_len, ' ');
                                                 }break;
                                 case TYPE_MUL8: {
                                                     copy_str_pad(stage, mull8, stage_len, ' ');
                                                 }break;
                                 case TYPE_MUL9: {
                                                     copy_str_pad(stage, mull9, stage_len, ' ');
                                                 }break;
                                 case TYPE_MUL10: {
                                                      copy_str_pad(stage, mull10, stage_len, ' ');
                                                  }break;

                                 case TYPE_DIV1: {
                                                     copy_str_pad(stage, div1, stage_len, ' ');
                                                 }break;
                                 case TYPE_DIV2: {
                                                     copy_str_pad(stage, div2, stage_len, ' ');
                                                 }break;
                                 case TYPE_DIV3: {
                                                     copy_str_pad(stage, div3, stage_len, ' ');
                                                 }break;
                                 case TYPE_DIV4: {
                                                     copy_str_pad(stage, div4, stage_len, ' ');
                                                 }break;
                                 case TYPE_DIV5: {
                                                     copy_str_pad(stage, div5, stage_len, ' ');
                                                 }break;
                                 case TYPE_DIV6: {
                                                     copy_str_pad(stage, div6, stage_len, ' ');
                                                 }break;
                                 case TYPE_DIV7: {
                                                     copy_str_pad(stage, div7, stage_len, ' ');
                                                 }break;
                                 case TYPE_DIV8: {
                                                     copy_str_pad(stage, div8, stage_len, ' ');
                                                 }break;
                                 case TYPE_DIV9: {
                                                     copy_str_pad(stage, div9, stage_len, ' ');
                                                 }break;
                                 case TYPE_DIV10: {
                                                      copy_str_pad(stage, div10, stage_len, ' ');
                                                  }break;
                                 case TYPE_DIV11: {
                                                      copy_str_pad(stage, div11, stage_len, ' ');
                                                  }break;
                                 case TYPE_DIV12: {
                                                      copy_str_pad(stage, div12, stage_len, ' ');
                                                  }break;
                                 case TYPE_DIV13: {
                                                      copy_str_pad(stage, div13, stage_len, ' ');
                                                  }break;
                                 case TYPE_DIV14: {
                                                      copy_str_pad(stage, div14, stage_len, ' ');
                                                  }break;
                                 case TYPE_DIV15: {
                                                      copy_str_pad(stage, div15, stage_len, ' ');
                                                  }break;
                                 case TYPE_DIV16: {
                                                      copy_str_pad(stage, div16, stage_len, ' ');
                                                  }break;
                                 case TYPE_DIV17: {
                                                      copy_str_pad(stage, div17, stage_len, ' ');
                                                  }break;
                                 case TYPE_DIV18: {
                                                      copy_str_pad(stage, div18, stage_len, ' ');
                                                  }break;
                                 case TYPE_DIV19: {
                                                      copy_str_pad(stage, div19, stage_len, ' ');
                                                  }break;
                                 case TYPE_DIV20: {
                                                      copy_str_pad(stage, div20, stage_len, ' ');
                                                  }break;
                                 case TYPE_DIV21: {
                                                      copy_str_pad(stage, div21, stage_len, ' ');
                                                  }break;
                                 case TYPE_DIV22: {
                                                      copy_str_pad(stage, div22, stage_len, ' ');
                                                  }break;
                                 case TYPE_DIV23: {
                                                      copy_str_pad(stage, div23, stage_len, ' ');
                                                  }break;
                                 case TYPE_DIV24: {
                                                      copy_str_pad(stage, div24, stage_len, ' ');
                                                  }break;
                                 case TYPE_DIV25: {
                                                      copy_str_pad(stage, div25, stage_len, ' ');
                                                  }break;
                                 case TYPE_DIV26: {
                                                      copy_str_pad(stage, div26, stage_len, ' ');
                                                  }break;
                                 case TYPE_DIV27: {
                                                      copy_str_pad(stage, div27, stage_len, ' ');
                                                  }break;
                                 case TYPE_DIV28: {
                                                      copy_str_pad(stage, div28, stage_len, ' ');
                                                  }break;
                                 case TYPE_DIV29: {
                                                      copy_str_pad(stage, div29, stage_len, ' ');
                                                  }break;
                                 case TYPE_DIV30: {
                                                      copy_str_pad(stage, div30, stage_len, ' ');
                                                  }break;
                                 case TYPE_DIV31: {
                                                      copy_str_pad(stage, div31, stage_len, ' ');
                                                  }break;
                                 case TYPE_DIV32: {
                                                      copy_str_pad(stage, div32, stage_len, ' ');
                                                  }break;
                                 case TYPE_DIV33: {
                                                      copy_str_pad(stage, div33, stage_len, ' ');
                                                  }break;
                                 case TYPE_DIV34: {
                                                      copy_str_pad(stage, div34, stage_len, ' ');
                                                  }break;
                                 case TYPE_DIV35: {
                                                      copy_str_pad(stage, div35, stage_len, ' ');
                                                  }break;
                                 case TYPE_DIV36: {
                                                      copy_str_pad(stage, div36, stage_len, ' ');
                                                  }break;
                                 case TYPE_DIV37: {
                                                      copy_str_pad(stage, div37, stage_len, ' ');
                                                  }break;
                                 case TYPE_DIV38: {
                                                      copy_str_pad(stage, div38, stage_len, ' ');
                                                  }break;
                                 case TYPE_DIV39: {
                                                      copy_str_pad(stage, div39, stage_len, ' ');
                                                  }break;
                                 case TYPE_DIV40: {
                                                      copy_str_pad(stage, div40, stage_len, ' ');
                                                  }break;





                             }
                         }break;
                case MEM: {
                              copy_str_pad(stage, mem_str, stage_len, ' ');
                          }break;
                case WB: {
                             copy_str_pad(stage, wb_str, stage_len, ' ');
                         }break;
                case STALL: {
                                copy_str_pad(stage, stall_str, stage_len, ' ');
                            }break;
                default: {
                             copy_str_pad(stage, "", stage_len, ' ');
                         }
            }

            write_buffer_str(arena, &buffer, stage, stage_len);
            write_buffer_char(arena, &buffer, col_div);
        }



        write_buffer_char(arena, &buffer, '\n');
    }

    write_buffer_char(arena, &buffer, '\n');
    copy_str_pad(instr, "Location", instr_len, ' ');
    write_buffer_str(arena, &buffer, instr, instr_len);

    copy_str_pad(instr, "Memory", instr_len, ' ');
    write_buffer_str(arena, &buffer, instr, instr_len);

    copy_str_pad(instr, "Int Registers", instr_len, ' ');
    write_buffer_str(arena, &buffer, instr, instr_len);

    copy_str_pad(instr, "FP Registers", instr_len, ' ');
    write_buffer_str(arena, &buffer, instr, instr_len);
    write_buffer_char(arena, &buffer, '\n');

    bool lanes_fin[3] = {0};
    int row = 0;
    int completed_lanes = 0;
    while (completed_lanes <3) {

        int digits = 0;
        snprintf(instr, instr_len, "%d", row);
        while (digits < instr_len) {
            if (instr[digits] == '\0') {
                instr[digits] = ' ';
                break;
            }
            ++digits;
        }
        for (int i = digits; i < instr_len; ++i) {
            instr[i] = ' ';
        }
        write_buffer_str(arena, &buffer, instr, instr_len);
        write_buffer_char(arena, &buffer, col_div);


        if (!lanes_fin[0]) {
            if (row >= cpu->address_count) {
                lanes_fin[0] = true;
                ++completed_lanes;
                copy_str_pad(instr, "", instr_len, ' ');
                write_buffer_str(arena, &buffer, instr, instr_len);
                write_buffer_char(arena, &buffer, col_div);
            }
            else {

                int val = cpu->mem[row].val;
                int digits = 0;
                snprintf(instr, instr_len, "%d", val);
                while (digits < instr_len) {
                    if (instr[digits] == '\0') {
                        instr[digits] = ' ';
                        break;
                    }
                    ++digits;
                }
                for (int i = digits; i < instr_len; ++i) {
                    instr[i] = ' ';
                }
                write_buffer_str(arena, &buffer, instr, instr_len);
                write_buffer_char(arena, &buffer, col_div);



            }
        } else {
            copy_str_pad(instr, "", instr_len, ' ');
            write_buffer_str(arena, &buffer, instr, instr_len);
            write_buffer_char(arena, &buffer, col_div);
        }

        if (!lanes_fin[1]) {
            if (row >= I_REGS) {
                lanes_fin[1] = true;
                ++completed_lanes;
                copy_str_pad(instr, "", instr_len, ' ');
                write_buffer_str(arena, &buffer, instr, instr_len);
                write_buffer_char(arena, &buffer, col_div);
            } else {

                int val = cpu->regs.i[row].val;
                int digits = 0;
                snprintf(instr, instr_len, "%d", val);
                while (digits < instr_len) {
                    if (instr[digits] == '\0') {
                        instr[digits] = ' ';
                        break;
                    }
                    ++digits;
                }
                for (int i = digits; i < instr_len; ++i) {
                    instr[i] = ' ';
                }
                write_buffer_str(arena, &buffer, instr, instr_len);
                write_buffer_char(arena, &buffer, col_div);



            }
        } else {
            copy_str_pad(instr, "", instr_len, ' ');
            write_buffer_str(arena, &buffer, instr, instr_len);
            write_buffer_char(arena, &buffer, col_div);
        }

        if (!lanes_fin[2]) {
            if (row >= FP_REGS) {
                lanes_fin[2] = true ;
                ++completed_lanes;
                copy_str_pad(instr, "", instr_len, ' ');
                write_buffer_str(arena, &buffer, instr, instr_len);
                write_buffer_char(arena, &buffer, col_div);
            } else {

                float val = cpu->regs.fp[row].val;
                int digits = 0;
                snprintf(instr, instr_len, "%f", val);
                while (digits < instr_len) {
                    if (instr[digits] == '\0') {
                        instr[digits] = ' ';
                        break;
                    }
                    ++digits;
                }
                for (int i = digits; i < instr_len; ++i) {
                    instr[i] = ' ';
                }
                write_buffer_str(arena, &buffer, instr, instr_len);
                write_buffer_char(arena, &buffer, col_div);
            }
        } else {
            copy_str_pad(instr, "", instr_len, ' ');
            write_buffer_str(arena, &buffer, instr, instr_len);
            write_buffer_char(arena, &buffer, col_div);
        }

        write_buffer_char(arena, &buffer, '\n');
        ++row;
    }

    fwrite(buffer->data, 1, buffer->size ,file);
    fclose(file);
}


int main(int argc, char **argv) {
    char *file_name;
    char *output_name;
    if (argc != 3) {
        printf("Usage: ./main.exe input.txt output.txt\n"
                "Using default example_text.txt");

        file_name = "example_text.txt";
    output_name = "example_out.txt";
    } else {
        file_name = argv[1];
        output_name = argv[2];
    }
    Arena *perm_arena = create_arena(ARENA_DEF_CAP);
    Arena *scratch_arena = create_arena(ARENA_DEF_CAP);
    Cpu cpu = {0};
    init_cpu(perm_arena, &cpu);

    Mip_File_Info file_info = {0};
    int input_file_lines = get_lines(file_name);
    init_file_info(&file_info, input_file_lines, perm_arena);

    get_file_info(perm_arena, scratch_arena, &cpu,  &file_info, file_name);
    original_instruction_count = file_info.rows;
    original_instruction_lines = file_info.line;

    Log_Table *table = process_instructions(perm_arena, &cpu, &file_info);

    write_table_file(scratch_arena, &cpu, table, output_name);


    free_chunks(scratch_arena->head);
    free(scratch_arena);
    free_chunks(perm_arena->head);
    free(perm_arena);
}
