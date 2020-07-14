#pragma once
#include<stdio.h>
#include<sys/types.h>
#include<unistd.h>
#include<stdlib.h>
#include"locker.h"
#include"block_queue.h"
class Log
{
public:
    static Log *get_instance()
    {
        static Log instance;
        return &instance;
    }

    bool init(const char* filename,int log_buf_size=8192,int max_lines=5000000,int max_queue_size=0);

    static void* flush_log_thread(void* args)
    {
        //异步写日志的共有方法，调用私有方法async_write_log
        Log::get_instance()->async_write_log();
    }

    void write_log(int level,const char* format,...);

    void flush(void);

private:
    Log();
    virtual ~Log();

    void* async_write_log()
    {
        string single_log;
        while(m_log_queue->pop(single_log))
        {
            m_mutex.lock();
            fputs(single_log.c_str(),m_fp);
            m_mutex.unlock();
        }
    }

private:
    char dir_name[128];
    char log_name[128];
    int m_max_lines;
    int m_log_buf_size;
    long long m_count;
    int m_today;
    FILE* m_fp;
    char* m_buf;
    block_queue<string>* m_log_queue;
    bool m_is_async;
    locker m_mutex;

}

#define LOG_DEBUG(format, ...) Log::get_instance()->write(0,format, __VA_ARGS__)
#define LOG_INFO(format, ...) Log::get_instance()->write(0,format, __VA_ARGS__)
#define LOG_WARN(format, ...) Log::get_instance()->write(0,format, __VA_ARGS__)
#define LOG_ERROR(format, ...) Log::get_instance()->write(0,format, __VA_ARGS__)

