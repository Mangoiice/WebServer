#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include <pthread.h>
#include "log.h"

using namespace std;

// 日志类构造函数，初始化日志行数为0，关闭异步写
Log::Log()
{
    m_count = 0;
    m_is_async = false;
}

// 日志类析构函数，关闭打开的日志文件句柄
Log::~Log()
{
    if(m_fp != NULL)
    {
        fclose(m_fp);
    }
}

bool Log::init(const char* file_name, int close_log, int log_buf_size, int split_lines, int max_queue_size)
{
    // 进行日志的初始化
    if(max_queue_size >= 1)
    {
        m_is_async = true;
        m_log_queue = new block_queue<string>(max_queue_size);
        
        // 创建一个写线程来写日志，回调函数是flush_log_thread
        pthread_t tid;
        pthread_create(&tid, NULL, flush_log_thread, NULL); 
    }

    // 根据函数的输入参数来初始化日志类的各个值
    m_close_log = close_log;
    m_split_lines = split_lines;
    m_log_buf_size = log_buf_size;
    m_buf = new char[m_log_buf_size];
    memset(m_buf, '\0', m_log_buf_size);

    // 创建struct tm变量来获取时间
    time_t t =time(NULL);
    struct tm* sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    /*
    strchr函数在file_name中查找最后一个'/'的位置
    未找到返回nullptr，找到返回指向'/'的指针
    file_name的格式类似于 /log/log.txt
    所以找到最后一个'/'是为了找到文件名，方便修改为我们想要的带有时间的格式
    */
    const char* p = strchr(file_name, '/');
    // 创建一个变量来存储完整的文件日志的名字，包括路径和文件名
    char log_full_name[256] = {0};

    // 写入完整的日志文件名
    if(p == nullptr)
    {
        /*
        说明文件直接存放在根目录下，file_name就是文件名
        snprintf函数将可变参数(...)，按照 format 格式化成字符串，
        并将字符串复制到 str 中，size 是要写入的字符的最大数目，超过 size 会被截断
        会复制size-1个字符，因为会自动添加一个\0
        返回值为欲写入的字符串长度，而非真正的长度
        */
        snprintf(log_full_name, 255, "%d_%02d_%02d_%s", my_tm.tm_year+1900, my_tm.tm_mon+1, my_tm.tm_mday, file_name);
        // 以下这行代码是我个人添加的一行代码
        // strcpy(log_name, file_name);
    }
    else
    {
        // file_name包含路径和文件名，需要将其分开才能为日志文件重命名
        strcpy(log_name, p+1);  // 存储文件名
        strncpy(dir_name, file_name, p-file_name+1); // 存储目录名
        snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s", dir_name, my_tm.tm_year+1900, my_tm.tm_mon+1, my_tm.tm_mday, log_name);
    }

    m_today = my_tm.tm_mday;

    // 以上日志初始化就完成了，开始创建日志文件
    m_fp = fopen(log_full_name, "a");
    if(m_fp == nullptr)
    {
        return false;
    }

    return true;
}

void Log::write_log(int level, const char* format, ...)
{
    // 也是在进行获取时间的操作
    struct timeval now = {0, 0};
    gettimeofday(&now, NULL);
    time_t t = now.tv_sec;
    struct tm* sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    // 要区分写的日志是哪一个层级的日志，根据输入的level来判断
    char s[16] = {0};
    switch(level)
    {
    case 0:
        strcpy(s, "[debug]");
        break;
    case 1:
        strcpy(s, "[info]");
        break;
    case 2:
        strcpy(s, "[warn]");
        break;
    case 3:
        strcpy(s, "[error]");
        break;
    default:
        strcpy(s, "[info]");
        break;
    }

    // 开始写入日志
    m_mutex.lock();
    m_count++;
    // 如果是新的一天，或者日志写满了，要创建新的日志
    if(m_today != my_tm.tm_mday || m_count % m_split_lines == 0)
    {
        // 存储新的日志文件名
        char new_log[256] = {0};
        // 刷新缓冲区，保证数据被写入
        fflush(m_fp);
        fclose(m_fp);
        // tail存储了新的时间信息，还需要在前面加上目录路径，在后面加上文件名
        char tail[16] = {0};

        snprintf(tail, 16,"%d_%02d_%02d_", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);

        // 是新的一天
        if(m_today != my_tm.tm_mday)
        {
            snprintf(new_log, 255, "%s%s%s", dir_name, tail, log_name);
            m_today = my_tm.tm_mday;
            m_count = 0;
        }
        // 日志写满了
        else
        {
            snprintf(new_log, 255, "%s%s%s.%lld", dir_name, tail, log_name, m_count / m_split_lines);
        }
        m_fp = fopen(new_log, "a");
    }
    m_mutex.unlock();

    // 可变参数定义初始化，在vsprintf时使用，作用：输入具体的日志内容
    va_list valst;
    va_start(valst, format);

    string log_str; // 存储一行日志内容
    m_mutex.lock();

    // 编写每一行日志的开头格式内容：2024-08-01 01:48:30.000000 [info]XXXXXXXXXX
    int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s", 
                    my_tm.tm_year+1900, my_tm.tm_mon+1, my_tm.tm_mday,
                    my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);
    int m = vsnprintf(m_buf+n, m_log_buf_size-n-1, format, valst);
    // 加入换行和\0表示一行结束
    m_buf[n+m] = '\n';
    m_buf[n+m+1] = '\0';    // 尤其重要
    log_str = m_buf;
    m_mutex.unlock();

    // 是异步写还是同步写
    if(m_is_async && !m_log_queue->full())
    {
        m_log_queue -> push(log_str);
    }
    else
    {
        m_mutex.lock();
        fputs(log_str.c_str(), m_fp);
        m_mutex.unlock();
    }

    // 关闭可变参数索引
    va_end(valst);
}

void Log::flush(void)
{
    m_mutex.lock();
    //强制刷新写入流缓冲区
    fflush(m_fp);
    m_mutex.unlock();
}