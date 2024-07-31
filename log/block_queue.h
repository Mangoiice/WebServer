#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H

#include<iostream>
#include<stdlib.h>
#include<pthread.h>
#include<sys/time.h>
#include"../lock/locker.h"

using namespace std;

/*
一个用于日志的循环工作队列类，为了通用性，写成了模版类
由工作线程放入，写线程取出，是异步日志
异步日志：将工作线程所写的日志内容先存入阻塞队列，写线程从阻塞队列中取出内容，写入日志
*/ 

template <class T>
class block_queue
{
public:
    // 初始化循环队列，设置私有成员的值
    block_queue(int max_size)
    {
        if(max_size <= 0)
        {
            exit(-1);
        }
        // 设置成员变量的值
        m_max_size = max_size;
        m_array = new T[max_size];
        m_size = 0;
        m_front = -1;
        m_back = -1;
    }

    // 初始化new了内存，需要在析构函数中删除
    ~block_queue()
    {
        m_mutex.lock();
        // 如果队列已经初始化，需要删除new出来的内存
        if(m_array != NULL)
        {
            // 按理说还需要将m_array指向nullptr，防止人为调用两次析构函数
            delete []m_array;
        }
        m_mutex.unlock();
    }

    // 清空队列
    bool clear()
    {
        m_mutex.lock();
        m_size = 0;
        // m_front初始化为-1，指向的是真正的头的前一个位置
        m_front = -1;
        m_back = -1;
        m_mutex.unlock();
    }

    // 判断队列是否已满
    bool full()
    {
        m_mutex.lock();
        if(m_size >= m_max_size)
        {
            m_mutex.unlock();
            return true;
        }
        m_mutex.unlock();
        return false;
    }

    // 判断队列是否为空
    bool empty()
    {
        m_mutex.lock();
        if(m_size == 0)
        {
            m_mutex.unlock();
            return ture;
        }
        m_mutex.unlock();
        return false;
    }

    // 获得队首元素
    bool front(T &val)
    {
        m_mutex.lock();
        // 不能调用empty函数，会导致加两次锁发生死锁
        if(m_size == 0)
        {
            m_mutex.unlock();
            return false;
        }
        val = m_array[(m_front+1) % m_max_size];
        m_mutex.unlock();
        return true;
    }

     // 获得队尾元素
    bool back(T &val)
    {
        m_mutex.lock();
        // 不能调用empty函数，会导致加两次锁发生死锁
        if(m_size == 0)
        {
            m_mutex.unlock();
            return false;
        }
        val = m_array[m_back];
        m_mutex.unlock();
        return true;
    }

    // 获得队列大小
    int size()
    {
        int tmp = 0;
        m_mutex.lock();
        tmp = m_size;
        m_mutex.unlock();
        return tmp;
    }

    // 获得队列容量
    int max_size()
    {
        int tmp = 0;
        m_mutex.lock();
        tmp = m_max_size;
        m_mutex.unlock();
        return tmp;
    }

    /*
    向队列中添加任务，是工作线程调用的
    在添加任务之前，需要唤醒所有使用队列的线程
    */
    bool push(T& item)
    {
        m_mutex.lock()
        // 队列已满，无法添加，需要先等待写线程消费任务
        if(m_size >= m_max_size)
        {
            m_cond.broadcast();
            m_mutex.unlock();
            return false;
        }

        m_back = (m_back + 1) % m_max_size;
        m_array[m_back] = item;
        m_size++;

        m_cond.broadcast()
        m_mutex.unlock();
        return true;
    }

    /*
    从队列中取出任务，是写线程调用的
    */
    bool pop(T& item)
    {
        m_mutex.lock();
        // 队列中还没有任务，等待工作线程放入任务
        while(m_size <= 0)
        {
            if(!m_cond.wait(m_mutex.get()))
            {
                m_mutex.unlock();
                return false;
            }
        }

        // 因为m_front指向的是前一个位置，所以需要先更新才能取出
        m_front = (m_front + 1) % m_max_size;
        item = m_array[m_front];
        m_size--;
        m_mutex.unlock()
        return true;
    }

private:
    locker m_mutex;    //队列是工作线程和写线程的共享资源，需要锁和条件变量
    cond m_cond;

    T* m_array;     // 循环队列，存储的变量类型是模板类型  
    int m_max_size; // 队列的最大容量
    int m_size;     // 队列现有任务
    int m_front;    // 队列头
    int m_back;     // 队列尾
};

#endif