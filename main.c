#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/param.h>

#define PAGE_SIZE 256
#define COUNT_MEMORY_PAGES 256
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


int get_frame_by_page_from_tlb(int page, int tlb_size_temp) {
    for (int x = 0; x < MIN(tlb_size_temp, TLB_SIZE); x++)
        if (tlb_table[x][0] == page) {
            return tlb_table[x][1];
        }
    return -1;
}

int get_frame_by_page_from_page_tab(int page, int page_table_size_temp){
    for (int x = 0; x < MIN(page_table_size_temp, COUNT_BACKING_STORE_PAGES); x++)
        if (page_table[x][0] == page) {
            return page_table[x][1];
        }
    return -1;
}

int get_free_ram_page_id() {
    for (int x = 0; x < COUNT_MEMORY_PAGES; x++)
        if (!claim_table[x][0]) {
            return x;
        }
    return -1;
}

int find_least_used_ram_page_id() {
    int use_frequency = claim_table[0][1];
    int least_used_ram_page_id = 0;
    for (int x = 1; x < COUNT_MEMORY_PAGES; x++)
        if (claim_table[x][1] <= use_frequency) {
            use_frequency = claim_table[x][1];
            least_used_ram_page_id = x;
        }
    return least_used_ram_page_id;
}

void delete_page_table_rows_by_frame(int frame, int page_table_size_temp) {
    for (int x = 0; x < MIN(page_table_size_temp, COUNT_BACKING_STORE_PAGES); x++) // ??
        if (page_table[x][1] == frame) {
            page_table[x][0] = -1;
            memcpy(page_table[x], page_table[(page_table_size - 1) % COUNT_BACKING_STORE_PAGES], 2);
            page_table_size--;
        }
}

void delete_tlb_rows_by_frame(int frame, int tlb_size_temp) {
    for (int x = 0; x < MIN(tlb_size_temp, TLB_SIZE); x++)
        if (tlb_table[x][1] == frame) {
            tlb_table[x][0] = -1;
            memcpy(tlb_table[x], tlb_table[(tlb_size - 1) % TLB_SIZE], 2);
            tlb_size--;
        }
}



void page_table_add(int page, int frame) {
    page_table[page_table_size % COUNT_BACKING_STORE_PAGES][0] = page;
    page_table[page_table_size % COUNT_BACKING_STORE_PAGES][1] = frame;
    page_table_size++;
}

void tlb_add(int page, int frame) {
    tlb_table[tlb_size % TLB_SIZE][0] = page;
    tlb_table[tlb_size % TLB_SIZE][1] = frame;
    tlb_size++;
}

int count_lines(FILE* file) {
    int count = 0;
    int data = 0;
    rewind(file);
    while (fscanf(file, "%d", &data) == 1) {
        count++;
    }
    return count;
}

void get_virtual_addresses(FILE* file, int* array) {
    int i = 0;
    int data = 0;
    rewind(file);
    while (fscanf(file, "%d", &data) == 1) {
        array[i] = data;
        i++;
    }
}

int main(int argc, char* argv[]) {
    FILE* file = fopen("backing_store.bin", "rb");
    
    fread(backing_store, 1, BACKING_STORE_SIZE, file);
    

    memset(page_table, -1, sizeof(int) * COUNT_BACKING_STORE_PAGES * 2);
    memset(tlb_table, -1, sizeof(int) * TLB_SIZE * 2);
    memset(claim_table, 0, sizeof(int) * COUNT_MEMORY_PAGES * 2);

    FILE* addreses = fopen("addresses.txt", "r");
    FILE* result = fopen("out.txt", "w");

    int tlb_hits = 0;
    int page_faults = 0;
    int total = 0;

    int virtual_addresses_count = count_lines(addreses);

    int* virtual_addresses = (int*)malloc(virtual_addresses_count * sizeof(int));
    get_virtual_addresses(addreses, virtual_addresses);

    // for (int i = 0; i < virtual_addresses_count; i++) {
    //     printf("%d\n", virtual_addresses[i]);
    // }
        

    int inequality_count = 0;
    // while (fscanf(addreses, "%d", &virtual_address) == 1) {
    for (int virtual_address_id = 0; virtual_address_id < virtual_addresses_count; virtual_address_id++){
        // int scan_result = fscanf(addreses, "%d", &virtual_address);
        // printf("Result: %d\n", scan_result);
        // if (scan_result != 1)
        //     break;

        // fseek(file, 0, SEEK_SET);
        
        // if (fseek(file, 10 * PAGE_SIZE, SEEK_SET) != 0) {
        //     perror("Ошибка установки указателя файла");
        //     fclose(file);
        //     return 1;
        // }
        // printf("%d", virtual_address_id);
        
        int virtual_address = virtual_addresses[virtual_address_id];
        int page = virtual_address >> 8;
        int offset = virtual_address & 0x00FF;
        long frame = -1;
        int tlb_size_temp = tlb_size;
        int page_table_size_temp = page_table_size;

        frame = get_frame_by_page_from_tlb(page, tlb_size_temp);
        if (frame > 0)
            tlb_hits++;

        if (frame < 0) { // miss
            frame = get_frame_by_page_from_page_tab(page, page_table_size_temp);

            if (frame < 0) { // page not found, need to allocate a new page
                int free_page = -1;

                free_page = get_free_ram_page_id();
                if (free_page >= 0)
                    claim_table[free_page][0] = 1;

                if (free_page < 0) { // free frame not found
                    free_page = find_least_used_ram_page_id();

                    delete_page_table_rows_by_frame(free_page, page_table_size_temp);
                    delete_tlb_rows_by_frame(free_page, tlb_size_temp);
                }

                // memcpy(memory + free_page * PAGE_SIZE, backing_store + page * PAGE_SIZE, PAGE_SIZE);
                
                // char test_read_buffer[PAGE_SIZE];
                // memcpy(test_read_buffer, backing_store + page * PAGE_SIZE, PAGE_SIZE);

                // rewind(file);
                char read_buffer[PAGE_SIZE];
                if (fseek(file, page * PAGE_SIZE, SEEK_SET) != 0) {
                    perror("Ошибка установки указателя файла");
                    fclose(file);
                    return 1;
                }
                size_t bytesRead = fread(read_buffer, 1, PAGE_SIZE, file);
                memcpy(memory + free_page * PAGE_SIZE, read_buffer, PAGE_SIZE);

                // for (int uu = 0; uu < PAGE_SIZE; uu++)
                // {
                //     if (test_read_buffer[uu] != read_buffer[uu]){
                //         inequality_count++;
                //         printf("id: %d      inq: %d     uu: %d\n", virtual_address_id, inequality_count, uu);
                //         break;
                //     }
                // }
                frame = free_page;

                page_table_add(page, free_page);
                tlb_add(page, free_page);

                claim_table[free_page][1]++;
                page_faults++;
            }
            
        }
        
        int value = memory[frame * PAGE_SIZE + offset];
        int phys_addr = frame * PAGE_SIZE + offset;
        
        fprintf(result, "Virtual address: %d Physical address: %d Value: %d\n", virtual_address, phys_addr, value);
        total++;
        
    }

    float tlb_hit_rate = tlb_hits / (float)total;
    float page_fault_rate = page_faults / (float)total;
    printf("Частота попаданий в TLB: %f\nЧастота ошибок страниц: %f\n", tlb_hit_rate, page_fault_rate);

    fclose(addreses);
    fclose(result);
    fclose(file);

    return 0;
}