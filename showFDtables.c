#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>


//Linkedlist struct for pid, fd, filename, inode 
typedef struct pidstruct {
    int node_pid;
    int fd;
    char file_name[1000];
    int inode;
    struct pidstruct* next;
} pidstruct;

//Linkedlist struct for summary and threshold table
typedef struct pidcountstruct {
    int node_pid;
    int count;
    struct pidcountstruct *next;
} pidcountstruct;


void parse_arguments(int argc, char *argv[], int *per_process, int *systemWide, int *Vnodes, int *composite, int *summary, int *threshold, int *pid, int* output_txt, int* output_b);
void print_header(int table_type);
void loop_pid(int pid, pidstruct** head);
void loop_fd(int pid, pidstruct** head);
void create_node(int pid, char *fd, char *file_name, ino_t inode, pidstruct** head);
void print_table(pidstruct* head, int table_type);
void summary_count(int pid, pidcountstruct** counthead);
void summary_list(pidstruct* head, pidcountstruct** counthead);
void print_summary(pidcountstruct* counthead);
void print_threshold(pidcountstruct* head, int threshold);
void output_text(pidstruct* head);
void output_binary(pidstruct* head);


int main(int argc, char *argv[]){
    ///_|> descry: main function to run program
    ///_|> argc: argument count
    ///_|> argv: argument flags
    ///_|> returning: return 0 after program ends

    // Process arguments
    int per_process, systemWide, Vnodes, composite, summary, threshold, pid, output_txt, output_b;
    per_process = 0; systemWide = 0; Vnodes = 0; composite = 0; summary = 0; threshold = -1; pid = -1; output_txt = 0; output_b = 0;
    parse_arguments(argc, argv, &per_process, &systemWide, &Vnodes, &composite, &summary, &threshold, &pid, &output_txt, &output_b);

    // If there is specific pid in argument, check if it is valid
    if (pid != -1){
        DIR *pdir;
        char file_path[1000]; 
        sprintf(file_path, "/proc/%d/fd", pid);
        pdir = opendir(file_path);
        if (pdir == NULL) {
            fprintf(stderr, "pid non valid\n");
            closedir(pdir);
            exit(1);
        }
        closedir(pdir);
    }

    // Generate linked list of all info
    pidstruct* head = NULL;
    loop_pid(pid, &head);

    //print tables 
    if (per_process == 1) {print_table(head, 1);}
    if (systemWide == 1) {print_table(head,2);}
    if (Vnodes == 1) {print_table(head, 3);}
    if (composite == 1) {print_table(head, 4);}
    if (output_txt == 1) {output_text(head);}
    if (output_b == 1) {output_binary(head);}

    if (summary == 1 || threshold != -1){

        // create linked list that counts occurances of pid for each fd
        pidcountstruct* counthead = NULL;
        summary_list(head, &counthead);

        // print summary and threshold graphs
        if (summary == 1){print_summary(counthead);}
        if (threshold != -1) {print_threshold(counthead, threshold);}
        
        // free linkedlist
        pidcountstruct* temp;
        while (counthead != NULL) {
            temp = counthead;
            counthead = counthead->next;
            free(temp);
        }
    }
    // free linkedlist
    pidstruct* temp;
    while (head != NULL) {
        temp = head;
        head = head->next;
        free(temp);
    }
    return 0;
}

void loop_pid(int pid, pidstruct** head){
    ///_|> descry: if no pid is given, loops through /proc to find all active pids and calls on loop_fd, else call on loop_fd using given pid
    ///_|> pid: stores pid argument 
    ///_|> head: pidstruct linkedlist head to pass on
    ///_|> returning: returns nothing   

    DIR *dir;
    struct dirent *dp; 

    // Check if user directory can be openned if no specifc pid
    if (pid == -1) {
        dir = opendir("/proc");  
        if (dir == NULL) {
            fprintf(stderr, "Cannot open current file directory\n");
            closedir(dir);
            exit(1);
        }

        // loops through for each pid
        while ((dp = readdir (dir)) != NULL) {  
            char file_name[256];
            strcpy(file_name, dp->d_name);

            // check if pid is a digit
            if (isdigit(file_name[0])){
                int pidvar = (int)(strtol(file_name, NULL,10)); 

                // find info about pid and add to linkedlist
               loop_fd(pidvar, head);
            }
        }
        closedir(dir);
    } else {
        // specific pid inputed
        loop_fd(pid, head);  
    }
}

