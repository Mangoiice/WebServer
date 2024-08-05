#ifndef LST_TIMER_H
#define LST_TIMER_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>

#include <time.h>
#include "../log/log.h"

class util_timer;

// 封装连接资源，包括socket地址 socket 定时器
struct client_data
{
    sockaddr_in address;
    int sockfd;
    util_timer *timer;
};

// 定时器类
class util_timer
{
public:
    // 构造函数初始化前后节点为空
    util_timer():prev(nullptr), next(nullptr){}

    time_t expire;  // 该定时器的到期时间，采用绝对时间
    void (*cb_func)(client_data* user_data);    // 函数指针
    client_data* user_data; // 连接资源

    // 定时器会被放在一个上升链表中，所以需要定义某一节点的前后节点指针
    util_timer* prev;   
    util_timer* next;   
};

// 定时器上升链表类
class sort_timer_lst
{
public:
    sort_timer_lst();
    ~sort_timer_lst();
    // 添加一个定时器，存在新的定时器需要放在头结点的情况，排除情况后调用私有add_timer函数
    void add_timer(util_timer* timer);  
    // 调整定时器，当一个连接发起请求并处理后，重新调整连接在链表中的的位置
    void adjust_timer(util_timer* timer);
    void del_timer(util_timer* timer);  //删除定时器
    /*
    心搏函数tick
    主循环定期调用tick，检查是否有到期的定时器并删除
    */
    void tick();

private:
    // 添加一个定时器到以lst_head为头节点的链表中
    void add_timer(util_timer* timer, util_timer* lst_head);

    util_timer* head;
    util_timer* tail;
};

// 使用上升链表的工具类
class Utils
{
public:

    // 初始化，timeslot为定时检查时间，检查时发现expire > 现在的时间即为超时
    void init(int timeslot);

    // 对文件描述符设置非阻塞
    int setnonblocking(int fd);
    // 向内核时间表注册读事件，ET模式，选择开启EPOLLONESHOT
    void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);
    // 信号处理函数：接收到信号后调用，把信号sig经由管道通知给epoll
    static void sig_handler(int sig);
    // 设置信号处理函数，第二个参数为传函数，接收到信号后会执行handler函数
    void addsig(int sig, void(handler)(int), bool restart = true);
    // 调用一次tick函数，并且在m_TIMESLOT时间后触发SIGALRM信号
    void timer_handler();

    // 向客户端发送错误信息，并且关闭connfd
    void show_error(int connfd, const char* info);

    static int* u_pipefd;
    sort_timer_lst m_timer_list;
    static int u_epollfd;
    int m_TIMESLOT;
};

// 回调函数
void cb_func(client_data* user_data);
#endif