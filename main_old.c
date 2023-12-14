#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/param.h>

#define PAGE_SIZE 256
#define COUNT_MEMORY_PAGES 128
#define COUNT_BACKING_STORE_PAGES 256
#define TLB_SIZE 16
#define MEMORY_SIZE PAGE_SIZE * COUNT_MEMORY_PAGES
#define BACKING_STORE_SIZE PAGE_SIZE * COUNT_BACKING_STORE_PAGES 

char memory[MEMORY_SIZE];
char backing_store[BACKING_STORE_SIZE];

int page_table[COUNT_BACKING_STORE_PAGES][2];
int tlb_table[TLB_SIZE][2]; 
int claim_table[COUNT_MEMORY_PAGES][2];
int tlb_size = 0;
int page_table_size = 0;

// void add_page(int page_number, int frame_number) {
//     page_table[page_table_size][0] = page_number;
//     page_table[page_table_size][1] = frame_number;
//     page_table_size++;
// }

int get_frame_by_page_from_tlb(int page) {
    for (int x = 0; x < MIN(tlb_size, TLB_SIZE); x++) // hit
        if (tlb_table[x][0] == page) {
            return tlb_table[x][1];
        }
    return -1;
}

int get_frame_by_page_from_page_tab(int page){
    for (int x = 0; x < MIN(page_table_size, COUNT_BACKING_STORE_PAGES); x++)
        if (page_table[x][0] == page) {
            return page_table[x][1];
        }
    return -1;
}

int get_free_ram_page_id() {
    for (int x = 0; x < COUNT_MEMORY_PAGES; x++) // find and claim free frame
        if (!claim_table[x][0]) {
            return x;
        }
    return -1;
}

int claim_ram_page(int ram_page_id){
    claim_table[ram_page_id][0] = 1;
}

void add_page_to_page_table(int page_id, int frame_id) {
    page_table[page_table_size % COUNT_BACKING_STORE_PAGES][0] = page_id;
    page_table[page_table_size % COUNT_BACKING_STORE_PAGES][1] = frame_id;
    page_table_size++;

    tlb_table[tlb_size % TLB_SIZE][0] = page_id;
    tlb_table[tlb_size % TLB_SIZE][1] = frame_id;
    tlb_size++;

    claim_ram_page(page_id);
}

void add_page_to_tlb(int page_id, int frame_id) {
    tlb_table[tlb_size % TLB_SIZE][0] = page_id;
    tlb_table[tlb_size % TLB_SIZE][1] = frame_id;
    tlb_size++;

    claim_ram_page(page_id);
}

int find_least_used_ram_page_id() {
    int use_frequency = claim_table[0][1];
    int least_used_ram_page_id = 0;
    for (int x = 1; x < COUNT_MEMORY_PAGES; x++) // find the least used frame
        if (claim_table[x][1] <= use_frequency) {
            use_frequency = claim_table[x][1];
            least_used_ram_page_id = x;
        }
    return least_used_ram_page_id;
}

int get_page_table_row_id_by_frame(int frame) {
    for (int x = 0; x < MIN(page_table_size, COUNT_BACKING_STORE_PAGES); x++) // ??
        if (page_table[x][1] == frame) {
            return x;
        }
    return -1;
}

int get_tlb_row_id_by_frame(int frame) {
    for (int x = 0; x < MIN(tlb_size, TLB_SIZE); x++)
        if (tlb_table[x][1] == frame) {
            return x;
        }
    return -1;
}

/*
for (int x = 0; x < MIN(tlb_size_temp, TLB_SIZE); x++)
                        if (tlb_table[x][1] == free_page) {
                            tlb_table[x][0] = -1;
                            memcpy(tlb_table[x], tlb_table[(tlb_size - 1) % TLB_SIZE], 2);
                            tlb_size--;
                        }
*/

int delete_row_from_page_table(int row_id){
    // 1, 2, 3, 4, e, 5, 6
    // To delete an element from an array where an elements order doesn't matter,
    // we can just move the last element to the element we want to delete and
    // set the last element as free (for example -1).
    // 1, 2, 3 ,4, 6, 5, _
    memcpy(page_table[row_id], page_table[(page_table_size - 1) % COUNT_BACKING_STORE_PAGES], 2);
    page_table_size--;
    // page_table_size is a pointer to the last added element, not the overall count of elements.
}

int delete_row_from_tlb(int row_id){
    memcpy(tlb_table[row_id], tlb_table[(tlb_size - 1) % TLB_SIZE], 2);
    tlb_size--;
}

