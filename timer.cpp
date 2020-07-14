#include"timer.h"

//信号处理函数
void sig_handler(int sig)
{
    //保留之前的errno，保证可重入
    int save_err=errno;
    int msg=sig;
    
    send(pipefd[1],(char*)&msg,1,0);

    errno=save_err;
}

void addsig(int sig,void(handler)(int),bool restart=true)
{
    struct sigaction sa;
    memset(&sa,0,sizeof(sa));
    
    sa.sa_handler=handler;
    if(restart)
    {
        sa.sa_flags|=SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig,&sa,NULL)!=-1);
}

#if 0
ret=socketpair(PF_UNIX,SOCK_STREAM,0,pipefd);
assert(ret!=-1);

setnonblocking(pipefd[1]);

addfd(epollfd,pipefd[0],false);

addsig(SIGALRM,sig_handler,false);
addsig(SIGTERM,sig_handler,false);

bool stop_server=false;

bool timeout=false;

alarm(TIMESLOT);

while(!stop_server)
{
    int number=epoll_wait(epollfd,events,MAX_EVENT_NUMBER,-1);
    if(number<0&&errno!=EINTR)
    {
        break;
    }
    for(int i=0;i<number;i++)
    {
        int sockfd=events[i].data.fd;
        if((sockfd==pipefd[0])&&(events[i].events & EPOLLIN))
        {
            int sig;
            char signals[1024];
            ret=recv(pipefd[0],signals,sizeof(signals),0);
            if(ret==-1)
            {
                continue;
            }
            else if(ret==0)
            {
                continue;
            }
            else
            {
                for(int i=0;i<ret;i++)
                {
                    switch(signals[i])
                    {
                        case SIGALRM:
                        {
                            timeout=true;
                            break;
                        }
                        case SIGTERM:
                        {
                            stop_server=true;
                        }
                        default:
                            break;
                    }
                }
            }
            
        }
    }
}
#endif

asc_timer::~asc_timer()
{
    active_timer* tmp=head;
    while(tmp)
    {
        head=tmp->next;
        delete tmp;
        tmp=head;
    }
}

void asc_timer::add_timer(active_timer* timer)
{
    if(!timer)
    {
        return;
    }
    if(!head)
    {
        head=tail=timer;
        return;
    }
    if(timer->expire<head->expire)
    {
        timer->next=head;
        head->prev=timer;
        head=timer;
        return;
    }
    add_timer(timer,head);
}

void asc_timer::adjust_timer(active_timer* timer)
{
    if(!timer)
    {
        return;
    }
    active_timer* tmp=timer->next;
    if(!tmp||(timer->expire<tmp->expire))
    {
        return;
    }

    if(timer==head)
    {
        head=head->next;
        head->prev=NULL;
        timer->next=NULL;
        add_timer(timer,head);
    }
    else
    {
        timer->prev->next=timer->next;
        timer->next->prev=timer->prev;
        add_timer(timer,head);
    }
    
}

void asc_timer::del_timer(active_timer* timer)
{
    if(!timer)
    {
        return;
    }
    if((timer==head)&&(timer==tail))
    {
        delete timer;
        head=NULL;
        tail=NULL;
        return;
    }
    if(timer==head)
    {
        head=head->next;
        head->prev=NULL;
        delete timer;
        return;
    }
    if(timer==tail)
    {
        tail=tail->prev;
        tail->next=NULL;
        delete timer;
        return;
    }
    timer->prev->next=timer->next;
    timer->next->prev=timer->prev;
    delete timer;
}

void asc_timer::add_timer(active_timer* timer,active_timer* head)
{
    active_timer* first_one=head;
    active_timer* tmp=first_one->next;
    while(tmp)
    {
        if(timer->expire<tmp->expire)
        {
            first_one->next=timer;
            timer->next=tmp;
            tmp->prev=timer;
            timer->prev=first_one;
            break;
        }
        first_one=tmp;
        tmp=tmp->next;
    }
    if(!tmp)
    {
        prev->next=timer;
        first_one->next=timer;
        timer->prev=first_one;
        timer->next=NULL;
        tail=timer;
    }
}

void asc_timer::tick() 
{
    if(!head)
    {
        return;
    }
    time_t cur=time(NULL);
    active_timer* tmp=head;

    while(tmp)
    {
        if(cur<tmp->expire)
        {
            break;
        }
        tmp->cb_func(tmp->user_data);

        head=tmp->next;
        if(head)
        {
            head->prev=NULL;
        }
        delete tmp;
        tmp=head;
    }
}