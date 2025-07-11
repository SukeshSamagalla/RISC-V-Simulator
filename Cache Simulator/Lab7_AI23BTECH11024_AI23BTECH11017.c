#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#define MAX_Labels 10000
#define MEM_VAL 0x50000

FILE *input_file;
FILE *output_file;
char global_file_name[50];
int64_t PC = 0;
int64_t dummy = 0;
int64_t line_num_global = 0;
int64_t offset_global = 0;
int32_t BREAK_POINTS[100] = {-1};
int32_t num_break_points = 0;
int64_t dot_line_num = 0;
int load_counter = 0;
int8_t Memory_Values[MEM_VAL] = {0};
int32_t num_byte = -1;

typedef struct Set_Block
{
    bool dirty;
    int32_t set_index;
    // bool valid;
    int tag;
    int used_count_lfu;
    time_t recent_time;
    uint8_t **block_byte;
    struct Set_Block *next;
    struct Set_Block *prev;
} Set_Block;

typedef struct cache
{
    Set_Block **SetBlock;
    int **tag_count;
} cache;

cache *actual_cache;
bool cache_stat = false;
FILE *cache_config;

int cache_size, block_size, associativity;
char write_policy[20];
char replacement_policy[20];
int no_sets;
bool flag_search_cache;
int hit_count = 0, miss_count = 0;
Set_Block *search_cache(int32_t tag, int32_t set_index);
cache *create_cache();
void insert_cache(int64_t address, int32_t tag, int32_t set_index);
void delete_cache(int tag, int32_t set_index);
int32_t min_lfu_count(int32_t set_index);
int32_t min_lru_count(int32_t set_index);
void miss_insert_cache(int64_t address, int32_t tag, int32_t set_index); //, int decide_length)

cache *create_cache()
{
    cache *empty_cache = (cache *)malloc(sizeof(cache));

    empty_cache->SetBlock = (Set_Block **)malloc(no_sets * sizeof(Set_Block *)); // cache_size/(block_size*associativity)
    empty_cache->tag_count = (int **)malloc(no_sets * sizeof(int *));

    for (int i = 0; i < no_sets; i++)
    {
        *(empty_cache->SetBlock + i) = (Set_Block *)malloc(sizeof(Set_Block));
        empty_cache->tag_count[i] = (int *)malloc(sizeof(int));
        *(empty_cache->tag_count[i]) = 0;
        (empty_cache->SetBlock[i])->next = NULL;
        (empty_cache->SetBlock[i])->prev = NULL;
        (empty_cache->SetBlock[i])->dirty = false;
        (empty_cache->SetBlock[i])->used_count_lfu = 0;
        (empty_cache->SetBlock[i])->recent_time = time(NULL);
        (empty_cache->SetBlock[i])->set_index = 0;

        empty_cache->SetBlock[i]->block_byte = (uint8_t **)malloc(block_size * sizeof(uint8_t *));

        for (int j = 0; j < block_size; j++)
        {
            empty_cache->SetBlock[i]->block_byte[j] = (uint8_t *)malloc(sizeof(uint8_t));
        }
    }

    return empty_cache;
}

