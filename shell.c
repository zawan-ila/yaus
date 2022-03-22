#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<stdbool.h>
#include<fcntl.h>
#include<string.h>
#include<sys/wait.h>

int pipe_len = 0;
int is_concurrent = 0;

typedef struct command_info{
    char** args;
    union {
        int out_fd;
        char* out_file;
    } out;

    union {
        int in_fd;
        char* in_file;
    } in;

} command_info;

command_info* make_command_info(char** a, char* out, char* in){
    command_info* comm_struct_ptr = malloc(sizeof(command_info));
    comm_struct_ptr -> args = a;
    (*comm_struct_ptr) . out . out_file = out;
    (*comm_struct_ptr) . in . in_file = in;
    return comm_struct_ptr;
}

char* ParseWord(char** line){
    char * l = *line;
    while (*l == ' ')
        l++;
    
    char* w = l;
    while(*l && *l != ' '){
        l++;
    }

    if(*l){
        *l = 0;
        l++;
        while(*l == ' ')
            l ++;
    }

    *line = l;

    return w;
}

char** ParseCommand(char* atom){
    // Parse stuff like "   ls   -l "
    // Assume whitespace delimitation


    int atom_size = 1;
    char** args = malloc(sizeof(char*) * atom_size);

    int atoms = 0;

    char* word; 
    
    while(*atom){

        word = ParseWord(&atom);

        if(*word){

            args[atoms++ ] = word;

            if (atoms >= atom_size){
                atom_size *= 2;
                args = realloc(args, sizeof(char*) * atom_size);
            }

        }

        else{

            break;
        
        }


    }

    args[atoms] = NULL;
    
    return args;

}


command_info* ParseSimpleCommand(char* sim_cmd){
    // sim_cmd looks like "  ls  -l  > foo.txt < bar.txt  "

    char* in_file = strchr(sim_cmd, '<');
    char* out_file = strchr(sim_cmd, '>');


    if (in_file){
        *in_file = 0;
        in_file ++;

    }

    if(out_file){
        *out_file = 0;
        out_file ++;
    }

    char** args = ParseCommand(sim_cmd);

    if (in_file){
        in_file = ParseWord(& in_file);
    }

    if (out_file){
        out_file = ParseWord(& out_file);
    }

    command_info* s = make_command_info(args, out_file, in_file);
    return s;

}

command_info** ParseComplexCommand(char* cmd){
    // cmd looks like "ls -l | grep the | cat > foo.txt"

    char* sim_cmd = strtok(cmd, "|");

    int chains = 1;
    int chain = 0;
    pipe_len = 0;
    command_info** cmd_struct_array = malloc(sizeof(command_info*) * chains);

    while (sim_cmd){
        pipe_len++;
        command_info* cmd_str = ParseSimpleCommand(sim_cmd);
        cmd_struct_array[chain++] = cmd_str;
        if (chain >= chains){
            chains *= 2;
            cmd_struct_array = realloc(cmd_struct_array, sizeof(command_info*) * chains);
        }        
        
        sim_cmd = strtok(NULL, "|");

    }


    cmd_struct_array[chain] = NULL;

    return cmd_struct_array;

}


void SetFileDescriptors(command_info** comm_arr){
    for(int t = 0; t < pipe_len - 1; t++){
        int fd[2];
        pipe(fd);
        (*comm_arr[t]).out.out_fd = fd[1];
        (*comm_arr[t + 1]).in.in_fd = fd[0];
    }

    if ( (* comm_arr[0]).in.in_file != NULL){

        int fd = open((* comm_arr[0]).in.in_file, O_RDONLY, 0770);
        (* comm_arr[0]).in.in_fd = fd;
    }
    else{
        (* comm_arr[0]).in.in_fd = -1;

    }

    if ( (* comm_arr[pipe_len - 1]).out.out_file != NULL){

        int fd = open((* comm_arr[pipe_len - 1]).out.out_file,   O_WRONLY | O_CREAT |O_TRUNC, 0770);
        (* comm_arr[pipe_len - 1]).out.out_fd = fd;
    }
    else{
        (* comm_arr[pipe_len - 1]).out.out_fd = -1;
    }

}

void CLOSE(int n){
    if (n >= 0){
        close(n);
    }
}

pid_t MakeBaby(command_info** comm_struct_arr, int idx){
    // spawn a child // exec the child // return its pid

    pid_t pid = fork();

    if (pid == 0){
        // child
        
        int i_fd = (* comm_struct_arr[idx]).in.in_fd;
        int o_fd = (* comm_struct_arr[idx]).out.out_fd;

        if (i_fd){
            dup2(i_fd, STDIN_FILENO);
        }
        if (o_fd){
            dup2(o_fd, STDOUT_FILENO);
        }

        for(int i = 0; i < pipe_len ; i++){

            CLOSE( (* comm_struct_arr[i]).in.in_fd);
            CLOSE( (* comm_struct_arr[i]).out.out_fd);

        }

        execvp((* comm_struct_arr[idx]).args[0], (* comm_struct_arr[idx]).args);

    }
    else if (pid > 0){
        CLOSE( (* comm_struct_arr[idx]).in.in_fd);
        CLOSE( (* comm_struct_arr[idx]).out.out_fd);

        return pid;
    }

    else{
        printf("Fork Failed\n");
        exit(1);
    }

}


bool IsConcurrent(char* inp){
    int last_idx = strlen(inp);
    last_idx --;
    inp[last_idx] = 0;
    last_idx --;
    while (last_idx >= 0 && inp[last_idx] == ' '){
        last_idx --;
    }

    if (last_idx < 0){
        is_concurrent = 0;
        return 0;
    }

    else{
        if (inp[last_idx] == '&'){
            inp[last_idx] = '\0';
            is_concurrent = 1;
            return 1;
        }

        is_concurrent = 0;
        return 0;
    }

}

void Handle(char* cmd){

    IsConcurrent(cmd);
        
    command_info** cmd_struct_arr = ParseComplexCommand(cmd);

    if (strcmp((* cmd_struct_arr[0]).args[0],"cd") == 0){
        chdir((* cmd_struct_arr[0]).args[1]);
    }

    else if (strcmp((* cmd_struct_arr[0]).args[0],"exit") == 0){

        exit(0);
    }

    else{

        SetFileDescriptors(cmd_struct_arr);
        
        for(int comm_num = 0; comm_num < pipe_len - 1; comm_num ++){
            MakeBaby(cmd_struct_arr, comm_num);
        }

        pid_t last_child = MakeBaby(cmd_struct_arr, pipe_len - 1);

        if (! is_concurrent){
            waitpid(last_child, NULL, 0);
        }

    }

    for (int tmp = 0; tmp < pipe_len; tmp++){
        free((* cmd_struct_arr[tmp]).args);
        free(cmd_struct_arr[tmp]);
    }

    free(cmd_struct_arr);

}

int main () {
   
    char* line;

    while ( 1 ){
        line = NULL;
        printf("YAUS>");
        size_t buf_len = 0;

        getline(&line, &buf_len, stdin);
        Handle(line);
        free(line);

    }

}