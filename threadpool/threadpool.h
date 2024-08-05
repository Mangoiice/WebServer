#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"

/*
线程池类
事实上，自调用线程池的init方法创建出线程后，多数线程会一直休眠在取出请求队列中的请求这一步骤
只有当调用线程池的append和append_p函数后，才会唤醒线程
运用泛型编程的思想，请求可以是任意的数据类型，该项目的请求是http_conn连接

手撕线程池：
1、一些必要的变量，比如线程数、请求队列最大值、请求队列、线程数组、请求队列锁、请求队列信号量
2、构造函数，用来创建线程和初始化变量
3、析构函数，delete线程数组
4、向请求队列中添加请求的函数
5、工作线程运行的函数worker、run
*/
template <typename T>
class threadpool
{
public:
    threadpool(int actor_model, connection_pool *connPool, int thread_number = 8, int max_requests =10000);
    ~threadpool();
    // Reactor模型向请求队列添加一个事件请求，由工作线程进行IO，state标记是写事件还是读事件
    bool append(T* request, int state);
    // Proactor模型向请求队列添加一个事件请求，主线程已经完成IO，工作线程只需进行逻辑处理
    bool append_p(T* request);

private:
    /*
    工作线程运行的函数
    worker函数将void *转化为threadpool *，调用run函数
    需要声明成静态成员函数，因为非静态成员函数会隐含this指针，在调用pthread_create函数时，
    与传参无法匹配，为了解决静态成员函数无法访问类内非静态成员变量和函数的问题，将this指针
    作为参数传入pthread_create中
    */
    static void* worker(void* args);

    /*
    线程不断的从请求队列中尝试取出请求，若没有请求会阻塞等待
    取得请求(http_conn对象)后，通过http_conn的类方法实现以下过程：
    1、Reactor模型下，进行读取请求报文
    2、解析请求报文，分析客户端需要跳转到什么界面
    3、编写响应报文
    4、Reactor模型下，将相应报文发送给客户端。如果客户端还需要访问新的网页，还需要将网页存储在
    服务端的文件一并返回给客户端
    */
    void run();

    int m_thread_number;        // 线程池中的线程数
    int m_max_requests;         // 请求队列中允许的最大请求数
    pthread_t* m_threads;       // 描述线程池的数组
    std::list<T*> m_workqueue;  // 请求队列
    locker m_queuelocker;       // 请求队列被多个工作线程共享，需要加锁
    sem m_queuestat;            // 标记是否有请求需要处理
    connection_pool *m_connPool;
    int m_actor_model;          // Reactor或者Proactor，0代表Proactor
};

template <typename T>
threadpool<T>::threadpool(int actor_model, connection_pool* connPool, int thread_number, int max_requests)
{
    m_actor_model = actor_model;
    m_thread_number = thread_number;
    m_max_requests = max_requests;
    m_threads = NULL;
    m_connPool = connPool;

    if(thread_number <= 0 || max_requests <= 0) throw std::exception();
    
    m_threads = new pthread_t[thread_number];
    if(!m_threads) throw std::exception();

    for(int i = 0; i < thread_number; i++)
    {
        /*
        pthread_create成功调用后，第一个参数指向的内存单元存放新线程的id
        第二个参数设置线程的属性
        第三个参数传入线程执行的函数
        第四个参数传入线程执行函数所需的参数
        */
        if(pthread_create(m_threads+i, NULL, worker, this) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }
        // 调用pthread_detach函数将线程分离出来，不需要手动join
        if(pthread_detach(m_threads[i]) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

template <typename T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
}

template <typename T>
bool threadpool<T>::append(T* request, int state)
{
    m_queuelocker.lock();
    if(m_workqueue.size() >= m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }

    request->m_state = state;
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

template <typename T>
bool threadpool<T>::append_p(T* request)
{
    m_queuelocker.lock();
    if(m_workqueue.size() >= m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }

    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

template <typename T>
void* threadpool<T>::worker(void* args)
{
    threadpool* pool =(threadpool*) args;
    pool->run();
    return pool;
}

template <typename T>
void threadpool<T>::run()
{
    while(true)
    {
        m_queuestat.wait();
        m_queuelocker.lock();
        if(m_workqueue.empty())
        {
            m_queuelocker.unlock();
            continue;
        }
        T* request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();

        if(!request) continue;
        // Reactor模型，工作线程负责IO读写
        if(m_actor_model == 1)
        {
            // 读事件
            if(request->m_state == 0)
            {
                if(request->read_once())
                {
                    request->improv = 1;    // 标志IO读写已经处理完毕
                    connectionRAII mysqlcon(&request->mysql, m_connPool);   // 连接上数据库
                    request->process();
                }
                else
                {
                    request->improv = 1;        // 处理IO处理过了
                    request->timer_flag = 1;    // 通知主线程处理出错了
                }
            }
            else
            {
                if(request->write())
                {
                    request->improv = 1;
                }
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
        }
        else
        {
            connectionRAII mysqlcon(&request->mysql, m_connPool);
            request->process();
        }
    }
}
#endif