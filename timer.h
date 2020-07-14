#include<stdio.h>
#include<string.h>
#include<signal.h>
#include"http.h"

class active_timer;

struct client_data 
{
    sockaddr_in address;
    int sockfd;
    active_timer* timer;
};

class active_timer
{
public:
    time_t expire;
    void (*cb_func)(client_data*);
    client_data* user_data;

    active_timer* prev;
    active_timer* next;
};

void cb_func(client_data* user_data)
{
    epoll_ctl(epollfd,EPOLL_CTL_DEL,user_data->sockfd,0);
    assert(user_data);

    close(user_data->sockfd);
    http_connection::m_user_count--;
}

class asc_timer_list
{
public:
    asc_timer_list():head(NULL),tail(NULL){}
    ~asc_timer_list();

    void add_timer(active_timer* timer);
    void adjust_timer(active_timer* timer);
    void del_timer(active_timer* timer);

    void tick();
private:
    void add_timer(active_timer* timer,active_timer* head);
private:
    active_timer* head;
    active_timer* tail;
    
};

