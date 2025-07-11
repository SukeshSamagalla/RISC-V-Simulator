#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MAX_Labels 10000
#define MEM_VAL 0x50000

FILE *input;
int64_t PC = 0;
int64_t dummy = 0;
int64_t line_num_global = 0;
int64_t offset_global = 0;
int32_t BREAK_POINTS[10] = {-1};
int32_t num_break_points = 0;
int64_t dot_line_num=0;

void add_label(char *name, int64_t line_num, int64_t offset);
void execute_instruction(char Instruction[], int32_t id, char rd[], char rs1[], char imm_str[], char line[]);
void initialization();
void data_segregation();
void PC_check();
int32_t assign_reg_val(char reg[]);
int32_t get_instruction(char *str);
int64_t find_label_address(char *name);
int64_t get_reg_val(char reg[]);
bool break_check();
bool is_hexadecimal(char *str);

void separator(char *str);
void show_stack();
void stack_update(int64_t dummy);
void Pop_Stack();
void Push_Stack(char name[], int pos);
void Print_Memory(char mem_start_add[], char count_in_str[]);
void display_register(void); 
void run(bool flag);
void load_file(char file_name[]);
void set_break_point(int64_t line_num);
void del_break_point(int64_t line_num);

bool ADD(char rd[], char rs1[], char rs2[]);
bool SUB(char rd[], char rs1[], char rs2[]);
bool AND(char rd[], char rs1[], char rs2[]);
bool OR(char rd[], char rs1[], char rs2[]);
bool XOR(char rd[], char rs1[], char rs2[]);
bool SLL(char rd[], char rs1[], char rs2[]);
bool SRL(char rd[], char rs1[], char rs2[]);
bool SRA(char rd[], char rs1[], char rs2[]);
bool ADDI(char rd[], char rs1[], char imm_str[]);
bool ANDI(char rd[], char rs1[], char imm_str[]);
bool ORI(char rd[], char rs1[], char imm_str[]);
bool XORI(char rd[], char rs1[], char imm_str[]);
bool SLLI(char rd[], char rs1[], char imm_str[]);
bool SRLI(char rd[], char rs1[], char imm_str[]);
bool SRAI(char rd[], char rs1[], char imm_str[]);


bool LUI(char rd[], char imm_str[]);
bool LOAD(char rd[], char rs1[], char imm_str[], int decide_length, bool flag);
bool STORE(char memory_addr_value[], char storing_value[], char imm_str[], int decide_length);

bool BEQ(char rd[], char rs1[], char imm_str[]);
bool BNE(char rd[], char rs1[], char imm_str[]);
bool BLT(char rd[], char rs1[], char imm_str[]);
bool BGE(char rd[], char rs1[], char imm_str[]);
bool BLTU(char rd[], char rs1[], char imm_str[]);
bool BGEU(char rd[], char rs1[], char imm_str[]);
bool JAL(char rd[], char imm_str[]);
bool JALR(char rd[], char imm_sr[], char rs1[]);

typedef struct Instruction
{
    char name[7];
    char type;
} Instruction;

Instruction instructions[] = {

    {"add", 'R'},
    {"sub", 'R'},
    {"and", 'R'},
    {"or", 'R'},
    {"xor", 'R'},
    {"sll", 'R'},
    {"srl", 'R'},
    {"sra", 'R'},
    {"addi", 'I'},
    {"andi", 'I'},
    {"ori", 'I'},
    {"xori", 'I'},
    {"slli", 's'},
    {"srli", 's'},
    {"srai", 's'},
    {"ld", 'I'},
    {"lw", 'I'},
    {"lh", 'I'},
    {"lb", 'I'},
    {"lwu", 'I'},
    {"lhu", 'I'},
    {"lbu", 'I'},
    {"sd", 'S'},
    {"sw", 'S'},
    {"sh", 'S'},
    {"sb", 'S'},
    {"jalr", 'I'},
    {"lui", 'U'},
    {"beq", 'B'},
    {"bne", 'B'},
    {"blt", 'B'},
    {"bge", 'B'},
    {"bltu", 'B'},
    {"bgeu", 'B'},
    {"jal", 'J'},
};

unsigned int NUM_INSTRUCTIONS = (sizeof(instructions) / sizeof(instructions[0]));

typedef struct Stack_Calls
{
    char name[30][20];
    int pos_of_calling[30];
    int top_index;
} Stack_Calls;

Stack_Calls Stack;

typedef struct Register_list
{
    char reg_name_og[4];
    char reg_name_alias[5];
    int64_t reg_value;
    int64_t jal_offset;
} Register_list;