void insert_cache(int64_t address, int32_t tag, int32_t set_index)
{
    Set_Block *new_tag = (Set_Block *)malloc(sizeof(Set_Block));
    new_tag->block_byte = (uint8_t **)malloc(block_size * sizeof(uint8_t *));

    new_tag->dirty = false;
    new_tag->tag = tag;
    new_tag->used_count_lfu = 0;
    new_tag->recent_time = time(NULL);
    new_tag->set_index = set_index;
    new_tag->next = NULL;
    new_tag->prev = NULL;

    for (int j = 0; j < block_size; j++)
    {
        new_tag->block_byte[j] = (uint8_t *)malloc(sizeof(uint8_t));
        *(new_tag->block_byte[j]) = (uint8_t)Memory_Values[((address / block_size) * block_size) + j];
        // printf("%u\n",*(new_tag->block_byte[j]));
    }

    new_tag->next = (actual_cache->SetBlock[set_index]->next);
    new_tag->prev = (actual_cache->SetBlock[set_index]);
    if (actual_cache->SetBlock[set_index]->next != NULL)
    {
        (actual_cache->SetBlock[set_index]->next)->prev = new_tag;
    }
    actual_cache->SetBlock[set_index]->next = new_tag;

    // actual_cache->SetBlock[set_index]->block_byte
    return;
}
void delete_cache(int tag, int32_t set_index)
{
    // bool flag = false;
    Set_Block *search_return = search_cache(tag, set_index);

    if (search_return->dirty == true)
    {
        for (int i = 0; i < block_size; i++)
        {
            Memory_Values[(set_index * block_size) + (tag * block_size * no_sets) + i] = (int8_t) * (search_return->block_byte[i]);
        }
    }

    (search_return->prev)->next = search_return->next;
    if (search_return != NULL)
    {
        if (search_return->next != NULL)
        {
            (search_return->next)->prev = search_return->prev;
        }
    }
    free(search_return);
    return;
}

int min_lfu_count(int32_t set_index)
{
    Set_Block *search_return = search_cache(actual_cache->SetBlock[set_index]->next->tag, set_index);
    int min_lfu = (search_return != NULL) ? search_return->used_count_lfu : 0;
    int tag_to_delete = (search_return != NULL) ? search_return->tag : 0;
    while (search_return != NULL)
    {
        if (min_lfu > search_return->used_count_lfu)
        {
            min_lfu = search_return->used_count_lfu;
            tag_to_delete = search_return->tag;
        }
        search_return = search_return->next;
    }
    return tag_to_delete;
}

int min_lru_count(int32_t set_index)
{
    Set_Block *search_return = search_cache(actual_cache->SetBlock[set_index]->next->tag, set_index);
    time_t min_lru = (search_return != NULL) ? search_return->recent_time : 0;
    int tag_to_delete = (search_return != NULL) ? search_return->tag : 0;
    while (search_return != NULL)
    {
        if (min_lru > search_return->recent_time)
        {
            min_lru = search_return->recent_time;
            tag_to_delete = search_return->tag;
        }
        search_return = search_return->next;
    }
    return tag_to_delete;
}

Set_Block *search_cache(int tag, int32_t set_index)
{
    flag_search_cache = false;
    Set_Block *searching = actual_cache->SetBlock[set_index]->next;
    while (!flag_search_cache && searching != NULL)
    {
        if (searching->tag == tag)
        {
            flag_search_cache = true;
            return searching;
        }
        searching = searching->next;
    }
    return NULL;
}

