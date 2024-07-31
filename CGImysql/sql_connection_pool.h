#ifndef SQL_CONNECTION_POOL
#define SQL_CONNECTION_POOL

#include <stdio.h>
#include <list>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include <iostream>
#include <string>
#include "../lock/locker.h"
#include "../log/log.h"
using namespace std;

class connection_pool
{
public:
    static connection_pool* GetInstance()
    {
        static connection_pool connPool;
        return &connPool;
    }

    MYSQL* Getconnection();   // 获取一个数据库的连接
    bool ReleaseConnection(MYSQL* con);    // 释放一个数据库连接
    int GetFreeConn();  // 获取可用连接数
    void DestoryPool(); // 销毁所有数据库连接

    // 初始化数据库连接池
    void init(string url, string User, string PassWord, string DBName, int port, int MaxConn, int close_log);

    string m_url;           // 主机地址
    string m_Port;          // 端口号
    string m_User;          // 数据库用户名
    string m_PassWord;      // 数据库密码
    string m_DatabaseName;  // 数据库名称
    int m_close_log;        // 是否开启日志

private:
    connection_pool();
    ~connection_pool();

    int m_MaxConn;  // 最大连接数
    int m_FreeConn; // 可用连接数
    int m_CurConn;  // 已用连接数
    list<MYSQL *> connlist; // 连接池，本质上是一个装载MYSQL*的列表

    locker m_mutex;
    sem reserve;
};


/*使用RAII技术来保证connPool单例对象的生命周期符合RAII规则*/
class connectionRAII {
public:
    connectionRAII(MYSQL **con, connection_pool *connPool);
    ~connectionRAII();
 
private:
    connection_pool *poolRALL;
    MYSQL *conRAII;
};
#endif