void loop_fd(int pid, pidstruct** head){
    //_|> descry: for each pid, find all fds, the find corresponding filename and inode. Calls create_node to store in linked list 
    ///_|> pid: stores pid argument 
    ///_|> head: pidstruct linkedlist head to pass on
    ///_|> returning: returns nothing 

    DIR *pdir;
    struct dirent *pdp;
    
    // create path
    char file_path[1000]; 
    sprintf(file_path, "/proc/%d/fd", pid);

    // Open fds for pid
    pdir = opendir(file_path);
    if (pdir == NULL) {
        closedir(pdir);
        return;
    }

    // Skips first two directories . and ..
    readdir(pdir); 
    readdir(pdir); 

    // loop through each pid, fd pair
    while ((pdp = readdir(pdir)) != NULL) {  

        //store fd
        char fd[256];
        strcpy(fd, pdp->d_name); 

        // Store symbolic link
        char file_name[1000] = {"\0"};
        char fd_path[1000];
        sprintf(fd_path, "/proc/%d/fd/%s", pid, fd);

        // Find file_name
        size_t link_return = readlink(fd_path, file_name, sizeof(file_name));
        if ((int)link_return != -1){

            //readlink does not null terminate
            file_name[link_return] = '\0'; 
        } else if ((int)link_return == -1){

            // Cannot access file_name, create pid and fd pair only
            create_node(pid, fd, "None", -1, head);
            closedir(pdir);
            return;
        }

        // find inode num
        struct stat sb;
        ino_t inode = 0;
        int stat_return =  lstat(file_name, &sb);
        if (stat_return == 0){
            inode = sb.st_ino;
        } else {

            // if file_name is a pipe/socket, use the inode in file_name. If anon_inode, show as 0
            char sentence[500];
            strcpy(sentence, file_name);
            char *inode_str = strtok(sentence, "[:]");
            if (inode_str != NULL) {
                inode_str = strtok(NULL, "[:]"); 
                if (inode_str != NULL) {
                    inode = strtol(inode_str, NULL, 10); 
                } else {
                    inode = -1; // Erro val
                }
            }
        }

        //link entry to linkedlist
        create_node(pid, fd, file_name, inode, head);
    }   

    closedir(pdir);
}


void print_table(pidstruct* head, int table_type){
    ///_|> descry: loops through pidstruct linked list to print per-process systemwide vnode or composite table, given table_type
    ///_|> head: head of pid struct linkedlist 
    ///_|> table_type: stores which table to print
    ///_|> returning: returns nothing 

    //print header
    print_header(table_type);

    // iterate through linked list
    pidstruct* curr = head;
    while (curr != NULL) {

        //store vals
        int pid = curr->node_pid;
        int fd = curr->fd;
        char file_name[1000];
        strcpy(file_name, curr->file_name);
        int inode = curr->inode;

        //print vals
        if (table_type == 1){
            printf("         %d       %d\n", pid, fd);
        } else if (table_type == 2 && strcmp("None", file_name) != 0){
            printf("         %d     %d       %s\n", pid, fd, file_name);
        } else if (table_type == 3){
            printf("         %d      %d\n", fd, inode);
        } else if (table_type == 4 && strcmp("None", file_name) != 0){
            printf("       %d        %d       %s     %d\n",pid, fd, file_name, inode); 
        }
        curr = curr->next;
    }
    printf("       ========================================================\n");
}

void print_header(int table_type){
    //_|> descry: helper function to print header
    ///_|> table_type: stores table type
    ///_|> returning: returns nothing 

    switch (table_type) {
        case 1:
            printf("         PID    FD \n");
            printf("        ========================================================\n");
          break;
        case 2:
            printf("         PID    FD      Filename \n");  
            printf("        ========================================================\n");
            break;
        case 3:
            printf("         FD     Inode\n");
            printf("        ========================================================\n");
          break;
        case 4:
              printf("         PID    FD      Filename       Inode\n");
            printf("        ========================================================\n");
          break;
      }
    }  

