#include <mysql/mysql.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <list>
#include <pthread.h>
#include <iostream>
#include "sql_connection_pool.h"

using namespace std;

connection_pool::connection_pool()
{
    m_FreeConn = 0;
    m_CurConn = 0;
}

connection_pool::~connection_pool()
{
    DestoryPool();
}

void connection_pool::init(string url, string User, string PassWord, string DBName, int Port, int MaxConn, int close_log)
{
    // 这里不对m_MaxConn赋值，防止有的连接创建失败
    m_url = url;
    m_User = User;
    m_PassWord = PassWord;
    m_DatabaseName = DBName;
    m_Port = Port;
    m_close_log = close_log;

    for(int i = 0; i < MaxConn; i++)
    {
        // 创建一个mysql连接对象指针并初始化
        MYSQL* con = NULL;
        con = mysql_init(con);
        if(con == nullptr)
        {
            LOG_ERROR("MySQL Error : mysql_init");
            exit(1);
        }

        // 使用mysql对象连接到数据库中
        con = mysql_real_connect(con, url.c_str(), User.c_str(), PassWord.c_str(), DBName.c_str(), Port, NULL, 0);
        if(con == nullptr)
        {
            LOG_ERROR("Mysql Error : mysql_real_connect");
            exit(1);
        }

        // 将创建好的连接加入到连接池中
        connlist.push_back(con);
        m_FreeConn++;
    }
    // 信号量用于标记可用的连接数
    reserve = sem(m_FreeConn);

    m_MaxConn = m_FreeConn;
}

MYSQL* connection_pool::Getconnection()
{
    MYSQL* con = NULL;
    if(connlist.size() == 0)
    {
        return NULL;
    }

    // 将reserve减一，代表一个连接将要被使用
    reserve.wait();
    m_mutex.lock();
    con = connlist.front();
    connlist.pop_front();

    m_FreeConn--;
    m_CurConn++;
    m_mutex.unlock();

    return con;
}

bool connection_pool::ReleaseConnection(MYSQL* con)
{
    if(con == nullptr)
    {
        return false;
    }
    m_mutex.lock();
    connlist.push_back(con);

    m_FreeConn++;
    m_CurConn--;
    m_mutex.unlock();

    // 信号量加1，代表连接已经使用完毕
    reserve.post();

    return true;
}

int connection_pool::GetFreeConn()
{
    return this -> m_FreeConn;
}

void connection_pool::DestoryPool()
{
    m_mutex.lock();
    if(connlist.size() > 0)
    {
        list<MYSQL*>::iterator it;
        for(it = connlist.begin(); it != connlist.end(); ++it)
        {
            MYSQL* con = *it;
            mysql_close(con);
        }
        m_FreeConn = 0;
        m_CurConn = 0;
        connlist.clear();
    }
    m_mutex.unlock();
}

connectionRAII::connectionRAII(MYSQL **SQL, connection_pool *connPool) {
    *SQL = connPool->Getconnection();
 
    conRAII = *SQL;
    poolRALL = connPool;
}
 
connectionRAII::~connectionRAII() {
    poolRALL ->ReleaseConnection(conRAII);
}

