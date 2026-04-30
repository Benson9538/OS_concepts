// myshell.c 關卡 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include <fcntl.h>

#define MAX_LINE  1024
#define MAX_ARGS  64

typedef struct {
    char *args[MAX_ARGS]; // 指令和參數，例如 ["ls", "-l", NULL]
    char *input_file;     // < 後面的檔名，沒有就是 NULL
    char *output_file;    // > 後面的檔名，沒有就是 NULL
    int append;           // 是否使用 >> 追加模式
} Command;

/*
 * parse()：把一行字串切成一個個 token
 *
 * 例如輸入 "ls -l /tmp"
 * 切完後 args[0]="ls", args[1]="-l", args[2]="/tmp", args[3]=NULL
 *
 * exec 系列函式要求最後一個參數是 NULL，這裡幫你處理好

int parse(char *line, char **args) {
    int count = 0;

    
     * strtok：用空白和 \t 當分隔符號，切割字串
     * 第一次呼叫傳入字串，後續傳入 NULL 繼續切同一個字串
     * 注意：strtok 會修改原本的字串（插入 \0），所以不能傳常數字串
    
    char *token = strtok(line, " \t\n");
    while (token != NULL && count < MAX_ARGS - 1) {
        args[count++] = token;
        token = strtok(NULL, " \t\n");
    }
    args[count] = NULL;   // exec 需要 NULL 結尾
    return count;
}
*/

/*
 * parse()：切割字串，同時辨識 > 和 < 符號
 *
 * 輸入：一行字串，例如 "ls -l > output.txt"
 * 輸出：填好 Command 結構
 */
void parse(char *line , Command *cmd){
    cmd->input_file = NULL;
    cmd->output_file = NULL;
    cmd->append = 0;

    int count = 0;
    char *token = strtok(line, " \t\n");
    
    while(token != NULL){
        if(strcmp(token, ">") == 0){
            /*
             * 遇到 >，下一個 token 是輸出檔名
             * 不把 > 放進 args，只記錄檔名
             */
            cmd->output_file = strtok(NULL, " \t\n");
            cmd->append = 0;            
        } else if(strcmp(token, ">>") == 0){
            /*
            * 遇到 >>，追加模式
            * 和 > 的差別：不清空檔案，從尾端開始寫
            */           
            cmd->output_file = strtok(NULL, " \t\n");
            cmd->append = 1;
        } else if(strcmp(token, "<") == 0){
            /*
             * 遇到 <，下一個 token 是輸入檔名
             */
            cmd->input_file = strtok(NULL, " \t\n");            
        } else {
            cmd->args[count++] = token;
        }

        token = strtok(NULL, " \t\n");
    }

    cmd->args[count] = NULL;
}

/*
 * setup_redirection()：在子行程裡設定 I/O redirection
 *
 * 這個函式在 fork 之後、execvp 之前呼叫
 * 它修改的是子行程的 fd table，不影響父行程
 */
void setup_redirection(Command *cmd){
    if(cmd->input_file != NULL){
        /*
        * 處理輸入 redirection（<）
        *
        * O_RDONLY：唯讀開啟
        * 如果檔案不存在，open 回傳 -1
        */
        int fd = open(cmd->input_file , O_RDONLY);
        if(fd < 0){
            perror(cmd->input_file);
            exit(1);
        }
        /*
         * dup2(fd, 0)：把 stdin（fd 0）改成指向這個檔案
         * 之後程式讀 stdin，實際上是在讀這個檔案
         */
        dup2(fd, 0);
        close(fd);
    }
    if(cmd->output_file != NULL){
        /*
        * 處理輸出 redirection（> 或 >>）
        *
        * O_WRONLY：唯寫
        * O_CREAT：檔案不存在就建立
        * O_TRUNC：清空已存在的檔案（> 的行為）
        * O_APPEND：從尾端追加（>> 的行為）
        * 0644：建立時的權限（rw-r--r--）
        */
        int flags = O_WRONLY | O_CREAT;
        flags |= cmd->append ? O_APPEND : O_TRUNC;

        int fd = open(cmd->output_file, flags, 0644);
        if(fd < 0){
            perror(cmd->output_file);
            exit(1);
        }

        /*
         * dup2(fd, 1)：把 stdout（fd 1）改成指向這個檔案
         * 之後程式寫 stdout（printf 等），實際上寫進這個檔案
         */
        dup2(fd, 1);
        close(fd);
    }
}

/*
 * has_pipe()：檢查 args 裡有沒有 "|"
 * 回傳 "|" 在 args 裡的位置，沒有就回傳 -1
 */
int has_pipe(char **args){
    for(int i=0;args[i] != NULL;i++){
        if(strcmp(args[i], "|") == 0){
            return i;
        }
    }
    return -1;
}

/*
 * run_pipe()：執行 cmd1 | cmd2
 *
 * pipe_pos 是 "|" 在 args 裡的位置
 * args[0..pipe_pos-1] 是左邊的指令
 * args[pipe_pos+1..] 是右邊的指令
 */