void parse_arguments(int argc, char *argv[], int *per_process, int *systemWide, int *Vnodes, int *composite, int *summary, int *threshold, int *pid, int* output_txt, int* output_b)
{
    //_|> descry: processes all argument and updates pointers in main
    //_|> argc: argument count
    //_|> argv: input
    //_|> per_process: per-process flag
    //_|> systemWide: systemWide flag
    //_|> Vnodes: vnodes flag
    //_|> composite: composite flag
    //_|> summary: summary flag
    //_|> threshold: threshold value
    //_|> pid: pid value
    //_|> output_txt: output ascii file flag
    //_|> output_b: output binary file flag
    ///_|> returning: returns nothing 

    int arg_num = 1;
    for (; arg_num < argc; arg_num++)
    {
        if (isdigit(argv[arg_num][0])){*pid = atoi(argv[arg_num]);}
        else if (strcmp(argv[arg_num], "--per-process") == 0){*per_process = 1;}
        else if (strcmp(argv[arg_num], "--systemWide") == 0){*systemWide = 1;}
        else if (strcmp(argv[arg_num], "--Vnodes") == 0){*Vnodes = 1;}
        else if (strcmp(argv[arg_num], "--composite") == 0){*composite = 1;}
        else if (strcmp(argv[arg_num], "--summary") == 0){*summary = 1;}
        else if (strncmp(argv[arg_num], "--threshold=", 12) == 0){*threshold = atoi(argv[arg_num] + 12);}
        else if (strcmp(argv[arg_num], "--output_TXT") == 0){*output_txt = 1;}
        else if (strcmp(argv[arg_num], "--output_binary") == 0){*output_b = 1; } 
        else    
        {
            //incorrect arguments
            fprintf(stderr, "Arguments incorrect\n");
            exit(1);
        }
    }

    // no arguments not including output file, print composite
    if (*per_process == 0 && *systemWide == 0 && *Vnodes == 0 && *composite == 0 && *summary == 0 && *threshold == -1) {
        *composite = 1;
    }
}

void create_node(int pid, char *fd, char *file_name, ino_t inode, pidstruct** head){
    //_|> descry: creates a node to store pid,fd,filename and inode in pidstruct linkedlist
    ///_|> pid: pid of current loop
    //_|> fd: fd of current loop
    //_|>filename: corresponding filename
    //_|>inode: corresponding inode
    //_|> head: head pointor for pidstruct linkedlist
    ///_|> returning: returns nothing 

    // find last node
    pidstruct* curr = *head;
    pidstruct* prev = NULL;
    while (curr!= NULL){
        prev = curr;
        curr = curr->next;
    }

    // create node
    pidstruct* node =  (pidstruct* )malloc(sizeof (pidstruct));
    if (node == NULL) {
        fprintf(stderr, "Insufficient memory");
        return;
    }

    //insert vals
    node->node_pid = pid;
    node->fd = (int)strtol(fd, NULL, 10 );
    strncpy(node->file_name, file_name, sizeof(node->file_name));
    node->inode = inode;
    node->next = NULL;

    // if no head node ,set as head
    if (*head == NULL){
        *head = node;
    } else {

        //link at end
        prev -> next = node;
    }
}

void summary_list(pidstruct* head, pidcountstruct** counthead){
    //_|> descry: iterates through pidstruct linkedlist and calls on summary_count to create pidcountstruct list
    ///_|> head: head pointer of pidstruct
    //_|> counthead: head pointer of pidcountstruct
    ///_|> returning: returns nothing 

    //iterate through pidstruct linked list
    pidstruct* curr = head;
    while (curr!= NULL){
        int pid = curr->node_pid;

        //count num of occurrances for each pid
        summary_count(pid, counthead);
        curr = curr->next;
    }
}

