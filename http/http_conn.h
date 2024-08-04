#ifndef HTTP_CONN_H
#define HTTP_CONN_H
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
#include <map>

#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"
#include "../timer/lst_timer.h"
#include "../log/log.h"

class http_conn
{
public:
    static const int FILENAME_LEN = 200;    // 读取文件的名称m_real_file的长度
    static const int READ_BUFFER_SIZE = 2048;   // 设置读缓冲区m_read_buf的大小
    static const int WRITE_BUFFER_SIZE = 1024;  // 设置写缓冲区m_write_buf的大小

    // 报文的请求方式
    enum METHOD{GET = 0, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT, PATH};
    // 主状态机的状态，对应请求行，请求头，正文
    enum CHECK_STATE{CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER, CHECK_STATE_CONTENT};
    // 报文解析结果
    enum HTTP_CODE{NO_REQUEST = 0, GET_REQUEST, BAD_REQUEST, NO_RESOURCE, FORBIDDEN_REQUEST, FILE_REQUEST, INTERNAL_ERROR, CLOSED_CONNECTIONS};
    // 从状态机的状态
    enum LINE_STATE{LINE_OK = 0, LINE_BAD, LINE_OPEN};

public:
    http_conn() {};
    ~http_conn() {};

public:
    // 初始化套接字地址，函数内部调用私有方法init
    void init(int sockfd, const sockaddr_in& addr, char*, int, int, string user, string passwd, string sqlname);
    // 关闭http连接
    void close_conn(bool real_close = true);
    void process();
    // 读取浏览器发来的全部数据，请求报文之类
    bool read_once();
    // 编写相应相应报文
    bool write();

    sockaddr_in* get_address() {return &m_address;}

    // 线程连接到数据库，读取存储着用户数据的表
    void initmysql_result(connection_pool* connPool);
    int timer_flag;
    int improv;

private:
    void init();
    // 从m_read_buf读取并处理请求报文
    HTTP_CODE process_read();
    // 向m_write_buf写入回应报文
    bool process_write(HTTP_CODE ret);
    // 主状态机解析请求报文中的请求行
    HTTP_CODE parse_request_line(char* text);
    // 主状态机解析请求报文中的请求头、空行
    HTTP_CODE parse_headers(char* text);
    // 主状态机解析请求报文中的正文
    HTTP_CODE parse_content(char* text);
    // 生成回应报文
    HTTP_CODE do_request();

    /*
    m_start_line是已经解析的字节数
    get_line用于将指针向后偏移，指向未处理的字符
    */
    char* get_line() {return m_read_buf + m_start_line;}

    // 从状态机读取一行，分析是请求报文的哪一部分
    LINE_STATE parse_line();

    void unmap();

    // 根据回应报文的格式，生成相应的部分，以下函数皆由do_request函数调用
    bool add_response(const char* format, ...);
    bool add_content(const char* content);
    bool add_status_line(int status, const char* title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

public:
    static int m_epollfd;
    static int m_user_count;
    MYSQL* mysql;
    int m_state;    // 读事件为1，写事件为0

private:
    int m_sockfd;
    sockaddr_in m_address;
    locker m_lock;

    char m_read_buf[READ_BUFFER_SIZE];  // 存储读取的请求报文
    long m_read_idx;                    // m_read_buf中数据最后一个字节的下一字节
    long m_checked_idx;                 // 从状态机读取到m_read_buf中的位置
    int m_start_line;                   // m_read_buf中已经读取的字符个数

    char m_write_buf[WRITE_BUFFER_SIZE];    // 存储发送的响应报文数据
    int m_write_idx;                        // m_write_buf中写入的字节数

    CHECK_STATE m_check_state;  // 主状态机的状态
    METHOD m_method;            // 请求方法：GET、POST等

    // 请求报文中的6个数据，分别由6个变量存储
    char m_real_file[FILENAME_LEN];
    char* m_url;
    char* m_version;
    char* m_host;
    long m_content_length;
    bool m_linger;

    char* m_file_address;       // 将服务器上的文件映射到共享内存中，该变量存储共享内存地址
    struct stat m_file_stat;    // 储存文件信息的结构体
    struct iovec m_iv[2];       // io向量机制iovec
    int m_iv_count;
    int cgi;                    // 是否启用POST
    char* m_string;             // 存储请求头数据
    int bytes_to_send;          // 剩余发送的字节数
    int bytes_have_send;        //已经发送的字节数
    char* doc_root;             // 网站根目录

    map<string, string> m_user;
    int m_TRIGMode; // 为1是epoll为ET触发
    int m_close_log;

    char sql_user[100];
    char sql_passwd[100];
    char sql_name[100];
};
#endif