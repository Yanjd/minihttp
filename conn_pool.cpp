#include"conn_pool.h"



conn_pool* conn_pool::GetInstance()
{
    static conn_pool connPool;
    return &connPool;
}

conn_pool::conn_pool()
{
    this->curConn=0;
    this->freeConn=0;
}

#include"conn_pool.h"



conn_pool::~conn_pool()
{
    destroyPool();
}

void conn_pool::init(std::string url,std::string user,std::string pswd,std::string dbname,int port,unsigned int max_conn)
{
    this->url=url;
    this->port=port;
    this->user=user;
    this->pswd=pswd;
    this->dbname=dbname;

    for(int i=0;i<max_conn;i++)
    {
        MYSQL *conn=NULL;
        conn=mysql_init(conn);

        if(conn==NULL)
        {
            std::cout<<"error:"<<mysql_error(conn);
        }
        conn=mysql_real_connect(conn,url.c_str(),user.c_str(),password.c_str(),dbname.c_str(),port,NULL,0);

        if(conn==NULL)
        {
            std::cout<<"error:"<<mysql_error(conn);
            exit(1);
        }

        connList.push_back(conn);
        ++freeConn;
    }
    reserve=sem(freeConn);
    this->max_conn=freeConn;

}

MYSQL* conn_pool::getConn()
{
    MYSQL* conn=NULL;

    if(0==connList.size())
    {
        return NULL;
    }

    reserve.wait();
    lock.lock();

    conn=connList.front();
    connList.pop_front();

    --freeConn;
    ++curConn;
    lock.unlock();
    return conn;
}

bool conn_pool::releaseConn(MYSQL* conn)
{
    if(NULL==conn)
    {
        return false;
    }
    lock.lock();
    connList.push_back(conn);
    ++freeConn;
    --curConn;
    lock.unlock();
    reserve.post();
    return true;
}

void conn_pool::destroyPool()
{
    lock.lock();
    if(connList.size()>0)
    {
        list<MYSQL*>::iterator iter;
        for(iter=connList.begin();iter!=connList.end();++iter)
        {
            MYSQL* connM=*iter;
            mysql_close(connM);
        }
        connList.clear();
        lock.unlock();
    }
    lock.unlock();
}

connRAII::connRAII(MYSQL** conn,conn_pool* connPool)
{
    *conn=connPool->getConn();

    conn_RAII=*conn;
    pool_RAII=connPool;
}

connRAII::~connRAII()
{
    pool_RAII->releaseConn(conn_RAII);
}