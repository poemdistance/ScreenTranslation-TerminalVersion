/* 本程序功能:
 *
 * 通过操作鼠标设备文件，监听当前鼠标左键双击事件
 * 以及长按左键进行区域选择的事件。
 *
 * 通过键盘设备文件event3进行模拟键盘操作
 *
 * 当检测到双击或者区域选择操作，在操作结束后，
 * 模拟键盘发送CTRL-C
 *
 * 调用getClipboard()函数获取剪贴板内容
 * */
#include <string.h>  
#include <math.h>  
#include <stdlib.h>  
#include <sys/stat.h>  
#include <stdio.h>  
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>  
#include <sys/stat.h>  
#include <fcntl.h>  
#include <linux/input.h>  
#include <time.h>
#include <linux/uinput.h>  
#include <stdio.h>  
#include <sys/time.h>  
#include <sys/types.h>  
#include <unistd.h>  
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#include "common.h"

const static char termName[3][19] = 
{
    "terminator",
    "gnome-terminal-",
    "konsole"
};

#define TEXTSIZE (1024*1024)
#define DOUBLECLICK (1)
#define SLIDE (2)

char *text = NULL;
FILE *fp = NULL;
int mousefd;
int fd_key = -1;

void delay() {

    /*等待数据被写入剪贴板,若不延时,获取的剪贴板内容还是上次的*/
    for(int i = 0; i < 1024; i++)
        for ( int j = 0; j < 6024; j++ );

}

void writePipe(char *text, int fd) {

    int writelen;

    /*排除空字符和纯回车字符*/
    if ( strcmp( text, " ") != 0 && strcmp( text, "\n") != 0 ) {

        writelen = strlen(text);
        for(int i=0; i<writelen; i++) {
            if ( text[i] == '\n')
                text[i] = ' ';
        }
        if ( text[writelen-1] != '\n')  {
            text = strcat(text, "\n");
            writelen++;
        }

        int ret = write( fd, text, writelen );
        if ( ret != writelen )
            fprintf(stderr, "write error\n");
    }
}

void handler(int signo) {

    pid_t pid;
    while( (pid=waitpid(-1, NULL, WNOHANG)) > 0);
}

int isTerminal(char *name) {

    int n = sizeof(termName) / sizeof(termName[0]);
    //fprintf(stdout, "name=%s\n", name);
    char *p = name;
    while(*p++ != '\n');
    *(p-1) = '\0';

    for ( int i = 0; i < n; i++ ) {
        if ( strcmp ( termName[i], name ) == 0 )
            return 1;
    }
    return -1;
}

/*获取当前数组下标的前一个下标值,
 *数组元素为4*/
int previous( int n )
{
    if ( n != 0 )
        return n - 1;
    else
        return  3;
}

int isAction(int history[], int last, int action) {
    int m, n, j, q;
    m = previous(last);
    n = previous(m);
    j = previous(n);
    q = previous(j);

    if(action == DOUBLECLICK &&
            history[m] == 0 && history[n] == 1 &&   
            history[j] == 0 && history[q] == 1 )
        return 1;

    else if(action == SLIDE &&
            history[m] == 0 && history[n] == 1 &&
            history[j] == 1 && history[q] == 1 
           )
        return 1;

    return 0;
}


/*退出前加个回车*/
void quit() {

    fprintf(stdout, "\n");
    if ( text != NULL )
        free(text);
    close(mousefd);
    pclose(fp);
    close(fd_key);
    exit(0);
}

/*同步键盘*/
void sync_key(
        int *fd,
        struct input_event *event,
        int *len)
{
    event->type = EV_SYN;
    event->code = SYN_REPORT;
    event->value = 0;
    write(*fd, event, *len);
}


/*发送按键keyCode*/
void press(int fd, int keyCode)
{
    struct input_event event;

    //发送
    event.type = EV_KEY;
    event.value = 1;
    event.code = keyCode;
    gettimeofday(&event.time,0);
    write(fd,&event,sizeof(event)) ;

    //同步
    int len = (int)sizeof(event);
    sync_key(&fd, &event, &len);
}

/*释放按键*/
void release(int fd, int keyCode)
{
    struct input_event event;

    //释放
    event.type = EV_KEY;
    event.code = keyCode;
    event.value = 0;
    gettimeofday(&event.time, NULL);
    write(fd, &event, sizeof(event));

    //同步
    int len = (int)sizeof(event);
    sync_key(&fd, &event, &len);
}

void simulateKey(int fd,  int key[], int len) {

    int i = 0;
    for(i=0; i<len; i++) 
        press(fd, key[i]);

    for(i=len-1; i>=0; i--) 
        release(fd, key[i]);
}


