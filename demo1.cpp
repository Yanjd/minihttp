#include<string.h>
#include<stdio.h>
#include"timer.h"
#include"http.h"

void timer_handler();
{
    timer_list.tick();
    alarm(TIMESLOT);
}

static asc_timer_list timer_list;

client_data *user_timer=new client_data[MAX_FD];

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
        if(sockfd==listenfd)
        {
            struct sockaddr_in client_addr;
            socklen_t client_addr_len=sizeof(client_addr);
            int connfd=accept(listenfd,(struct sockaddr*)&client_addr,&client_addr_len);
            users_timer[connfd].address=client_addr;
            users_timer[connfd].sockfd=connfd;
            active_timer *timer=new active_timer;
            timer->user_data=&users_timer[connfd];
            timer->cb_func=cb_func;
            time_t cur=time(NULL);

            timer->expire=cur+3*TIMESLOT;

            users_timer[connfd].timer=timer;

        }
        else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
        {
            cb_func(&users_timer[sockfd]);
            active_timer* timer=users_timer[sockfd].timer;
            if(timer)
            {
                timer_list.del_timer(timer);
            }
        }
        else if((sockfd==pipefd[0])&&(events[i].events & EPOLLIN))
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
        else if(events[i].events & EPOLLIN) 
        {
            active_timer* timer=users_timer[sockfd].timer;
            if(users[sockfd].write())
            {
                if(timer)
                {
                    time_t cur=time(NULL);
                    timer->expire=cur+3*TIMESLOT;
                    timer_list.adjust_timer(timer);
                }
            }
            else
            {
                cb_func(&users_timer[sockfd]);
                if(timer)
                {
                    timer_list.delete_timer(timer);
                }
            }
        }
    }
    if(timeout)
    {
        timer_handler();
        timeout=false;
    }
}