Register_list Registers[32] = {
    {"x0", "zero", 0,0},
    {"x1", "ra", 0,0},
    {"x2", "sp", 0,0},
    {"x3", "gp", 0,0},
    {"x4", "tp", 0,0},
    {"x5", "t0", 0,0},
    {"x6", "t1", 0,0},
    {"x7", "t2", 0,0},
    {"x8", "s0", 0,0},
    {"x9", "s1", 0,0},
    {"x10", "a0", 0,0},
    {"x11", "a1", 0,0},
    {"x12", "a2", 0,0},
    {"x13", "a3", 0,0},
    {"x14", "a4", 0,0},
    {"x15", "a5", 0,0},
    {"x16", "a6", 0,0},
    {"x17", "a7", 0,0},
    {"x18", "s2", 0,0},
    {"x19", "s3", 0,0},
    {"x20", "s4", 0,0},
    {"x21", "s5", 0,0},
    {"x22", "s6", 0,0},
    {"x23", "s7", 0,0},
    {"x24", "s8", 0,0},
    {"x25", "s9", 0,0},
    {"x26", "s10", 0,0},
    {"x27", "s11", 0,0},
    {"x28", "t3", 0,0},
    {"x29", "t4", 0,0},
    {"x30", "t5", 0,0},
    {"x31", "t6", 0,0},
};


typedef struct Label
{
    char name[21];
    int64_t line_num;
    int64_t offset;
} Label;

Label labels[MAX_Labels];
int32_t label_count = 0;

int8_t Memory_Values[MEM_VAL] = {0};
int32_t num_byte = -1;


void initialization()
{
    PC = 0;
    dummy = 0;
    line_num_global = 0;
    offset_global = 0;
    dot_line_num=0;
    label_count=0;
    num_byte=-1;
    for (int i = 0; i < 32; i++)
    {
        Registers[i].reg_value = 0;
        Registers[i].jal_offset=0;
    }
    for (int i = 0; i < MEM_VAL; i++)
    {
        Memory_Values[i] = 0;
    }
    for (int i = 0; i < 10; i++)
    {
        BREAK_POINTS[i] = -1;
    }
    num_break_points = 0;
    strcpy(Stack.name[0], "main");
    Stack.pos_of_calling[0] = 0;
    Stack.top_index = 0;
    return;
}

void Push_Stack(char name[], int pos)
{
    Stack.top_index++;
    strcpy(Stack.name[Stack.top_index], name);
    Stack.pos_of_calling[Stack.top_index - 1] = pos;
    return;
}

void Pop_Stack()
{
    Stack.top_index--;
    return;
}

void stack_update(int64_t dummy)
{ 
    Stack.pos_of_calling[Stack.top_index] = dummy+1;
    return;
}


void show_stack()
{
    if (dummy + 1  == line_num_global)
    {
        printf("Empty Call Stack: Execution complete\n");
    }
    else
    {
        printf("Call Stack:\n");
        if (dummy == 0)
        {
            printf("main:%ld\n",dot_line_num);
            // printf("main:0\n");
        }
        else
        {
            
            for (int i = 0; i <= Stack.top_index; i++)
            {
                printf("%s:%ld\n", Stack.name[i], Stack.pos_of_calling[i]+dot_line_num);
                //printf("%s:%d\n", Stack.name[i], Stack.pos_of_calling[i]);
            }
        }
    }
    puts("");
    return;
}


void set_break_point(int64_t line_num)
{
    BREAK_POINTS[num_break_points] = line_num-dot_line_num;
    printf("Breakpoint set at line %ld\n\n", line_num);
    num_break_points++;
    return;
}
void del_break_point(int64_t line_num)
{
    line_num=line_num-dot_line_num;
    int i;
    for (i = 0; i < 10; i++)
    {
        if (BREAK_POINTS[i] == line_num)
        {
            BREAK_POINTS[i] = -1;
            printf("Breakpoint deleted at line %ld\n\n", line_num);
            return;
        }
    }
    puts("No Break point present at this line number\n");
    return;
}

bool break_check()
{
    int i;
    for (i = 0; i < 10; i++)
    {
        if (PC + 1 == BREAK_POINTS[i])
        {
            puts("Execution Stopped at Break point\n");
            return true;
        }
    }
    return false;
}

void Print_Memory(char mem_start_add[], char count_in_str[])
{
    int start_address = strtol(mem_start_add, NULL, 16);
    int count = atoi(count_in_str);
    for (int i = 0; i < count; i++)
    {
        printf("Memory[%#5X] = 0x%02X\n", start_address + i, (uint8_t)Memory_Values[start_address + i]);
    }
    puts("");
    return;
}

