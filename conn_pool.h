#pragma once

#include<iostream>
#include<string>
#include<stdio.h>
#include<error.h>
#include<mysql/mysql.h>
#include<list>
#include"locker.h"


class conn_pool
{
public:
    static conn_pool* GetInstance();
    
    void init(string url,string user,string pswd,string dbname,int port,unsigned int max_conn);

    MYSQL* getConn();
    bool releaseConn();
    int getFreeConn();
    void destroyPool();


private:
    conn_pool();
    ~conn_pool();

private:
    string url;
    string user;
    string pswd;
    string dbname;
    int port;
    

private:
    sem reserve;
    unsigned int max_conn;
    int curConn;
    int freeConn;
    locker lock;
    List<MYSQL*> connList;

}

class connRAII
{
public:
    connRAII(MYSQL** conn,conn_pool* connPool);
    ~connRAII();

private:
    MYSQL* conn_RAII;
    conn_pool* pool_RAII;
}
