#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <iostream>
#include <string>
#include <stdarg.h>
#include <pthread.h>
#include "block_queue.h"

using namespace std;

// 日志类的声明
class Log
{
public:
    // 单例模式：通过共有静态方法创建类的实例，并且返回指针
    static Log* get_instance()
    {
        static Log instance;
        return &instance;
    }

    // 向日志中写入信息
    static void* flush_log_thread(void* args)
    {
        Log::get_instance() -> async_write_log();
    }

    /*
    初始化日志
    file_name：日志文件存放的路径以及本身的文件名，如/root/ServerLog.txt
    close_log：标记是否关闭日志
    log_buf_size：日志缓冲区的大小
    split_lines：日志的最大行数
    max_queue_size：存放日志任务的循环队列的大小，可以用来区分同步/异步写日志
    */
    bool init(const char* file_name, int close_log, int log_buf_size = 8192,
                int split_lines = 5000000, int max_queue_size = 0);

    /*
    分层级生成日志内容，有四个层级debug、info、warn、error
    可变参数列表传入要写的内容
    如果选择异步写日志，将生成的内容放入循环队列
    */
    void write_log(int level, const char* format, ...);

    // 刷新缓冲区
    void flush();
    
private:
    // 单例模式，私有化构造函数，防止人为调用构造函数创建日志实例
    Log();
    virtual ~Log();

    // 从循环队列中取出任务，写入日志内容
    void* async_write_log()
    {
        string single_line;
        // 循环从队列中取得任务，写入日志文件
        while(m_log_queue -> pop(single_line))
        {
            m_mutex.lock();
            fputs(single_line.c_str(), m_fp);
            m_mutex.unlock();
        }
    }

    FILE* m_fp;         // 指向log文件的指针
    long long m_count;  // 记录日志的行数
    bool m_is_async;    // 是否为异步写日志

    block_queue<string>* m_log_queue;   // 日志的循环队列
    int m_close_log;    // 标记日志是否关闭
    int m_log_buf_size; // 日志缓冲区大小
    char* m_buf;        // 缓冲区
    int m_split_lines;  // 日志最大行数
    int m_today;        // 日志按天记录，m_today存放当天的日期

    char log_name[128]; // 日志文件名
    char dir_name[128]; // 日志存放的目录路径

    locker m_mutex;

};

#define LOG_DEBUG(format, ...) if(0 == m_close_log) {Log::get_instance() -> write_log(0, format, ##__VA_ARGS__); Log::get_instance() -> flush();}
#define LOG_INFO(format, ...) if(0 == m_close_log) {Log::get_instance() -> write_log(1, format, ##__VA_ARGS__); Log::get_instance() -> flush();}
#define LOG_WRAN(format, ...) if(0 == m_close_log) {Log::get_instance() -> write_log(2, format, ##__VA_ARGS__); Log::get_instance() -> flush();}
#define LOG_ERROR(format, ...) if(0 == m_close_log) {Log::get_instance() -> write_log(3, format, ##__VA_ARGS__); Log::get_instance() -> flush();}

#endif