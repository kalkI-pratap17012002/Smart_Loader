#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <elf.h>
#include <unistd.h>
#include <sys/mman.h>
#include <signal.h>

#define PAGE_SIZE 4096

/* Quick list to keep track of allocated pages for cleanup later */
struct allocated_page 
{
    void *virtual_address;
    struct allocated_page *next;
};

/* Global stuff for the loader */
Elf32_Ehdr *elf_header;          /* ELF header pointer */
Elf32_Phdr *program_headers;     /* Program headers array */
int *segment_page_count;         /* Pages per segment tracker */
int elf_fd;                      /* File descriptor for the ELF */
int total_fragmentation = 0;     /* Total wasted space */
int fault_count = 0;             /* Page faults we've handled */
int allocation_count = 0;        /* Pages we've allocated */
struct allocated_page *page_list_head = NULL;  /* Head of the page list */

/* Tried adding a debug flag for extra logging, but ended up not using it */
/*
int debug_flag = 0;
*/

/* Function prototypes */
void load_and_execute_elf(char **executable_path);
static void handle_segmentation_fault(int signal_number, siginfo_t *fault_info, void *context);
void cleanup_loader();

int main(int argc, char **argv) 
{
    /* Added this counter for some random tracking, but commented it out after testing */
    /*
    int extra_counter = 0;
    */

    if (argc != 2) 
    {
        printf("Usage: %s <ELF Executable>\n", argv[0]);
        exit(1);
    }

    /* Set up SIGSEGV handler to catch page faults */
    struct sigaction segfault_action;
    memset(&segfault_action, 0, sizeof(segfault_action));
    segfault_action.sa_flags = SA_SIGINFO;  /* Use sa_sigaction */
    segfault_action.sa_sigaction = handle_segmentation_fault;
    
    if (sigaction(SIGSEGV, &segfault_action, NULL) == -1) 
    {
        perror("sigaction failed");
        exit(1);
    }

    load_and_execute_elf(&argv[1]);

    /* Spit out some stats after the program runs */
    printf("\nExecution Statistics:\n");
    printf("Total Page Faults: %d\n", fault_count);
    printf("Total Page Allocations: %d\n", allocation_count);
    printf("Internal Fragmentation: %d Bytes (%.3f KB)\n", 
           total_fragmentation, total_fragmentation / 1024.0);
    
    cleanup_loader();
    return 0;
}

void load_and_execute_elf(char **exe_path) 
{
    /* Tried a temp buffer for reading, but didn't need it in the end */
    /*
    char *temp_buf = NULL;
    */

    /* Open the ELF file */
    elf_fd = open(*exe_path, O_RDONLY);
    if (elf_fd < 0) 
    {
        perror("Failed to open ELF file");
        exit(1);
    }

    /* Grab some memory for the ELF header */
    elf_header = (Elf32_Ehdr *)malloc(sizeof(Elf32_Ehdr));
    if (elf_header == NULL) 
    {
        perror("malloc failed for ELF header");
        exit(1);
    }

    /* Read the header */
    if (lseek(elf_fd, 0, SEEK_SET) < 0) 
    {
        perror("lseek failed");
        exit(1);
    }

    if (read(elf_fd, elf_header, sizeof(Elf32_Ehdr)) < 0) 
    {
        perror("Failed to read ELF header");
        exit(1);
    }

    /* Check if it's actually an ELF file */
    if (elf_header->e_ident[0] != ELFMAG0 || 
        elf_header->e_ident[1] != ELFMAG1 || 
        elf_header->e_ident[2] != ELFMAG2 || 
        elf_header->e_ident[3] != ELFMAG3) 
    {
        printf("Error: Not a valid ELF file\n");
        exit(1);
    }

    /* Make sure it's 32-bit */
    if (elf_header->e_ident[EI_CLASS] != ELFCLASS32) 
    {
        printf("Error: ELF file is not 32-bit (compile with -m32 flag)\n");
        exit(1);
    }

    /* Set up array to count pages per segment */
    segment_page_count = (int *)calloc(elf_header->e_phnum, sizeof(int));
    if (segment_page_count == NULL) 
    {
        perror("calloc failed for segment page count");
        exit(1);
    }

    /* Jump to the entry point - this will segfault and get handled */
    int (*entry_point)() = (int (*)())elf_header->e_entry;

    printf("Starting ELF execution...\n");
    int return_value = entry_point();
    printf("Program returned: %d\n", return_value);
}

/* Check if a page is already mapped to avoid double work */
int is_page_mapped(unsigned int addr) 
{
    struct allocated_page *walker = page_list_head;
    while (walker != NULL) 
    {
        if ((unsigned int)walker->virtual_address == addr) 
        {
            return 1;
        }
        walker = walker->next;
    }
    return 0;
}