void miss_insert_cache(int64_t address, int tag, int32_t set_index) //, int decide_length)
{
    if (*(actual_cache->tag_count[set_index]) == associativity)
    {
        // Here replacement policy applied
        switch (replacement_policy[1])
        {
        case 'I':
        { // FIFO
            Set_Block *to_delete = actual_cache->SetBlock[set_index]->next;
            while (to_delete->next != NULL)
            {
                to_delete = to_delete->next;
            }
            delete_cache(to_delete->tag, set_index);
            break;
        }
        case 'F':
        { // LFU
            int tag_to_delete = min_lfu_count(set_index);
            delete_cache(tag_to_delete, set_index);
            break;
        }
        case 'R':
        { // LRU
            int tag_to_delete = min_lru_count(set_index);
            delete_cache(tag_to_delete, set_index);
            break;
        }
        case 'A':
        { // RANDOM

            srand(time(NULL));

            int index = 1 + rand() % associativity;
            Set_Block *to_delete = actual_cache->SetBlock[set_index]->next;
            for (int i = 1; i < index; i++)
            {
                to_delete = to_delete->next;
            }
            delete_cache(to_delete->tag, set_index);
            break;
        }

        default:
            break;
        }
        insert_cache(address, tag, set_index);
    }
    else
    {
        // here new tag is loaded in the set without disturbing the existing tags
        insert_cache(address, tag, set_index);
        *(actual_cache->tag_count[set_index]) = *(actual_cache->tag_count[set_index]) + 1;
    }
    return;
}

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
    {"x0", "zero", 0, 0},
    {"x1", "ra", 0, 0},
    {"x2", "sp", 0, 0},
    {"x3", "gp", 0, 0},
    {"x4", "tp", 0, 0},
    {"x5", "t0", 0, 0},
    {"x6", "t1", 0, 0},
    {"x7", "t2", 0, 0},
    {"x8", "s0", 0, 0},
    {"x9", "s1", 0, 0},
    {"x10", "a0", 0, 0},
    {"x11", "a1", 0, 0},
    {"x12", "a2", 0, 0},
    {"x13", "a3", 0, 0},
    {"x14", "a4", 0, 0},
    {"x15", "a5", 0, 0},
    {"x16", "a6", 0, 0},
    {"x17", "a7", 0, 0},
    {"x18", "s2", 0, 0},
    {"x19", "s3", 0, 0},
    {"x20", "s4", 0, 0},
    {"x21", "s5", 0, 0},
    {"x22", "s6", 0, 0},
    {"x23", "s7", 0, 0},
    {"x24", "s8", 0, 0},
    {"x25", "s9", 0, 0},
    {"x26", "s10", 0, 0},
    {"x27", "s11", 0, 0},
    {"x28", "t3", 0, 0},
    {"x29", "t4", 0, 0},
    {"x30", "t5", 0, 0},
    {"x31", "t6", 0, 0},
};

typedef struct Label
{
    char name[21];
    int64_t line_num;
    int64_t offset;
} Label;

Label labels[MAX_Labels];
int32_t label_count = 0;

void initialization()
{
    PC = 0;
    dummy = 0;
    line_num_global = 0;
    offset_global = 0;
    dot_line_num = 0;
    label_count = 0;
    hit_count = 0;
    miss_count = 0;
    num_byte = -1;
    if (load_counter != 0)
    {
        free(actual_cache);
        actual_cache = create_cache();
    }
    for (int i = 0; i < 32; i++)
    {
        Registers[i].reg_value = 0;
        Registers[i].jal_offset = 0;
    }
    for (int i = 0; i < MEM_VAL; i++)
    {
        Memory_Values[i] = 0;
    }
    for (int i = 0; i < 100; i++)
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
    Stack.pos_of_calling[Stack.top_index] = dummy + 1;
    return;
}

void show_stack()
{
    if (dummy + 1 == line_num_global)
    {
        printf("Empty Call Stack: Execution complete\n");
    }
    else
    {
        printf("Call Stack:\n");
        if (dummy == 0)
        {
            printf("main:%ld\n", dot_line_num);
            // printf("main:0\n");
        }
        else
        {

            for (int i = 0; i <= Stack.top_index; i++)
            {
                printf("%s:%ld\n", Stack.name[i], Stack.pos_of_calling[i] + dot_line_num);
                // printf("%s:%d\n", Stack.name[i], Stack.pos_of_calling[i]);
            }
        }
    }
    puts("");
    return;
}

void set_break_point(int64_t line_num)
{
    BREAK_POINTS[num_break_points] = line_num - dot_line_num;
    printf("Breakpoint set at line %ld\n\n", line_num);
    num_break_points++;
    return;
}
void del_break_point(int64_t line_num)
{
    line_num = line_num - dot_line_num;
    int i;
    for (i = 0; i < 100; i++)
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
    for (i = 0; i < 100; i++)
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
    dummy = PC;
    stack_update(dummy);
    int64_t offset = find_label_address(imm_str);
    fseek(input_file, offset, SEEK_SET);

    return true;
}

