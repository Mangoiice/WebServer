#include"lst_timer.h"
#include"../http/http_conn.h"

// 上升链表构造函数，初始化头尾节点
sort_timer_lst::sort_timer_lst()
{
    head = NULL;
    tail = NULL;
}

// 上升链表析构函数，销毁整个链表
sort_timer_lst::~sort_timer_lst()
{
    util_timer* tmp = head;
    while(tmp)
    {
        head = tmp->next;
        delete tmp;
        tmp = head;
    }
}

void sort_timer_lst::add_timer(util_timer* timer)
{
    if(!timer) return;
    // 链表为空
    if(!head) head = tail = timer; return;

    // 需要放在头节点
    if(timer->expire < head->expire)
    {
        timer->next = head;
        head->prev = timer;
        head = timer;
    }

    add_timer(timer, head);
}

void sort_timer_lst::adjust_timer(util_timer* timer)
{
    if(!timer) return;

    util_timer* tmp = timer->next;
    // timer是尾节点或者timer的到期时间仍然是最后的情况
    if(!tmp || timer->expire < tmp->expire) return;

    // 实现思路是先把timer从链表中取出，再重新添加
    if(timer == head)
    {
        head = head->next;
        head->prev = NULL;
        timer->next = NULL;
        add_timer(timer, head);
    }
    else
    {
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer(timer, timer->next);
    }
}

void sort_timer_lst::del_timer(util_timer* timer)
{
    if(!timer) return;
    // 特殊情况：链表中只有timer
    if((timer == head) && (timer == tail))
    {
        delete timer;
        head = NULL;
        tail = NULL;
        return;
    }

    // timer是头节点
    if(timer == head)
    {
        head = timer->next;
        head->prev = NULL;
        delete timer;
        return;
    }

    // timer是尾节点
    if(timer == tail)
    {
        tail = timer->prev;
        tail->next = NULL;
        delete timer;
        return;
    }

    // timer是常规节点
    timer->next->prev = timer->prev;
    timer->prev->next = timer->next;
    delete timer;
    return;
}

void sort_timer_lst::add_timer(util_timer* timer, util_timer* lst_head)
{
    util_timer* prev = lst_head;
    util_timer* tmp = lst_head->next;
    while(tmp)
    {
        if(timer->expire < tmp->expire)
        {
            prev->next = timer;
            tmp->prev = timer;
            timer->prev = prev;
            timer->next = tmp;
            break;
        }

        prev = tmp;
        tmp = tmp->next;
    }

    if(!tmp)
    {
        prev->next = timer;
        timer->prev = prev;
        timer->next = NULL;
        tail = timer;
    }
}

void sort_timer_lst::tick()
{
    if(!head) return;

    time_t cur = time(NULL);
    util_timer* tmp = head;

    // 检查链表中的每一个定时器
    while(tmp)
    {
        // 因为是上升链表，所以当前的定时器没过期，以后的定时器都不会过期
        if(cur < tmp->expire) break;

        // 执行回调函数，删除连接
        tmp->cb_func(tmp->user_data);

        // 将当前定时器从链表中清除
        head = tmp->next;
        if(head)
        {
            head->prev = NULL;
        }
        delete tmp;
        tmp = head;
    }
}

void Utils::init(int timeslot)
{
    m_TIMESLOT = timeslot;
}

int Utils::setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void Utils::addfd(int epollfd, int fd, bool one_shot, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;

    if(TRIGMode == 1)
    {
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    }
    else 
    {
        event.events = EPOLLIN | EPOLLRDHUP;
    }
    if(one_shot)
    {
        event.events |= EPOLLONESHOT;
    }

    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void Utils::sig_handler(int sig)
{
    int save_errno = errno;
    int msg = sig;
    send(u_pipefd[1], (char*)&msg, 1, 0);
    errno = save_errno;
}

void Utils::addsig(int sig, void(handler)(int), bool restart)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    
    // 设置新的信号处理函数为handler
    sa.sa_handler = handler;
    // 设置restart，允许程序阻塞在系统调用的时候被信号打断，处理完信号后重新发起系统调用
    if(restart)
    {
        sa.sa_flags |= SA_RESTART;
    }
    // 添加所有信号到信号集中，在处理某个信号时，阻塞sa_mask中的信号，暂不处理
    sigfillset(&sa.sa_mask);
    // 设置对sig信号的处理方式
    assert(sigaction(sig, &sa, NULL) != -1);
}

void Utils::timer_handler()
{
    // 执行心搏函数，并且在经过m_TIMESLOT时间后发出SIGALRM信号
    m_timer_list.tick();
    alarm(m_TIMESLOT);
}

void Utils::show_error(int connfd, const char* info)
{
    send(connfd, info, sizeof(info), 0);
    close(connfd);
}

int *Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;

class Utils;
void cb_func(client_data* user_data)
{
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);

    close(user_data->sockfd);

    http_conn::m_user_count--;
}