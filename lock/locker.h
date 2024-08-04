/*
声明了sem、locker、cond三个类，用来保证多线程通信中的线程同步
采用RAII的思想，确保互斥锁和信号量被销毁
*/

#ifndef LOCKER_H
#define LOCKER_H
 
#include<exception>
#include<pthread.h>
#include<semaphore.h>

using namespace std;

class sem
{
public:
    // 初始化值为0的多线程信号量
    sem()
    {
        if(sem_init(&m_sem, 0, 0) != 0)
        {
            throw exception();
        }
    }

    // 初始化值为val的多线程信号量
    sem(int val)
    {
        if(sem_init(&m_sem, 0, val) != 0)
        {
            throw exception();
        }
    }

    // 析构函数，销毁信号量，RAII思想
    ~sem()
    {
        sem_destroy(&m_sem);
    }

    // 使信号量-1，代表工作线程开始工作，如果信号量为0则阻塞等待
    bool wait()
    {
        return sem_wait(&m_sem) == 0;
    }

    // 使信号量+1，代表工作线程完成工作
    bool post()
    {
        return sem_post(&m_sem) == 0;
    }

private:
    sem_t m_sem;  // 信号量，当信号量为0时会阻塞等待
};

class locker
{
public:
    locker()
    {
        if(pthread_mutex_init(&m_mutex, NULL) != 0)
        {
            throw exception();
        }
    }

    ~locker()
    {
        pthread_mutex_destroy(&m_mutex);
    }

    // 上锁
    bool lock()
    {
        return pthread_mutex_lock(&m_mutex) == 0;
    }

    // 解锁
    bool unlock()
    {
        return pthread_mutex_unlock(&m_mutex) == 0;
    }

    // 获取指向锁的指针
    pthread_mutex_t* get()
    {
        return &m_mutex;
    }

private:
    pthread_mutex_t m_mutex;
};

class cond
{
public:
    cond()
    {
        if(pthread_cond_init(&m_cond, NULL) != 0)
        {
            throw exception();
        }
    }

    ~cond()
    {
        pthread_cond_destroy(&m_cond);
    }

    /*
    调用pthread_cond_wait函数，一直阻塞等待，直到其他线程调用signal或broadcast函数唤醒
    会先解锁mutex，收到通知后先上锁获取资源，再将线程加入工作队列
    之所以先上锁再加入工作队列，是防止先加入工作队列后，唤醒条件被其他线程改变
    */
    bool wait(pthread_mutex_t* m_mutex)
    {
        return pthread_cond_wait(&m_cond, m_mutex) == 0;
    }

    // 阻塞一段时间的wait
    bool timewait(pthread_mutex_t* m_mutex, struct timespec* m_abstime)
    {
        return pthread_cond_timedwait(&m_cond, m_mutex, m_abstime) == 0;
    }

    //唤醒至少一个工作线程
    bool signal()
    {
        return pthread_cond_signal(&m_cond) == 0;
    }

    // 唤醒全部工作线程
    bool broadcast()
    {
        return pthread_cond_broadcast(&m_cond) == 0;
    }
    
private:
    pthread_cond_t m_cond;
};

#endif