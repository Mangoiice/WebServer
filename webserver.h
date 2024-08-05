#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>

#include "./threadpool/threadpool.h"
#include "./http/http_conn.h"

const int MAX_FD = 65536;           // 最大文件描述符数量
const int MAX_EVENT_NUMBER = 10000; // 最大事件数量
const int TIMESLOT = 5;             // 最小超时单位

class WebServer
{
public:
    WebServer();
    ~WebServer();

public:
    void init(int port, string user, string passWord, string databaseName,
            int log_write, int opt_linger, int trigmode, int sql_num,
            int thread_num, int close_log, int actor_model);
    // 设置监听socket和客户端socket的触发方式，有2*2=4种组合。默认LT+LT
    void trig_mode();
    // 初始化日志
    void log_write();
    // 初始化数据库连接池
    void sql_pool();
    // 初始化线程池
    void thread_pool();
    // 设置监听socket，创建epoll
    void eventListen();
    // 服务器的主循环
    void eventLoop();

private:
    // 初始化客户端连接的http_conn对象，并且设置他的定时器
    void timer(int connfd, struct sockaddr_in client_address);
    // 处理完connfd的事件后，重新设置timer
    void adjust_timer(util_timer* timer);
    // 客户端断开连接后，删除timer
    void del_timer(util_timer* timer, int sockfd);

    // 有新的客户端连接时，accept连接加入到epoll内核事件注册表中
    bool dealclientdata();
    // 发生事件的是管道时调用，处理SIGALRM信号
    bool dealwithsignal(bool& timerout, bool& stop_server);
    // 客户端socket发生读事件，即客户端发来了请求报文时调用
    void dealwithread(int sockfd);
    // 客户端socket发生写事件
    void dealwithwrite(int sockfd);

public:
    // 服务器本身的参数
    int m_port;         // 服务器端口
    char* m_root;       // 服务器网页文件的根目录
    int m_log_write;    // 标记日志是同步写还是异步写，1为异步
    int m_close_log;    // 标记是否开启日志，默认开启
    int m_actormodel;   // Reactor(1) 或 Proactor(0)，默认为Proactor

    int m_epollfd;      // epollfd
    int m_pipefd[2];    // 管道
    http_conn* users;   // 客户端连接的数组

    // 数据库
    connection_pool* m_connPool;
    string m_user;
    string m_passWord;
    string m_databaseName;
    int m_sql_num;  // 连接池的最大连接数

    // 线程池
    threadpool<http_conn>* m_pool;
    int m_thread_num;

    // epoll多路复用模型
    epoll_event events[MAX_EVENT_NUMBER];

    int m_TRIGMode;
    int m_OPT_LINGER;
    int m_listenfd;
    int m_LISTENTrigmode;
    int m_CONNTrigmode;

    // 定时器
    client_data* users_timer;
    Utils utils;

};

#endif