int64_t find_label_address(char *name)
{
    
    for (int i = 0; i < label_count; i++)
    {
        if (strcmp(name, labels[i].name) == 0)
        {
            
            PC = labels[i].line_num;
            return labels[i].offset;
        }
    }
    return -1; 
}

bool JAL(char rd[], char imm_str[])
{
    int64_t rd_num = PC * 4 + 4;
    Registers[assign_reg_val(rd)].reg_value = rd_num;
    Push_Stack(imm_str, PC + 1);
    dummy=PC;
    stack_update(dummy);
    int64_t offset = find_label_address(imm_str);
    fseek(input, offset, SEEK_SET);

    return true;
}

bool JALR(char rd[], char imm_str[], char rs1[])
{
    char buffer[50];

    int64_t rs1_num = get_reg_val(rs1);
    int64_t rd_num = PC * 4 + 4;
    Registers[assign_reg_val(rd)].reg_value = rd_num;

    int64_t imm = (atoi(imm_str)) / 4;

    dummy =PC;
    stack_update(dummy);
    PC = (rs1_num / 4) + imm;

    int64_t dummy_use;
    for(int i=0;i<32;i++)
    {
        if((rs1_num/4)==labels[i].line_num)
        {
            Registers[assign_reg_val(rs1)].jal_offset=labels[i].offset;
        }
    }

    dummy_use=Registers[assign_reg_val(rs1)].jal_offset;

    fseek(input, dummy_use, SEEK_SET);
    if (imm > 0)
    {
        for (int i = 0; i < imm; i++)
        {
            fgets(buffer, sizeof(buffer), input);
            fseek(input, strlen(buffer), SEEK_CUR);
            dummy=PC;
            PC++;
        }
    }
    if (imm < 0)
    {
        for (int i = 0; i > imm; i--)
        {
            fgets(buffer, sizeof(buffer), input);
            fseek(input, -strlen(buffer), SEEK_CUR);
            dummy=PC;
            PC--;
        }
    }
    Pop_Stack();

    return true;
}