char get_data_by_virtual_adress(int virtual_address) {
    int page = virtual_address >> 8;
    int offset = virtual_address & 0x00FF;
    int frame = -1;
    char value = -1;

    frame = get_frame_by_page_from_tlb(page);
    if (frame < 0){ // the frame wasn't found in tlb
        frame = get_frame_by_page_from_page_tab(page);

        if (frame < 0){ // the frame wasn't found in page table
            int free_ram_page_id = get_free_ram_page_id();

            if (free_ram_page_id < 0){ // free ram page wasn't found, i.e ram is full
                int least_used_ram_page_id = find_least_used_ram_page_id();
                int page_table_row = get_page_table_row_id_by_frame(least_used_ram_page_id);
                int tlb_row = get_tlb_row_id_by_frame(least_used_ram_page_id);
                delete_row_from_page_table(page_table_row);
                delete_row_from_tlb(tlb_row);

                free_ram_page_id = least_used_ram_page_id;
            }
            add_page_to_page_table(page, free_ram_page_id);
            add_page_to_tlb(page, free_ram_page_id);
            memcpy(memory + free_ram_page_id * PAGE_SIZE, backing_store + page * PAGE_SIZE, PAGE_SIZE);
            frame = free_ram_page_id;
        }
    }
    value = memory[frame * PAGE_SIZE + offset];
    return value;
}

int main(int argc, char* argv[]) {
    FILE* file = fopen("backing_store.bin", "rb");
    fread(backing_store, 1, BACKING_STORE_SIZE, file);
    fclose(file);

    memset(page_table, -1, sizeof(int) * COUNT_BACKING_STORE_PAGES * 2);
    memset(tlb_table, -1, sizeof(int) * TLB_SIZE * 2);
    memset(claim_table, 0, sizeof(int) * COUNT_MEMORY_PAGES * 2);

    FILE* addreses = fopen("addresses.txt", "r");
    FILE* result = fopen("out.txt", "w");

    int tlb_hits = 0;
    int page_faults = 0;
    int total = 0;

    int virtual_address;
    while (fscanf(addreses, "%d", &virtual_address) == 1) {
        int page = virtual_address >> 8;
        int offset = virtual_address & 0x00FF;
        long frame = -1;
        int value;
        int tlb_size_temp = tlb_size;
        int page_table_size_temp = page_table_size;

        for (int x = 0; x < MIN(tlb_size_temp, TLB_SIZE); x++) // hit
            if (tlb_table[x][0] == page) {
                frame = tlb_table[x][1];
                value = memory[frame * PAGE_SIZE + offset];
                tlb_hits++;
                break;
            }

        if (frame < 0) { // miss

            // find the page number in the page table
            for (int x = 0; x < MIN(page_table_size_temp, COUNT_BACKING_STORE_PAGES); x++)
                if (page_table[x][0] == page) {
                    frame = page_table[x][1];
                    value = memory[frame * PAGE_SIZE + offset];
                    break;
                }

            if (frame < 0) { // page not found, need to allocate a new page
                int free_page = -1;
                for (int x = 0; x < COUNT_MEMORY_PAGES; x++) // find and claim free frame
                    if (!claim_table[x][0]) {
                        free_page = x;
                        claim_table[x][0] = 1;
                        break;
                    }

                if (free_page < 0) { // free frame not found
                    int count_use_min = claim_table[0][1];
                    free_page = 0;
                    for (int x = 1; x < COUNT_MEMORY_PAGES; x++) // find the least used frame
                        if (claim_table[x][1] <= count_use_min) {
                            count_use_min = claim_table[x][1];
                            free_page = x;
                        }

                    for (int x = 0; x < MIN(page_table_size_temp, COUNT_BACKING_STORE_PAGES); x++) // ??
                        if (page_table[x][1] == free_page) {
                            page_table[x][0] = -1;
                            memcpy(page_table[x], page_table[(page_table_size - 1) % COUNT_BACKING_STORE_PAGES], 2);
                            page_table_size--;
                        }

                    for (int x = 0; x < MIN(tlb_size_temp, TLB_SIZE); x++)
                        if (tlb_table[x][1] == free_page) {
                            tlb_table[x][0] = -1;
                            memcpy(tlb_table[x], tlb_table[(tlb_size - 1) % TLB_SIZE], 2);
                            tlb_size--;
                        }
                }

                memcpy(memory + free_page * PAGE_SIZE, backing_store + page * PAGE_SIZE, PAGE_SIZE);

                frame = free_page;

                page_table[page_table_size % COUNT_BACKING_STORE_PAGES][0] = page;
                page_table[page_table_size % COUNT_BACKING_STORE_PAGES][1] = free_page;
                page_table_size++;

                tlb_table[tlb_size % TLB_SIZE][0] = page;
                tlb_table[tlb_size % TLB_SIZE][1] = free_page;
                tlb_size++;

                claim_table[free_page][1]++;

                value = memory[free_page * PAGE_SIZE + offset];

                page_faults++;
            }
        }

        int phys_addr = frame * PAGE_SIZE + offset;

        fprintf(result, "%d: Virtual address: %d Physical address: %d Value: %d\n", total, virtual_address, phys_addr, value);
        total++;
    }

    float tlb_hit_rate = tlb_hits / (float)total;
    float page_fault_rate = page_faults / (float)total;
    printf("Частота попаданий в TLB: %f\nЧастота ошибок страниц: %f\n", tlb_hit_rate, page_fault_rate);

    fclose(addreses);
    fclose(result);

    return 0;
}