void summary_count(int pid, pidcountstruct** counthead){
    //_|> descry: iterates through pidcountstruct linked list, if there exists pid num count+1, else create new node for pid val
    //_|> counthead: head pointer of pidcountstruct
    ///_|> returning: returns nothing 

    //iterate through pidcountstruct list
    pidcountstruct* curr = *counthead;
    pidcountstruct* prev = NULL;

    while (curr!= NULL){

        // if matching pid, count++
        if(curr->node_pid == pid){
            curr -> count += 1;
            return; 
        }

        // Non matching pid, move through list
        prev = curr;
        curr = curr->next;
    }

    //no matching pid, create new node
    pidcountstruct* node = (pidcountstruct *)malloc(sizeof(pidcountstruct));
    if (node == NULL) {
        fprintf(stderr, "Insufficient memory");
        return;
    }
    node->node_pid = pid;
    node->count = 1;
    node->next = NULL;

    //if no head, set as head node, otherwise link at end
    if (*counthead == NULL){
        *counthead = node;
    } else {
        prev -> next = node;
    }
}

void print_summary(pidcountstruct* counthead) {
    //_|> descry: loops through pidcountstruct to print pid and num of occurances
    //_|> counthead: head pointer of pidcountstruct
    ///_|> returning: returns nothing 

    pidcountstruct* curr = counthead;
    printf("         Summary Table\n");
    printf("         =============\n");

    //iterate through pidcountstruct to print 
    while (curr != NULL) {
        printf("%d (%d),  ", curr->node_pid, curr->count);
        curr = curr->next;
    }
    printf("\n\n");
}


void print_threshold(pidcountstruct* counthead, int threshold) {
    //_|> descry: iterates through pidcountstruct linked list, prints threshold table by comparing threshold to count 
    //_|> counthead: head pointer of pidcountstruct
    ///_|> returning: returns nothing 

    pidcountstruct* curr = counthead;
    printf("## Offending processes:\n");

    //iterate through pidcountstruct to print 
    while (curr != NULL) {
        // Add threshold condition
        if (curr->count >= threshold){
            printf("%d (%d),  ", curr->node_pid, curr->count);
        }
        curr = curr->next;
    }
    printf("\n\n");
}

void output_text(pidstruct* head ){
    //_|> descry: saves composite table to txt file
    //_|> head: head pointer for pidstruct
    ///_|> returning: returns nothing 

    // generate file or overwrite
    FILE *fptr;
    fptr = fopen("compositeTable.txt", "w");
    if (fptr == NULL) {
        fprintf(stderr, "Error creating file");
        return;
    }

    //write to file
    fprintf(fptr, "        PID    FD      Filename       Inode\n");
    fprintf(fptr, "       ========================================================\n");
    pidstruct* curr = head;

    // iterate through pidstruct list to print composite table
    while (curr != NULL) {
            int pid = curr->node_pid;
            int fd = curr->fd;
            char file_name[1000];
            strcpy(file_name, curr->file_name);
            int inode = curr->inode;
            fprintf(fptr, "       %d        %d       %s     %d\n",pid, fd, file_name, inode); 
            curr = curr->next;
        }
        fprintf(fptr,   "       ========================================================\n");
    fclose(fptr);
    }

void output_binary(pidstruct* head){
    //_|> descry: saves composite table to binary file
    //_|> head: head pointer for pidstruct
    ///_|> returning: returns nothing 

    // generate file or overwrite
    FILE *fptr;
    fptr = fopen("compositeTable.bin", "wb");
    if (fptr == NULL) {
        fprintf(stderr, "Error creating file\n");
        return;
    }

    //write to file
    char line[1200];
    sprintf(line, "        PID    FD      Filename       Inode\n");
    fwrite(line, sizeof(char), strlen(line), fptr);
    sprintf(line, "       ========================================================\n");
    fwrite(line, sizeof(char), strlen(line), fptr);

    // iterate through pidstruct list to print composite table
    
    pidstruct* curr = head;
    while (curr != NULL) {
            int pid = curr->node_pid;
            int fd = curr->fd;
            char file_name[1000];
            strcpy(file_name, curr->file_name);
            int inode = curr->inode;
            sprintf(line ,"       %d        %d       %s     %d\n",pid, fd, file_name, inode);
            fwrite(line, sizeof(char), strlen(line), fptr);
            curr = curr->next;
        }
        sprintf(line,   "       ========================================================\n");
        fwrite(line, sizeof(char), strlen(line), fptr);
    fclose(fptr);
}

    