bool JALR(char rd[], char imm_str[], char rs1[])
{
    char buffer[50];

    int64_t rs1_num = get_reg_val(rs1);
    int64_t rd_num = PC * 4 + 4;
    Registers[assign_reg_val(rd)].reg_value = rd_num;

    int64_t imm = (atoi(imm_str)) / 4;

    dummy = PC;
    stack_update(dummy);
    PC = (rs1_num / 4) + imm;

    int64_t dummy_use;
    for (int i = 0; i < 32; i++)
    {
        if ((rs1_num / 4) == labels[i].line_num)
        {
            Registers[assign_reg_val(rs1)].jal_offset = labels[i].offset;
        }
    }

    dummy_use = Registers[assign_reg_val(rs1)].jal_offset;

    fseek(input_file, dummy_use, SEEK_SET);
    if (imm > 0)
    {
        for (int i = 0; i < imm; i++)
        {
            fgets(buffer, sizeof(buffer), input_file);
            fseek(input_file, strlen(buffer), SEEK_CUR);
            dummy = PC;
            PC++;
        }
    }
    if (imm < 0)
    {
        for (int i = 0; i > imm; i--)
        {
            fgets(buffer, sizeof(buffer), input_file);
            fseek(input_file, -strlen(buffer), SEEK_CUR);
            dummy = PC;
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

bool LOAD(char rd[], char rs1[], char imm_str[], int decide_length, bool flag)
{
    int64_t imm = atol(imm_str);
    int64_t rs1_num = get_reg_val(rs1);

    int64_t address = rs1_num + imm;
    int64_t position = address;
    int64_t rd_num = 0;

    if (!cache_stat)
    {

        for (int i = 0; i < decide_length; i++)
        {
            uint8_t byte = (uint8_t)Memory_Values[position + i];
            rd_num |= (int64_t)byte << (8 * (i));
        }

        if (flag && decide_length == 1)
        {
            rd_num = (int8_t)rd_num;
        }
        else if (flag && decide_length == 2)
        {
            rd_num = (int16_t)rd_num;
        }
        else if (flag && decide_length == 4)
        {
            rd_num = (int32_t)rd_num;
        }
        else if (flag && decide_length == 8)
        {
            rd_num = (int64_t)rd_num;
        }

        Registers[assign_reg_val(rd)].reg_value = rd_num;
        return true;
    }
    else // cache has been enabled now, so use caches
    {
        int32_t byte_offset = position % block_size;
        int32_t set_index = (position / block_size) % no_sets;
        int32_t tag = position / (no_sets * block_size);

        Set_Block *search_return = search_cache(tag, set_index);

        if (flag_search_cache) // hit happens
        {
            hit_count++;
            output_file = fopen(global_file_name, "a");
            fprintf(output_file, "R: Address: 0x%5lx, Set: 0x%X, Hit, Tag: 0x%X, %s\n", address, set_index, tag, (search_return->dirty == true) ? "Dirty" : "Clean");
            fclose(output_file);
        }
        else // miss happens
        {
            miss_count++;
            output_file = fopen(global_file_name, "a");
            fprintf(output_file, "R: Address: 0x%5lx, Set: 0x%X, Miss, Tag: 0x%X, Clean\n", address, set_index, tag);
            fclose(output_file);
            miss_insert_cache(address, tag, set_index); //, decide_length);
        }

        Set_Block *search_new_return = search_cache(tag, set_index);
        for (int i = 0; i < decide_length; i++)
        {
            uint8_t byte = *(search_new_return->block_byte[byte_offset + i]);
            search_new_return->recent_time = time(NULL);
            rd_num |= (int64_t)byte << (8 * (i));
        }
        search_new_return->used_count_lfu = search_new_return->used_count_lfu +1 ;

        if (flag && decide_length == 1)
        {
            rd_num = (int8_t)rd_num;
        }
        else if (flag && decide_length == 2)
        {
            rd_num = (int16_t)rd_num;
        }
        else if (flag && decide_length == 4)
        {
            rd_num = (int32_t)rd_num;
        }
        else if (flag && decide_length == 8)
        {
            rd_num = (int64_t)rd_num;
        }

        Registers[assign_reg_val(rd)].reg_value = rd_num;

        return true;
    }
}

void load_file(char file_name[])
{
    file_name[strcspn(file_name, "\n")] = 0;
    input_file = fopen(file_name, "r");
    char *dot = strrchr(file_name, '.');
    if (dot != NULL)
    {
        strcpy(dot, ".output");
    }
    strcpy(global_file_name,file_name);
    //output_file = fopen(global_file_name, "w");

    if (input_file == NULL)
    {
        puts("Error opening file: No such file or directory");
        return;
    }
    char line[200];
    while (fgets(line, sizeof(line), input_file))
    {
        if (line[0] == '.')
        {

            dot_line_num++;
        }
    }

    rewind(input_file);
    data_segregation();
    while (fgets(line, sizeof(line), input_file))
    {
        if (line[0] == '.')
        {
            offset_global += strlen(line);

            continue;
        }
        char label[21];
        if (strchr(line, ':') != NULL)
        {
            sscanf(line, "%[^:]:", label);
        }
        else
        {
            sscanf(line, "%[^ ] ", label);
        }
        add_label(label, line_num_global, offset_global);
        line_num_global += 1;
        offset_global += strlen(line);
    }
    rewind(input_file);

    return;
}

void run(bool flag)
{
    char line[50] = "";
    if (input_file == NULL)
    {
        puts("Error opening file: No such file or directory");
        return;
    }
    while (fgets(line, sizeof(line), input_file))
    {
        Registers[0].reg_value = 0;
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
        Registers[0].reg_value = 0;
    }
    printf("D-cache statistics: Accesses=%d, Hit=%d, Miss=%d Hit Rate=%.2f\n\n", (hit_count + miss_count), hit_count, miss_count, ((hit_count + miss_count) == 0) ? 0 : ((hit_count) / (float)(hit_count + miss_count)));
    return;
}
void print_cache_status()
{
    char line[20];
    int i = 0;
    char *list[] = {"Cache Size", "Block Size", "Associativity", "Replacement Policy", "Write Back Policy"};
    puts("");
    while (fgets(line, sizeof(line), cache_config))
    {
        printf("%s: %s", *(list + i), line);
        i++;
    }
    puts("\n");
    rewind(cache_config);
    return;
}

void display_register(void)
{
    puts("Registers:");
    for (int32_t i = 0; i < 32; i++)
    {
        printf("%s = 0x%0lx\n", Registers[i].reg_name_og, Registers[i].reg_value); //, Registers[i].reg_value);
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
            dummy = PC;
            stack_update(dummy);

            int64_t offset = find_label_address(imm_str);
            fseek(input_file, offset, SEEK_SET);
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
            dummy = PC;
            stack_update(dummy);

            int64_t offset = find_label_address(imm_str);
            fseek(input_file, offset, SEEK_SET);
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
            dummy = PC;
            stack_update(dummy);
            int64_t offset = find_label_address(imm_str);
            fseek(input_file, offset, SEEK_SET);
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
            dummy = PC;
            stack_update(dummy);
            int64_t offset = find_label_address(imm_str);
            fseek(input_file, offset, SEEK_SET);
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
            dummy = PC;
            stack_update(dummy);
            int64_t offset = find_label_address(imm_str);

            fseek(input_file, offset, SEEK_SET);
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
            dummy = PC;
            stack_update(dummy);
            int64_t offset = find_label_address(imm_str);

            fseek(input_file, offset, SEEK_SET);
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
    dummy = PC;
    stack_update(dummy);
    if (PC == line_num_global - 1)
    {
        return;
    }
    PC = PC + 1;
    return;
}

void data_segregation()
{
    char line[200];
    fgets(line, sizeof(line), input_file);
    if (line[0] == '.')
    {
        offset_global += strlen(line);

        if (strstr(line, "data") != NULL)
        {
            fgets(line, sizeof(line), input_file);
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
                        Memory_Values[num_byte + 0x10000] = byte_n;
                        num = num >> 8;
                    }
                    token = strtok(NULL, ", \n");
                }
                fgets(line, sizeof(line), input_file);
                offset_global += strlen(line);
            } while (strstr(line, ".text") == NULL);
        }
        else if (strstr(line, "text") != NULL)
        {
        }
    }
    else
    {
        rewind(input_file);
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
        else if (strcmp(reg, "fp") == 0)
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
        else if (strcmp(reg, "fp") == 0)
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
    if (!cache_stat)
    {
        for (int i = 0; i < decide_length; i++)
        {
            Memory_Values[position + i] = value & 0xff;
            value = ((uint64_t)value) >> 8;
        }
        return true;
    }
    else
    { // cache has been enabled now !.
        int32_t byte_offset = position % block_size;
        int32_t set_index = (position / block_size) % no_sets;
        int32_t tag = position / (no_sets * block_size);

        Set_Block *search_return = search_cache(tag, set_index);

        bool check_for_policy = false;

        if (flag_search_cache)
        {
            hit_count++;

            if (write_policy[1] == 'B')
            {
                output_file = fopen(global_file_name, "a");
                fprintf(output_file, "W: Address: 0x%5lx, Set: 0x%X, Hit, Tag: 0x%X, %s\n", address, set_index, tag, (search_return->dirty == true) ? "Dirty" : "Clean");
                fclose(output_file);
                
            }
            else
            {
                check_for_policy = true;
                output_file = fopen(global_file_name, "a");
                fprintf(output_file, "W: Address: 0x%5lx, Set: 0x%X, Hit, Tag: 0x%X, Clean\n", address, set_index, tag);
                fclose(output_file);
            }
            value = get_reg_val(storing_value);
            for (int i = 0; i < decide_length; i++)
            {
                *(search_return->block_byte[byte_offset + i]) = value & 0x0ff; // changing only the cache values!.
                search_return->recent_time = time(NULL);
                search_return->dirty = true;
                if (check_for_policy)                                          // changing mem values if policy was WT
                {
                    Memory_Values[position + i] = value & 0xff; // for the write through cases! we are changing the memory values
                    search_return->dirty = false;
                }
                value = ((uint64_t)value) >> 8;
            }
            search_return->used_count_lfu = search_return->used_count_lfu + 1;
        }
        else
        {
            miss_count++;

            if (write_policy[1] == 'B')
            { // write back policy
                miss_insert_cache(position, tag, set_index);
                Set_Block *new_block_for_WB = search_cache(tag, set_index);

                

                for (int i = 0; i < decide_length; i++)
                {
                    *(new_block_for_WB->block_byte[byte_offset + i]) = value & 0xff;
                    new_block_for_WB->recent_time = time(NULL);
                    value = ((uint64_t)value) >> 8;
                }
                new_block_for_WB->used_count_lfu = new_block_for_WB->used_count_lfu +1 ;
                new_block_for_WB->dirty = true;
                output_file = fopen(global_file_name, "a");
                fprintf(output_file, "W: Address: 0x%5lx, Set: 0x%X, Miss, Tag: 0x%X, %s\n", address, set_index, tag, (new_block_for_WB->dirty == true) ? "Dirty" : "Clean");
                fclose(output_file);
            }
            else
            { // write through policy
                for (int i = 0; i < decide_length; i++)
                {
                    Memory_Values[position + i] = value & 0xff;
                    value = ((uint64_t)value) >> 8;
                }
                output_file = fopen(global_file_name, "a");
                fprintf(output_file, "W: Address: 0x%5lx, Set: 0x%X, Miss, Tag: 0x%X, Clean\n", address, set_index, tag);
                fclose(output_file);
            }
        }
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
        load_counter++;
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
        return;
    }
    else if (strstr(token, "cache_sim") != NULL)
    {
        token = strtok(NULL, " ");
        if (strstr(token, "enable") != NULL)
        {
            cache_stat = true;
            token = strtok(NULL, " ");
            token[strcspn(token, "\n")] = 0;
            cache_config = fopen(token, "r");
            if (cache_config == NULL)
            {
              puts("Error opening file: No such file or directory");
              return;
            }

            char line[20];
            fgets(line, sizeof(line), cache_config);
            line[strcspn(line, "\n")] = 0;

            cache_size = atoi(line);

            fgets(line, sizeof(line), cache_config);
            line[strcspn(line, "\n")] = 0;

            block_size = atoi(line);

            fgets(line, sizeof(line), cache_config);
            line[strcspn(line, "\n")] = 0;

            associativity = atoi(line);

            fgets(line, sizeof(line), cache_config);
            line[strcspn(line, "\n")] = 0;

            strcpy(replacement_policy, line);

            fgets(line, sizeof(line), cache_config);
            line[strcspn(line, "\n")] = 0;

            strcpy(write_policy, line);
            if(associativity == 0)
            {
                associativity=cache_size /block_size;
            }
            no_sets = cache_size / (block_size * associativity);
            
            actual_cache = create_cache();

            rewind(cache_config);
        }
        else if (strstr(token, "disable") != NULL)
        {
            cache_stat = false;
        }
        else if (strstr(token, "status") != NULL)
        {
            if (!cache_config) // == NULL)
            {
                puts("Error opening file: No such file or directory");
                return;
            }
            print_cache_status();
        }
        else if (strstr(token, "invalidate") != NULL)
        {

            // write dirty blocks to memory before invalidation!!
            for (int i = 0; i < no_sets; i++)
            {
                if (((actual_cache->SetBlock[i])->next) != NULL)
                {
                    Set_Block *to_copy = (actual_cache->SetBlock[i])->next;
                    while (to_copy != NULL)
                    {
                        if (to_copy->dirty == true)
                        {
                            for (int i = 0; i < block_size; i++)
                            {
                                Memory_Values[(to_copy->set_index * block_size) + (to_copy->tag * block_size * no_sets) + i] = (int8_t) * (to_copy->block_byte[i]);
                            }
                        }
                        to_copy = to_copy->next;
                    }
                }
            }

            free(actual_cache);
            actual_cache = create_cache();
        }
        else if (strstr(token, "stats") != NULL)
        {
            printf("D-cache statistics: Accesses=%d, Hit=%d, Miss=%d Hit Rate=%.2f\n\n", (hit_count + miss_count), hit_count, miss_count, ((hit_count + miss_count) == 0) ? 0 : ((hit_count) / (float)(hit_count + miss_count)));
        }
        else if (strstr(token, "dump") != NULL)
        {
            token = strtok(NULL, " ");
            token[strcspn(token, "\n")] = 0;
            FILE *cache_dump;
            cache_dump = fopen(token, "w");
            for (int i = 0; i < no_sets; i++)
            {
                if (((actual_cache->SetBlock[i])->next) != NULL)
                {
                    Set_Block *to_print = (actual_cache->SetBlock[i])->next;
                    while (to_print != NULL)
                    {
                        fprintf(cache_dump, "Set: 0x%x, Tag: 0x%x, %s\n", to_print->set_index, to_print->tag, (to_print->dirty == true) ? "Dirty" : "Clean");
                        to_print = to_print->next;
                    }
                }
            }
            fclose(cache_dump);
        }

        return;
    }
    else if (strstr(token, "exit") != NULL)
    {
        puts("Exited the Simulator");
        if (input_file != NULL)
        {
            fclose(input_file);
        }
        exit(0);
    }
    else if (strstr(token, "show-stack") != NULL)
    {
        show_stack();
        return;
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