/* Handle SIGSEGV for lazy loading - allocate pages on demand */
static void handle_segmentation_fault(int sig_num, siginfo_t *info, void *ctx) 
{
    /* Added error tracker for debugging faults, but commented out since it wasn't helpful */
    /*
    int error_tracker = 0;
    */

    if (sig_num != SIGSEGV) 
    {
        return;
    }

    /* First time around, load the program headers */
    if (fault_count == 0) 
    {
        program_headers = (Elf32_Phdr *)malloc(sizeof(Elf32_Phdr) * elf_header->e_phnum);
        if (program_headers == NULL) 
        {
            perror("malloc failed for program headers");
            exit(1);
        }

        if (lseek(elf_fd, elf_header->e_phoff, SEEK_SET) < 0) 
        {
            perror("lseek failed for program headers");
            exit(1);
        }

        if (read(elf_fd, program_headers, sizeof(Elf32_Phdr) * elf_header->e_phnum) < 0) 
        {
            perror("Failed to read program headers");
            exit(1);
        }
    }

    /* Get the address that caused the fault */
    unsigned int faulty_addr = (unsigned int)info->si_addr;
    
    /* Align to page boundary */
    unsigned int aligned_page = faulty_addr & ~(PAGE_SIZE - 1);

    /* If it's already mapped, something's wrong */
    if (is_page_mapped(aligned_page)) 
    {
        printf("Error: Double fault at address: 0x%x\n", faulty_addr);
        exit(1);
    }

    /* Find which segment this address belongs to */
    int seg_idx = -1;
    Elf32_Phdr *seg = NULL;

    for (int i = 0; i < elf_header->e_phnum; i++) 
    {
        if (faulty_addr >= program_headers[i].p_vaddr && 
            faulty_addr < program_headers[i].p_vaddr + program_headers[i].p_memsz) 
        {
            seg = &program_headers[i];
            seg_idx = i;
            break;
        }
    }

    /* If not in any segment, bail */
    if (seg == NULL) 
    {
        printf("Error: Segmentation fault at invalid address: 0x%x\n", faulty_addr);
        exit(1);
    }

    /* Figure out where this page sits in the segment */
    unsigned int seg_start = seg->p_vaddr;
    unsigned int seg_offset = aligned_page - seg_start;

    /* Map just this page */
    void *page = mmap((void *)aligned_page, PAGE_SIZE, 
                             PROT_READ | PROT_WRITE | PROT_EXEC, 
                             MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED, -1, 0);
    
    if (page == MAP_FAILED) 
    {
        perror("mmap failed");
        exit(1);
    }

    /* Load data from the file into the page */
    unsigned int file_pos = seg->p_offset + seg_offset;
    
    unsigned int left_in_seg = seg->p_filesz > seg_offset 
                                        ? seg->p_filesz - seg_offset 
                                        : 0;
    
    unsigned int bytes = left_in_seg < PAGE_SIZE 
                                 ? left_in_seg 
                                 : PAGE_SIZE;

    if (bytes > 0) 
    {
        if (lseek(elf_fd, file_pos, SEEK_SET) < 0) 
        {
            perror("lseek failed for segment data");
            exit(1);
        }

        if (read(elf_fd, page, bytes) < 0) 
        {
            perror("Failed to read segment data");
            exit(1);
        }
    }

    /* Update our counters */
    fault_count++;
    allocation_count++;
    segment_page_count[seg_idx]++;

    /* Calc fragmentation when we finish a segment */
    unsigned int total_pages = (seg->p_memsz + PAGE_SIZE - 1) / PAGE_SIZE;
    if (segment_page_count[seg_idx] == total_pages) 
    {
        unsigned int last_used = seg->p_memsz % PAGE_SIZE;
        if (last_used > 0) 
        {
            total_fragmentation += (PAGE_SIZE - last_used);
        }
    }

    /* Track this page for cleanup */
    struct allocated_page *new_node = (struct allocated_page *)malloc(sizeof(struct allocated_page));
    if (new_node == NULL) 
    {
        perror("malloc failed for page node");
        exit(1);
    }
    new_node->virtual_address = page;
    new_node->next = page_list_head;
    page_list_head = new_node;
    
    /* Back to the program */
}

/* Clean up everything */
void cleanup_loader() 
{
    struct allocated_page *curr = page_list_head;
    struct allocated_page *tmp;

    /* Unmap all pages */
    while (curr != NULL) 
    {
        if (munmap(curr->virtual_address, PAGE_SIZE) < 0) 
        {
            perror("munmap failed");
        }
        tmp = curr;
        curr = curr->next;
        free(tmp);
    }

    /* Close the file */
    if (close(elf_fd) == -1) 
    {
        perror("close failed");
    }

    /* Free the rest */
    free(segment_page_count);
    free(program_headers);
    free(elf_header);
}