void run_pipe(Command *cmd, int pipe_pos){
    int pipefd[2];
    
    /*
     * 建立 pipe
     * pipefd[0] = 讀端
     * pipefd[1] = 寫端
     */
    if(pipe(pipefd) < 0){
        perror("pipe");
        return;
    }

    /*
     * 把 args 切成左右兩個指令
     *
     * 左邊：cmd->args[0] 到 cmd->args[pipe_pos-1]
     * 右邊：cmd->args[pipe_pos+1] 之後
     *
     * 直接在原陣列上切：把 "|" 那個位置設成 NULL
     * 這樣左邊的 args 就自然以 NULL 結尾
     */
    cmd->args[pipe_pos] = NULL;
    char **left_args = cmd->args;
    char **right_args = cmd->args + pipe_pos + 1;

    // ── 子行程 1：執行左邊的指令，輸出寫進 pipe ──────
    pid_t pid1 = fork();
    if(pid1 < 0) { perror("fork"); return; }

    if(pid1 == 0){
        /*
         * 子行程 1 負責寫 pipe
         * 關掉讀端（不需要讀）
         * 把 stdout（fd 1）換成 pipe 的寫端
         */
        close(pipefd[0]); // 關掉讀端
        dup2(pipefd[1], 1); // stdout -> pipe 寫端
        close(pipefd[1]); // pipe 寫端已經複製到 stdout

        /*
         * 左邊的指令如果有 input redirection（< file）
         * 在這裡設定（只設定 input，output 已經換成 pipe 了）
         */
        if(cmd->input_file != NULL){
            int fd = open(cmd->input_file, O_RDONLY);
            if(fd < 0){ perror(cmd->input_file); exit(1); }
            dup2(fd, 0);
            close(fd);
        }

        execvp(left_args[0], left_args);
        perror(left_args[0]);
        exit(1);
    }

    // ── 子行程 2：執行右邊的指令，從 pipe 讀輸入 ────

    pid_t pid2 = fork();
    if(pid2 < 0) { perror("fork"); return; }

    if (pid2 == 0) {
        /*
         * 子行程 2 負責讀 pipe
         * 關掉寫端（不需要寫）
         * 把 stdin（fd 0）換成 pipe 的讀端
         */
        close(pipefd[1]);          // 關掉寫端
        dup2(pipefd[0], 0);        // stdin → pipe 讀端
        close(pipefd[0]);          // dup2 後原本的 fd 不需要了

        /*
         * 右邊的指令如果有 output redirection（> file）
         * 在這裡設定
         */
        if (cmd->output_file != NULL) {
            int flags = O_WRONLY | O_CREAT;
            flags |= cmd->append ? O_APPEND : O_TRUNC;
            int fd = open(cmd->output_file, flags, 0644);
            if (fd < 0) { perror(cmd->output_file); exit(1); }
            dup2(fd, 1);
            close(fd);
        }

        execvp(right_args[0], right_args);
        perror(right_args[0]);
        exit(1);
    }

    /*
     * 父行程：
     * 把 pipe 的兩端都關掉
     * 父行程不參與資料傳輸
     * 如果不關，子行程 2 永遠不會收到 EOF
     */
    close(pipefd[0]);
    close(pipefd[1]);

    // 等兩個子行程都結束
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
}

int main() {
    char  line[MAX_LINE];
    // char *args[MAX_ARGS];
    Command cmd;

    while (1) {
        /*
         * 提示符顯示目前的工作目錄
         * getcwd 取得目前目錄的完整路徑
         */
        char cwd[256];
        getcwd(cwd, sizeof(cwd));
        printf("%s$ ", cwd);
        // 印出提示符，fflush 確保立刻顯示（不被緩衝擋住）
        fflush(stdout);

        // 讀取一行輸入
        // fgets 讀到 \n 或 EOF 為止
        if (fgets(line, sizeof(line), stdin) == NULL) {
            // Ctrl+D 產生 EOF，這時候結束 shell
            printf("\n");
            break;
        }

        // 如果只按了 Enter（空行），跳過
        if (line[0] == '\n') continue;

        // 解析指令
        // int argc = parse(line, args);
        // if (argc == 0) continue;
        parse(line, &cmd);

        // ── 內建指令處理 ──────────────────────────────

        // exit
        // if (strcmp(args[0], "exit") == 0) break;
        if (strcmp(cmd.args[0], "exit") == 0) break;

        if(strcmp(cmd.args[0], "cd") == 0){
            /*
             * cd 必須由 shell 自己執行（不能 fork）
             * 因為改變的必須是 shell 自己的 CWD
             *
             * cd 沒有參數：回到 HOME 目錄
             * cd 有參數：切換到指定目錄
             */
            char *target = cmd.args[1]; 
            if(target == NULL){
                target = getenv("HOME");
            }
            if(chdir(target) < 0){
                perror(target);
            }
            continue;
        }

        /*
         * 檢查有沒有 pipe
         * 有的話走 run_pipe，沒有的話走普通的 fork+exec
         */
        int pipe_pos = has_pipe(cmd.args);

        if(pipe_pos >= 0){
            run_pipe(&cmd, pipe_pos);
        } else {
            // ── 外部指令：fork + exec ─────────────────────

            pid_t pid = fork();

            if (pid < 0) {
                perror("fork");
                continue;

            } else if (pid == 0) {
                /*
                * 子行程：執行指令
                *
                * execvp：
                *   v = 參數用陣列（vector）
                *   p = 自動搜尋 PATH 環境變數找程式
                *
                * 所以你可以直接打 "ls" 而不用打 "/bin/ls"
                */
                setup_redirection(&cmd);
                execvp(cmd.args[0], cmd.args);

                /*
                * 只有 execvp 失敗才會到這裡
                * 例如打了一個不存在的指令
                */
                perror(cmd.args[0]);   // 印出錯誤訊息，例如 "ls123: No such file or directory"
                exit(1);

            } else {
                /*
                * 父行程：等待子行程結束
                * 不等待的話子行程會變成 zombie
                */
                int status;
                waitpid(pid, &status, 0);

                // 顯示子行程的結束狀態
                if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
                    printf("[結束碼: %d]\n", WEXITSTATUS(status));
            }
        }
    }

    printf("掰掰！\n");
    return 0;
}