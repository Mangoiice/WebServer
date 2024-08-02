#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"

template <typename T>
class threadpool
{
public:
    threadpool(int actor_model, connection_pool *connPool, int thread_number = 8, int max_requests =10000);
    ~threadpool();
    bool append(T* request, int state); // 向请求队列中添加请求
    bool append_p(T* request);

private:
    // 工作线程运行的函数，不断的从工作队列中取出任务并执行
    static void* worker(void* args);
    void run();

    int m_thread_number;        // 线程池中的线程数
    int m_max_requests;         // 请求队列中允许的最大请求数
    pthread_t* m_threads;       // 描述线程池的数组
    std::list<T*> m_workqueue;  // 工作任务队列
    locker m_queuelocker;
    sem m_queuestat;            // 标记是否有任务需要处理
    connection_pool *m_connPool;
    int m_actor_model; 
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
        
        if(m_actor_model == 1)
        {
            if(request->m_state == 0)
            {
                if(request->read_once())
                {
                    request->improv = 1;
                    connectionRAII mysqlcon(&request->mysql, m_connPool);
                    request->process();
                }
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
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