int main(int argc, char **argv)
{  
    struct sigaction sa; 
    int retval ;  
    char buf[3];  
    char appName[100];
    int releaseButton = 1;
    time_t current;
    fd_set readfds;  
    struct timeval tv;  
    struct timeval old, now;
    double oldtime = 0;
    double newtime = 0;
    double lasttime = 0;
    int thirdClick;

    int fd[2];
    int status;
    pid_t pid;

    int Ctrl_Shift_C[] = {KEY_LEFTCTRL, KEY_LEFTSHIFT, KEY_C};
    int Ctrl_C[] = {KEY_LEFTCTRL, KEY_C};

    if ( (status = pipe(fd)) != 0 ) {
        fprintf(stderr, "create pipe fail\n");
        exit(1);
    } 

    if ( ( pid = fork() ) == -1 ) {
        fprintf(stderr, "fork fail\n");
        exit(1);
    }

    if ( pid > 0 ) {

        /*父进程:关闭读端口*/
        close(fd[0]);

        sa.sa_handler = handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART;
        if ( sigaction(SIGCHLD, &sa, NULL) == -1) {
            perror("sigaction");
            exit(1);
        }

        // 打开鼠标设备  
        mousefd = open("/dev/input/mice", O_RDONLY );  
        if ( mousefd < 0 ) {  
            fprintf(stderr, "Failed to open mice");
            exit(1);  
        }

        int history[4] = { 0 };
        int i = 0, n = 0, m = 0;

        /*捕捉ctrl-c退出信号*/
        signal(SIGINT, quit);

        while(1) {
            // 设置最长等待时间
            tv.tv_sec = 5;  
            tv.tv_usec = 0;  

            FD_ZERO( &readfds );  
            FD_SET( mousefd, &readfds );  

            retval = select( mousefd+1, &readfds, NULL, NULL, &tv );  
            if(retval==0) {  
                continue;
            }  
            if(FD_ISSET(mousefd,&readfds)) {

                // 读取鼠标设备中的数据  
                if(read(mousefd, buf, 3) <= 0) {  
                    continue;  
                }  

                /*循环写入鼠标数据到数组*/
                history[i++] = buf[0] & 0x07;
                if ( i == 4 )
                    i = 0;

                /*m为最后得到的鼠标键值*/
                m = previous(i);
                n = previous(m);

                /*没有按下按键并活动鼠标,标志releaseButton=1*/
                if ( history[m] == 0 && history[n] == 0 )
                    releaseButton = 1;

                /*按下左键*/
                if ( history[m] == 1 && history[n] == 0 ) {
                    if ( releaseButton ) {
                        time(&current);
                        gettimeofday(&old, NULL);

                        /* lasttime为双击最后一次的按下按键时间;
                         * 如果上次双击时间到现在不超过600ms，则断定为3击事件;
                         * 3击会选中一整段，或一整句，此种情况也应该复制文本*/
                        if (abs(lasttime - ((old.tv_usec + old.tv_sec*1000000) / 1000)) < 600 \
                                && lasttime != 0)
                            thirdClick = 1; /*3击标志*/
                        else { /*不是3击事件则按单击处理，更新oldtime*/
                            oldtime = (old.tv_usec + old.tv_sec*1000000) / 1000;
                            thirdClick = 0;
                        }
                        releaseButton = 0;

                        /*非3击事件，则为单击，更新oldtime后返回检测鼠标新一轮事件*/
                        if ( !thirdClick )
                            continue;
                    }
                }

                /*检测双击时间间隔*/
                if ( isAction(history, i, DOUBLECLICK) )  {
                    releaseButton = 1;
                    gettimeofday( &now, NULL );
                    newtime = (now.tv_usec + now.tv_sec*1000000) / 1000;

                    /*双击超过600ms的丢弃掉*/
                    if ( abs (newtime - oldtime) > 600)  {
                        memset(history, 0, sizeof(history));
                        continue;
                    }
                    /*更新最后一次有效双击事件的发生时间*/
                    lasttime = newtime;
                }

                /*双击,3击或者按住左键滑动区域选择事件处理*/
                if ( isAction(history, i, DOUBLECLICK) 
                        || isAction(history, i, SLIDE) 
                        || (thirdClick == 1)) {

                    if ( thirdClick == 1 ) {
                        thirdClick = 0;

                        /* 通知已释放左键，让检测程序能继续更新oldtime
                         * 否则下次releaseButton只能在双击事件检测里执行
                         * 造成oldtime长时未更新导致每次执行3击后的双击
                         * 都被视为超时*/
                        releaseButton = 1;
                    }

                    if ( fd_key < 0 )
                        fd_key = open("/dev/input/event3", O_RDWR);

                    /*需每次都执行才能判断当前的窗口是什么*/
                    fp = popen("ps -p `xdotool getwindowfocus getwindowpid`\
                            | awk '{print $NF}' | tail -n 1", "r");

                    memset ( appName, 0, sizeof(appName) );
                    if ( fread(appName, sizeof(appName), 1, fp) < 0) {
                        fprintf(stderr, "fread error\n");
                        continue;
                    }

                    if ( isTerminal(appName) == 1)
                        simulateKey(fd_key, Ctrl_Shift_C, 3);
                    else
                        simulateKey(fd_key, Ctrl_C, 2);

                    delay();
                    if ( text == NULL )
                        text = malloc(TEXTSIZE);
                    memset(text, 0, TEXTSIZE);
                    getClipboard(text);
                    writePipe(text, fd[1]);

                    /*清除鼠标记录*/
                    memset(history, 0, sizeof(history));

                }/*双击,3击或者区域选择事件处理*/

            } /*if(FD_ISSET(mousefd,&readfds))*/

        } /*while loop*/  

    } /*if pid > 0*/

    else { /*child process*/

        close(fd[1]); /*关闭写端口*/

        /*重映射标准输入为管道读端口*/
        if ( fd[0] != STDIN_FILENO) {
            if ( dup2( fd[0], STDIN_FILENO ) != STDIN_FILENO) {
                fprintf(stderr, "dup2 error");
                close(fd[0]);
                exit(1);
            }
        }

        char * const cmd[2] = {"tranen", (char*)0};
        if ( execv( "/usr/bin/tranen", cmd ) < 0) {
            fprintf(stderr, "Execv error\n");
            exit(1);
        }
    }
    return 0;  
}