bool BEQ(char rd[], char rs1[], char imm_str[])
{
    int64_t rd_num = 0, rs1_num = 0;
    rd_num = get_reg_val(rd);
    rs1_num = get_reg_val(rs1);
    if (rd_num == rs1_num)
    {
        return true;
    }
    else
    {
        return false;
    }
}
bool BNE(char rd[], char rs1[], char imm_str[])
{
    int64_t rd_num = 0, rs1_num = 0;
    rd_num = get_reg_val(rd);
    rs1_num = get_reg_val(rs1);
    if (rd_num != rs1_num)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool BLT(char rd[], char rs1[], char imm_str[])
{
    int64_t rd_num = 0, rs1_num = 0;
    rd_num = get_reg_val(rd);
    rs1_num = get_reg_val(rs1);
    if (rd_num < rs1_num)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool BGE(char rd[], char rs1[], char imm_str[])
{
    int64_t rd_num = 0, rs1_num = 0;
    rd_num = get_reg_val(rd);
    rs1_num = get_reg_val(rs1);
    if (rd_num >= rs1_num)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool BLTU(char rd[], char rs1[], char imm_str[])
{
    uint64_t rd_num = 0, rs1_num = 0;
    rd_num = get_reg_val(rd);
    rs1_num = get_reg_val(rs1);
    if (rd_num < rs1_num)
    {
        return true;
    }
    else
    {
        return false;
    }
}
bool BGEU(char rd[], char rs1[], char imm_str[])
{
    uint64_t rd_num = 0, rs1_num = 0;
    rd_num = get_reg_val(rd);
    rs1_num = get_reg_val(rs1);
    if (rd_num >= rs1_num)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool LOAD(char rd[], char rs1[], char imm_str[], int decide_length, bool flag) {
    int64_t imm = atol(imm_str);
    int64_t rs1_num = get_reg_val(rs1);

    int64_t address = rs1_num + imm;
    int64_t position = address;
    int64_t rd_num = 0;

    for (int i = 0; i < decide_length; i++) {
        uint8_t byte = (uint8_t)Memory_Values[position + i];
        rd_num |= (int64_t)byte << (8 * ( i));
    }

    if (flag && decide_length == 1) {
        rd_num = (int8_t)rd_num;
    } else if (flag && decide_length == 2) {
        rd_num = (int16_t)rd_num;
    } else if (flag && decide_length == 4) {
        rd_num = (int32_t)rd_num;
    } else if (flag && decide_length == 8) {
        rd_num = (int64_t)rd_num;
    }

    Registers[assign_reg_val(rd)].reg_value = rd_num;
    return true;
}


void load_file(char file_name[])
{
    file_name[strcspn(file_name, "\n")] = 0;
    input = fopen(file_name, "r");

    if (input == NULL)
    {
        puts("Error opening file: No such file or directory");
        return;
    }
    char line[200];
    while(fgets(line,sizeof(line),input))
    {
        if(line[0]=='.')
        {

            dot_line_num++;
        }
    }

    rewind(input);
    data_segregation();
    while (fgets(line, sizeof(line), input))
    {
        if (line[0] == '.')
        {
            offset_global += strlen(line);

            continue;
        
        }
        char label[21];
        if(strchr(line, ':') != NULL)
        {
            sscanf(line, "%[^:]:", label);
        }
        else
        {
            sscanf(line, "%[^ ] ",label);
        }
        add_label(label, line_num_global, offset_global);
        line_num_global += 1;
        offset_global += strlen(line);

    } 
    rewind(input);

    return;
}

void run(bool flag)
{
    char line[50] = "";
    if (input == NULL)
    {
        puts("Error opening file: No such file or directory");
        return;
    }
    while (fgets(line, sizeof(line), input))
    {
        Registers[0].reg_value=0;
        if (line[0] == '.')
            continue;

        else
        {
            char instruction[7] = {0}, rd[5] = {0}, rs1[5] = {0}, imm_str[21] = {0}, label[21] = {0};
            bool offset = strchr(line, '(') != NULL;
            bool colon = strchr(line, ':') != NULL;
            bool lui = strstr(line, "lui") != NULL;
            bool jal = strstr(line, "jal ") != NULL;

            if (colon)
            {
                if (offset)
                    sscanf(line, "%[^:]: %[^ ] %[^,], %[^(](%[^)])", label, instruction, rd, imm_str, rs1);

                else if (lui || jal)
                    sscanf(line, "%[^:]: %[^ ] %[^,], %s", label, instruction, rd, imm_str);

                else
                    sscanf(line, "%[^:]: %[^ ] %[^,], %[^,], %s", label, instruction, rd, rs1, imm_str);
            }
            else
            {
                if (offset)
                    sscanf(line, "%s %[^,], %[^(](%[^)])", instruction, rd, imm_str, rs1);

                else if (lui || jal)
                    sscanf(line, "%s %[^,], %s", instruction, rd, imm_str);

                else
                    sscanf(line, "%s %[^,], %[^,], %s", instruction, rd, rs1, imm_str);
            }

            int32_t instruction_id = get_instruction(instruction);
            if (instruction_id != -1)
                execute_instruction(instruction, instruction_id, rd, rs1, imm_str, line);

            
            bool check_break = break_check();
            if (!flag || check_break)
            {
                break;
            }
        }
        Registers[0].reg_value=0;

    }
    return;
}

void display_register(void)
{
    puts("Registers:");
    for (int32_t i = 0; i < 32; i++)
    {
        printf("%s = 0x%0lx\n", Registers[i].reg_name_og, Registers[i].reg_value);//, Registers[i].reg_value);
    }
    puts("");
    return;
}

void add_label(char *name, int64_t line_num, int64_t offset)
{
    strcpy(labels[label_count].name, name);
    labels[label_count].line_num = line_num;
    labels[label_count].offset = offset;
    label_count++;
    return;
}

void execute_instruction(char Instruction[], int32_t id, char rd[], char rs1[], char imm_str[], char line[])
{
    line[strcspn(line, "\n")] = 0;
    switch (id)
    {
    case 0:
    {
        bool done = ADD(rd, rs1, imm_str);

        if (done)
            printf("Executed %s;\t", line);

        printf("PC=%08lx\n\n", PC * 4);
        PC_check();
        break;
    }
    case 1:
    {
        bool done = SUB(rd, rs1, imm_str);

        if (done)
            printf("Executed %s;\t", line);

        printf("PC=%08lx\n\n", PC * 4);
        PC_check();
        break;
    }
    case 2:
    {
        bool done = AND(rd, rs1, imm_str);

        if (done)
            printf("Executed %s;\t", line);

        printf("PC=%08lx\n\n", PC * 4);
        PC_check();
        break;
    }
    case 3:
    {
        bool done = OR(rd, rs1, imm_str);

        if (done)
            printf("Executed %s;\t", line);

        printf("PC=%08lx\n\n", PC * 4);
        PC_check();
        break;
    }
    case 4:
    {
        bool done = XOR(rd, rs1, imm_str);

        if (done)
            printf("Executed %s;\t", line);

        printf("PC=%08lx\n\n", PC * 4);
        PC_check();
        break;
    }
    case 5:
    {
        bool done = SLL(rd, rs1, imm_str);

        if (done)
            printf("Executed %s;\t", line);

        printf("PC=%08lx\n\n", PC * 4);
        PC_check();
        break;
    }
    case 6:
    {
        bool done = SRL(rd, rs1, imm_str);

        if (done)
            printf("Executed %s;\t", line);

        printf("PC=%08lx\n\n", PC * 4);
        PC_check();
        break;
    }
    case 7:
    {
        bool done = SRA(rd, rs1, imm_str);

        if (done)
            printf("Executed %s;\t", line);

        printf("PC=%08lx\n\n", PC * 4);
        PC_check();
        break;
    }
    case 8:
    {
        bool done = ADDI(rd, rs1, imm_str);

        if (done)
            printf("Executed %s;\t", line);

        printf("PC=%08lx\n\n", PC * 4);
        PC_check();
        break;
    }
    case 9:
    {
        bool done = ANDI(rd, rs1, imm_str);

        if (done)
            printf("Executed %s;\t", line);

        printf("PC=%08lx\n\n", PC * 4);
        PC_check();
        break;
    }
    case 10:
    {
        bool done = ORI(rd, rs1, imm_str);

        if (done)
            printf("Executed %s;\t", line);

        printf("PC=%08lx\n\n", PC * 4);
        PC_check();
        break;
    }
    case 11:
    {
        bool done = XORI(rd, rs1, imm_str);

        if (done)
            printf("Executed %s;\t", line);

        printf("PC=%08lx\n\n", PC * 4);
        PC_check();
        break;
    }
    case 12:
    {
        bool done = SLLI(rd, rs1, imm_str);

        if (done)
            printf("Executed %s;\t", line);

        printf("PC=%08lx\n\n", PC * 4);
        PC_check();
        break;
    }
    case 13:
    {
        bool done = SRLI(rd, rs1, imm_str);

        if (done)
            printf("Executed %s;\t", line);

        printf("PC=%08lx\n\n", PC * 4);
        PC_check();
        break;
    }
    case 14:
    {
        bool done = SRAI(rd, rs1, imm_str);

        if (done)
            printf("Executed %s;\t", line);

        printf("PC=%08lx\n\n", PC * 4);
        PC_check();
        break;
    }
    case 15:
    {
        bool done = LOAD(rd, rs1, imm_str, 8, true);

        if (done)
            printf("Executed %s;\t", line);

        printf("PC=%08lx\n\n", PC * 4);
        PC_check();
        break;
    }
    case 16:
    {
        bool done = LOAD(rd, rs1, imm_str, 4, true);

        if (done)
            printf("Executed %s;\t", line);

        printf("PC=%08lx\n\n", PC * 4);
        PC_check();
        break;
    }
    case 17:
    {
        bool done = LOAD(rd, rs1, imm_str, 2, true);

        if (done)
            printf("Executed %s;\t", line);

        printf("PC=%08lx\n\n", PC * 4);
        PC_check();
        break;
    }
    case 18:
    {
        bool done = LOAD(rd, rs1, imm_str, 1, true);

        if (done)
            printf("Executed %s;\t", line);

        printf("PC=%08lx\n\n", PC * 4);
        PC_check();
        break;
    }
    case 19:
    {
        bool done = LOAD(rd, rs1, imm_str, 4, false);

        if (done)
            printf("Executed %s;\t", line);

        printf("PC=%08lx\n\n", PC * 4);
        PC_check();
        break;
    }
    case 20:
    {
        bool done = LOAD(rd, rs1, imm_str, 2, false);

        if (done)
            printf("Executed %s;\t", line);

        printf("PC=%08lx\n\n", PC * 4);
        PC_check();
        break;
    }
    case 21:
    {
        bool done = LOAD(rd, rs1, imm_str, 1, false);

        if (done)
            printf("Executed %s;\t", line);

        printf("PC=%08lx\n\n", PC * 4);
        PC_check();
        break;
    }
    case 22:
    {
        bool done = STORE(rs1, rd, imm_str, 8);

        if (done)
            printf("Executed %s;\t", line);

        printf("PC=%08lx\n\n", PC * 4);
        PC_check();
        break;
    }
    case 23:
    {
        bool done = STORE(rs1, rd, imm_str, 4);

        if (done)
            printf("Executed %s;\t", line);

        printf("PC=%08lx\n\n", PC * 4);
        PC_check();
        break;
    }
    case 24:
    {
        bool done = STORE(rs1, rd, imm_str, 2);

        if (done)
            printf("Executed %s;\t", line);

        printf("PC=%08lx\n\n", PC * 4);
        PC_check();
        break;
    }
    case 25:
    {
        bool done = STORE(rs1, rd, imm_str, 1);

        if (done)
            printf("Executed %s;\t", line);

        printf("PC=%08lx\n\n", PC * 4);
        PC_check();
        break;
    }
    case 26:
    {
        printf("Executed %s;\t", line);
        printf("PC=%08lx\n\n", PC * 4);
        bool done = JALR(rd, imm_str, rs1);
        break;
    }
    case 27:
    {
        bool done = LUI(rd, imm_str);

        if (done)
            printf("Executed %s;\t", line);

        printf("PC=%08lx\n\n", PC * 4);
        PC_check();
        break;
    }
    case 28:
    {
        bool done = BEQ(rd, rs1, imm_str);
        printf("Executed %s;\t", line);
        printf("PC=%08lx\n\n", PC * 4);
        if (done)
        {
            dummy=PC;
            stack_update(dummy);

            int64_t offset = find_label_address(imm_str);
            fseek(input, offset, SEEK_SET);

        }

        else
        {
            PC_check();
        }
        break;
    }
    case 29:
    {
        bool done = BNE(rd, rs1, imm_str);
        printf("Executed %s;\t", line);
        printf("PC=%08lx\n\n", PC * 4);
        if (done)
        {
            dummy=PC;
            stack_update(dummy);

            int64_t offset = find_label_address(imm_str);
            fseek(input, offset, SEEK_SET);

        }

        else
        {
            PC_check();
        }
        break;
    }
    case 30:
    {
        bool done = BLT(rd, rs1, imm_str);
        printf("Executed %s;\t", line);
        printf("PC=%08lx\n\n", PC * 4);
        if (done)
        {
            dummy=PC;
            stack_update(dummy);
            int64_t offset = find_label_address(imm_str);
            fseek(input, offset, SEEK_SET);
        }

        else
        {
            PC_check();
        }
        break;
    }
    case 31:
    {
        bool done = BGE(rd, rs1, imm_str);
        printf("Executed %s;\t", line);
        printf("PC=%08lx\n\n", PC * 4);
        if (done)
        {
            dummy=PC;
            stack_update(dummy);
            int64_t offset = find_label_address(imm_str);
            fseek(input, offset, SEEK_SET);
        }

        else
        {
            PC_check();
        }
        break;
    }
    case 32:
    {
        bool done = BLTU(rd, rs1, imm_str);
        printf("Executed %s;\t", line);
        printf("PC=%08lx\n\n", PC * 4);
        if (done)
        {
            dummy=PC;
            stack_update(dummy);
            int64_t offset = find_label_address(imm_str);

            fseek(input, offset, SEEK_SET);

        }

        else
        {
            PC_check();
        }
        break;
    }
    case 33:
    {
        bool done = BGEU(rd, rs1, imm_str);
        printf("Executed %s;\t", line);
        printf("PC=%08lx\n\n", PC * 4);
        if (done)
        {
            dummy=PC;
            stack_update(dummy);
            int64_t offset = find_label_address(imm_str);

            fseek(input, offset, SEEK_SET);
        }

        else
        {
            PC_check();
        }
        break;
    }
    case 34:
    {
        printf("Executed %s;\t", line);
        printf("PC=%08lx\n\n", PC * 4);
        bool done = JAL(rd, imm_str);

        break;
    }
    default:
    {
        PC_check();
        break;
    }
    }
    return;
}

void PC_check()
{
    dummy=PC;
    stack_update(dummy);
    if (PC == line_num_global - 1)
    {
        //puts("End OF File\n");
        return;
    }
    PC = PC + 1;
    return;
}

void data_segregation()
{
    char line[200];
    fgets(line, sizeof(line), input);
    if (line[0] == '.')
    {
        offset_global += strlen(line);

        if (strstr(line, "data") != NULL)
        {
            fgets(line, sizeof(line), input);
            offset_global += strlen(line);
            do
            {
                int32_t count = 0;
                char *token = strtok(line, ", ");
                switch (*(token + 1))
                {
                case 'd':
                    count = 8;
                    break;
                case 'w':
                    count = 4;
                    break;
                case 'h':
                    count = 2;
                    break;
                case 'b':
                    count = 1;
                    break;
                default:
                    break;
                }
                token = strtok(NULL, ", ");
                while (token != NULL)
                {
                    int64_t num = 0;
                    num = is_hexadecimal(token) ? strtol(token, NULL, 16) : atol(token);
                    for (int32_t i = 0; i < count; i++)
                    {
                        int8_t byte_n = (int8_t)(0xFF & num);
                        num_byte++;
                        Memory_Values[num_byte+0x10000] = byte_n;
                        num = num >> 8;
                    }
                    token = strtok(NULL, ", \n");
                }
                fgets(line, sizeof(line), input);
                offset_global += strlen(line);
            } while (strstr(line, ".text") == NULL);
        }
        else if (strstr(line, "text") != NULL)
        {
        }
    }
    else
    {
        rewind(input);
    }
    return;
}

int32_t get_instruction(char *str)
{
    for (int32_t i = 0; i < NUM_INSTRUCTIONS; i++)
    {
        if (strcmp(str, instructions[i].name) == 0)
            return i;
    }
    return -1;
}

int64_t get_reg_val(char reg[])
{
    for (int32_t i = 0; i < 32; i++)
    {
        if (strcmp(reg, Registers[i].reg_name_og) == 0 || strcmp(reg, Registers[i].reg_name_alias) == 0)
        {
            return Registers[i].reg_value;
        }
        else if(strcmp(reg, "fp") == 0)
        {
            return Registers[8].reg_value;
        }
    }
}

int32_t assign_reg_val(char reg[])
{
    for (int32_t i = 0; i < 32; i++)
    {
        if (strcmp(reg, Registers[i].reg_name_og) == 0 || strcmp(reg, Registers[i].reg_name_alias) == 0)
        {
            return i;
        }
        else if(strcmp(reg,"fp") == 0)
        {
            return 8;
        }
    }
}


bool STORE(char memory_addr_value[], char storing_value[], char imm_str[], int decide_length)
{
    int64_t imm = atol(imm_str);
    int64_t base_addr = get_reg_val(memory_addr_value);
    int64_t value = get_reg_val(storing_value);
    int64_t address = base_addr + imm;
    int64_t position = address;
    for (int i = 0; i < decide_length; i++)
    {
        Memory_Values[position + i] = value & 0xff;
        value = ((uint64_t)value) >> 8;
    }
    return true;
}

bool is_hexadecimal(char *str)
{
    if (strlen(str) > 2 && str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
    {
        for (int32_t iter = 2; str[iter] != '\0'; iter++)
        {
            if (!isxdigit(str[iter]))
                return false;
        }
        return true;
    }
    return false;
}

void separator(char *str)
{
    char *token;
    token = strtok(str, " ");
    if (strstr(token, "load") != NULL)
    {
        token = strtok(NULL, " ");
        initialization();
        load_file(token);
        return;
    }
    else if (strstr(token, "step") != NULL)
    {
        run(false);
        return;
    }
    else if (strstr(token, "run") != NULL)
    {
        run(true);
        return;
    }
    else if (strstr(token, "regs") != NULL)
    {
        display_register();
        return;
    }
    else if (strstr(token, "break") != NULL)
    {
        token = strtok(NULL, " ");
        set_break_point(atol(token));
        return;
    }
    else if (strstr(token, "del") != NULL)
    {
        token = strtok(NULL, " ");
        token = strtok(NULL, " ");
        del_break_point(atol(token));
        return;
    }
    else if (strstr(token, "mem") != NULL)
    {
        token = strtok(NULL, " ");
        char *count = strtok(NULL, " ");
        Print_Memory(token, count);
    }
    else if (strstr(token, "exit") != NULL)
    {
        puts("Exited the Simulator");
        if (input != NULL)
        {
            fclose(input);
        }
        exit(0);
    }
    else if (strstr(token, "show-stack") != NULL)
    {
        show_stack();
    }
    else
    {
        puts("Un-identified Command");
        puts("enter \"exit\" to exit the simulator");
        return;
    }

    return;
}

bool ADDI(char rd[], char rs1[], char imm_str[])
{
    int64_t rd_num = 0, rs1_num = 0, imm = 0;
    imm = is_hexadecimal(imm_str) ? strtol(imm_str, NULL, 16) : atol(imm_str);
    rs1_num = get_reg_val(rs1);
    rd_num = rs1_num + imm;
    Registers[assign_reg_val(rd)].reg_value = rd_num;

    return true;
}

bool ANDI(char rd[], char rs1[], char imm_str[])
{
    int64_t rd_num = 0, rs1_num = 0, imm = 0;
    imm = atol(imm_str);
    rs1_num = get_reg_val(rs1);
    rd_num = rs1_num & imm;
    Registers[assign_reg_val(rd)].reg_value = rd_num;
    return true;
}

bool ORI(char rd[], char rs1[], char imm_str[])
{
    int64_t rd_num = 0, rs1_num = 0, imm = 0;
    imm = atol(imm_str);
    rs1_num = get_reg_val(rs1);
    rd_num = rs1_num | imm;
    Registers[assign_reg_val(rd)].reg_value = rd_num;
    return true;
}

bool XORI(char rd[], char rs1[], char imm_str[])
{
    int64_t rd_num = 0, rs1_num = 0, imm = 0;
    imm = atol(imm_str);
    rs1_num = get_reg_val(rs1);
    rd_num = rs1_num ^ imm;
    Registers[assign_reg_val(rd)].reg_value = rd_num;
    return true;
}

bool SLLI(char rd[], char rs1[], char imm_str[])
{
    int64_t rd_num = 0, rs1_num = 0;
    int64_t imm = atol(imm_str);
    rs1_num = get_reg_val(rs1);
    rd_num = rs1_num << imm;
    Registers[assign_reg_val(rd)].reg_value = rd_num;
    return true;
}


bool SRLI(char rd[], char rs1[], char imm_str[])
{
    int64_t rd_num = 0, rs1_num = 0;
    int64_t imm = atol(imm_str);
    rs1_num = get_reg_val(rs1);
    rd_num = (unsigned)rs1_num >> imm;
    Registers[assign_reg_val(rd)].reg_value = rd_num;
    return true;
}

bool SRAI(char rd[], char rs1[], char imm_str[])
{
    int64_t rd_num = 0, rs1_num = 0;
    int64_t imm = atol(imm_str);
    rs1_num = get_reg_val(rs1);
    rd_num = rs1_num >> imm;
    Registers[assign_reg_val(rd)].reg_value = rd_num;
    return true;
}

bool ADD(char rd[], char rs1[], char rs2[])
{
    int64_t rd_num = 0, rs1_num = 0, rs2_num = 0;
    rs1_num = get_reg_val(rs1);
    rs2_num = get_reg_val(rs2);
    rd_num = rs1_num + rs2_num;
    Registers[assign_reg_val(rd)].reg_value = rd_num;
    return true;
}

bool SUB(char rd[], char rs1[], char rs2[])
{
    int64_t rd_num = 0, rs1_num = 0, rs2_num = 0;
    rs1_num = get_reg_val(rs1);
    rs2_num = get_reg_val(rs2);
    rd_num = rs1_num - rs2_num;
    Registers[assign_reg_val(rd)].reg_value = rd_num;
    return true;
}

bool OR(char rd[], char rs1[], char rs2[])
{
    int64_t rd_num = 0, rs1_num = 0, rs2_num = 0;
    rs1_num = get_reg_val(rs1);
    rs2_num = get_reg_val(rs2);
    rd_num = rs1_num | rs2_num;
    Registers[assign_reg_val(rd)].reg_value = rd_num;
    return true;
}

bool AND(char rd[], char rs1[], char rs2[])
{
    int64_t rd_num = 0, rs1_num = 0, rs2_num = 0;
    rs1_num = get_reg_val(rs1);
    rs2_num = get_reg_val(rs2);
    rd_num = rs1_num & rs2_num;
    Registers[assign_reg_val(rd)].reg_value = rd_num;
    return true;
}

bool XOR(char rd[], char rs1[], char rs2[])
{
    int64_t rd_num = 0, rs1_num = 0, rs2_num = 0;
    rs1_num = get_reg_val(rs1);
    rs2_num = get_reg_val(rs2);
    rd_num = rs1_num ^ rs2_num;
    Registers[assign_reg_val(rd)].reg_value = rd_num;
    return true;
}

bool SLL(char rd[], char rs1[], char rs2[])
{
    int64_t rd_num = 0, rs1_num = 0, rs2_num = 0;
    rs1_num = get_reg_val(rs1);
    rs2_num = get_reg_val(rs2);
    rd_num = rs1_num << rs2_num;
    Registers[assign_reg_val(rd)].reg_value = rd_num;
    return true;
}

bool SRL(char rd[], char rs1[], char rs2[])
{
    int64_t rd_num = 0, rs1_num = 0, rs2_num = 0;
    rs1_num = get_reg_val(rs1);
    rs2_num = get_reg_val(rs2);
    rd_num = (unsigned)rs1_num >> rs2_num;
    Registers[assign_reg_val(rd)].reg_value = rd_num;
    return true;
}

bool SRA(char rd[], char rs1[], char rs2[])
{
    int64_t rd_num = 0, rs1_num = 0, rs2_num = 0;
    rs1_num = get_reg_val(rs1);
    rs2_num = get_reg_val(rs2);
    rd_num = rs1_num >> rs2_num;
    Registers[assign_reg_val(rd)].reg_value = rd_num;
    return true;
}

bool LUI(char rd[], char imm_str[])
{
    int32_t imm = is_hexadecimal(imm_str) ? strtol(imm_str, NULL, 16) : atoi(imm_str);
    Registers[assign_reg_val(rd)].reg_value = (imm << 12);
    return true;
}

int main(void)
{
    while (true)
    {
        char command[100];
        fgets(command, sizeof(command), stdin);
        separator(command);
    }
    return 0;
}
