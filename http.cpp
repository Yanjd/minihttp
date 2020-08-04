#include "http.h"

//存放请求的资源和跳转的html文件
const char* root_path="/home/myProject/root";

bool http_connection::read_once()
{
    if(m_read_index>=READ_BUFFER_SIZE)
    {
        return false;
    }
    int byte_read=0;
    while(true)
    {
        byte_read=recv(m_sockfd,m_read_buf+m_read_index,READ_BUFFER_SIZE-m_read_index,0);
        if(byte_read==-1)
        {
            if(errno==EAGAIN||errno==EWOULDBLOCK)
            {
                break;
            }
            return false;
        }
        else if(byte_read==0)
        {
            return false;
        }
        m_read_index+=byte_read;
        
    }
    return true;
}

int setnonblocking(int fd)
{
    int old_option=fcntl(fd,F_GETFL);
    int new_option=old_option | O_NONBLOCK;
    fcntl(fd,F_SETFL,new_option);
    return old_option;
}

void addfd(int epollfd,int fd,bool one_shot)
{
    epoll_event event;
    event.data.fd=fd;
#ifdef ET
    event.events=EPOLLIN | EPOLLET | EPOLLRDHUP;
#endif

#ifdef LT
    event.events=EPOLLIN |EPOLLRDHUP;
#endif
    if(one_shot)
        event.events | =EPOLLONESHOT;
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
    setnonblocking(fd);
}

void removefd(int epollfd,int fd)
{
    epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,0);
    close(fd);
}

void modify(int epollfd,int fd,int ev)
{
    epoll_event event;
    event.data.fd=fd;

#ifdef ET
    event.events=ev| EPOLLET| EPOLLRDHUP| EPOLLONESHOT
#endif

#ifdef LT
    event.events=ev| EPOLLONESHOT| EPOLLRDHUP
#endif

    epoll_ctl(epollfd,EPOLL_CTL_MOD,fd,&event);
}

void http_connection::process()
{
    HTTP_CODE read_ret=process_read();
    if(read_ret==NO_REQUEST)
    {
        modify(m_epollfd,m_sockfd,EPOLLIN);
        return;
    }

    bool write_ret=process_write(read_ret);
    if(!write_ret)
    {
        close_connection();
    }
    modify(m_epollfd,m_sockfd,EPOLLOUT);
}


HTTP_CODE http_connection::process_read()
{
    LINE_STATUS line_status=LINE_OK;
    HTTP_CODE ret=NO_REQUEST;
    char* text=0;
    while((m_check_state==CHECK_STATE_CONTENT && line_status==LINE_OK)
    ||(line_status=parse_line())==LINE_OK)
    {
        text=get_line();
        
        m_start_line = m_checked_index;

        switch(m_check_state)
        {
            case CHECK_STATE_REQUESTLINE:
            {
                ret=parse_request_line(text);
                if(ret==BAD_REQUEST)
                {
                    return BAD_REQUEST;
                }
                break;
            }
            case CHECK_STATE_HEADER:
            {
                ret=parse_headers(text);
                if(ret==BAD_REQUEST)
                {
                    return BAD_REQUEST;
                }
                else if(ret==GET_REQUEST)
                {
                    return do_request();
                }
                break;
            }
            case CHECK_STATE_CONTENT:
            {
                ret=parse_content(text);
                if(ret==GET_REQUEST)
                {
                    return do_request();
                }
                line_status=LINE_OPEN;
                break;
            }
            default:
                return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}

LINE_STATUS http_connection::parse_line()
{
    char tmp;
    for(;m_checked_index<m_read_index;++m_checked_index)
    {
        tmp=m_read_buf[m_checked_index];
        if(tmp=='\r')
        {

            if((m_checked_index+1)==m_read_index)
            {
                return LINE_OPEN;
            }
            else if(m_read_buf[m_checked_index+1]=='\n')
            {
                m_read_buf[m_checked_index++]='\0';
                m_read_buf[m_checked_index++]='\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if(tmp=='\n')
        {
            if(m_checked_index>1&&m_read_buf[m_checked_index-1]=='\r')
            {
                m_read_buf[m_checked_index-1]='\0';
                m_read_buf[m_checked_index++]='\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        
    }

    return LINE_OPEN;
}

HTTP_CODE http_connection::parse_request_line(char* text)
{   
    m_url=strpbrk(text," \t");
    if(!m_url)
    {
        return BAD_REQUEST;
    }
    *m_url++='\0';
    char* method=text;
    if(strcasecmp(method,"GET")==0)
    {
        m_method=GET;
    }
    else if(strcasecmp(method,"POST")==0)
    {
        m_method=POST;
        cgi=1;
    }
    else
    {
        return BAD_REQUEST;
    }
    m_url+=strspn(m_url," \t");
    m_version=strpbrk(m_url," \t");
    if(!m_version)
    {
        return BAD_REQUEST;
    }
    *m_version++='\0';
    m_version+=strspn(m_version," \t");

    if(strcasecmp(m_version,"HTTP/1.1")!=0)
    {
        return BAD_REQUEST;
    }

    if(strncasecmp(m_url,"http://",7)==0)
    {
        m_url+=7;
        m_url=strchr(m_url,'/');
    }
    if(strncasecmp(m_url,"https://",8)==0)
    {
        m_url+=8;
        m_url=strchr(m_url,'/');
    }

    if(!m_url||m_url[0]!='/')
    {
        return BAD_REQUEST;
    }
    if(strlen(m_url)==1)
    {
        strcat(m_url,"welcome.html");
    }

    m_check_state=CHECK_STATE_HEADER;
    return NO_REQUEST;

}

HTTP_CODE http_connection::parse_headers(char* text)
{
    if(text[0]=='\0')
    {
        if(m_content_length!=0)
        {
            m_check_state=CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }

    else if(strncasecmp(text,"Connection:",11)==0)
    {
        text+=11;
        text+=strspn(text," \t");
        if(strcasecmp(text,"keep_alive")==0)
        {
            m_linger=true;
        }
    }
    else if(strcasecmp(text,"Content-length",15)==0)
    {
        text+=15;
        text+=strspn(text," \t");
        m_content_length=atol(text);
    }
    else if(strncasecmp(text,"Host",5)==0)
    {
         text+=5;
         text+=strspn(text,' \t');
         m_host=text;
    }
    else
    {
        printf("unknown header: %s\n",text);
    }

    return NO_REQUEST;
 
}

HTTP_CODE http_connection::parse_content(char *text)
{
    if(m_read_index>=(m_content_length+m_checked_index))
    {
        text[m_content_length]='\0';
        m_string=text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

HTTP_CODE http_connection::do_request()
{
    strcpy(m_real_file,root_path);
    int plen=strlen(root_path);

    const char* p=strrchr(m_url,'/');

    if(cgi==1&&(*(p+1)=='2'||*(p+1)=='3'))
    {
        //登陆和注册功能
    }

    if(*(p+1)=='0')
    {
        char* real_path=(char*)malloc(sizeof(char)*200);
        strcpy(real_path,"/register.html");
        strncpy(m_real_file+plen,real_path,strlen(real_path));
        free(real_path);
    }
    else if(*(p+1)=='1')
    {
        char *real_path=(char*)malloc(sizeof(char)*200);
        strcpy(real_path,"/log.html");
        strncpy(m_real_file+plen,real_path,strlen(real_path));
        free(real_path);
    }
    else
    {
        strncpy(m_real_file+plen,m_url,FILENAME_LEN-plen-1);
    }

    if(stat(m_real_file,&m_file_stat)<0)
    {
        return NO_RESOURCE;
    }
    if(!(m_file_stat.st_mode&S_IROTH))
    {
        return FORBIDDEN_REQUEST;
    }
    if(S_ISDIR(m_real_file,st_mode))
    {
        return BAD_REQUEST;
    }

    int fd=open(m_real_file,O_RDONLY);
    m_file_address=(char *)mmap(0,m_file_stat.st_size,PROT_READ,MAP_PRIVATE,fd,0);
    close(fd);
    return FILE_REQUEST;
}

bool http_connection::add_response(const char* format, ...)
{
    if(m_write_index>=WRITE_BUFFER_SIZE)
    {
        return false;
    }

    va_list arg_list;
    va_start(arg_list, format);
    int len=vsnprintf(m_write_buf+m_write_index,WRITE_BUFFER_SIZE-1-m_write_index,format,arg_list);
    if(len>=(WRITE_BUFFER_SIZE-1-m_write_index))
    {
        va_end(arg_list);
        return false;
    }

    m_write_index+=len;
    va_end(arg_list);
    return true;
}

//存在问题
bool http_connection::add_headers(int content_len)
{
    add_content_length(int content_len);
    add_linger();
    add_blank_line();
}

bool http_connection::add_content_length(int content_len)
{
    return add_response("Content-Length:%d\r\n",content_len);
}

bool http_connection::add_status_line(int status,const char* title)
{
    return add_response("%s %d %s\r\n","HTTP/1.1",status,title);
}

bool http_conncetion::add_content_type()
{
    return add_response("Content-Type:%s\r\n","text.html");
}

bool http_connection::add_linger()
{
    return add_response("Connection:%s\r\n",(m_linger==true)?"keep-alive","close");
}

bool http_connection::add_blank_line()
{
    return add_response("%s","\r\n");
}

bool http_connection::add_content(const char* content)
{
    return add_response("%s",content);
}

bool http_connection::process_write(HTTP_CODE ret)
{
    switch(ret)
    {
        case INTERNAL_ERROR:
        {
            add_status_line(500,error_500_title);
            add_headers(strlen(error_500_form));
            if(!add_content(error_500_form))
            {
                return false;
            }
            break;
        }
        case BAD_REQUEST:
        {
            add_status_line(404,error_404_title);
            add_headers(strlen(error_404_form));
            if(!add_content(error_404_form));
            {
                return false;
            }
            break;
        }
        case FORBIDDEN_REQUEST:
        {
            add_status_line(404,error_403_title);
            add_headers(strlen(error_403_form));
            if(!add_content(error_403_form));
            {
                return false;
            }
            break;
        }
        case FILE_REQUEST:
        {
            add_status_line(200,ok_200_title);
            if(m_file_stat.st_size!=0)
            {
                add_headers(m_file_stat.st_size);
                m_iv[0].iov_base=m_write_buf;
                m_iv[0].iov_len=m_write_index;
                m_iv[1].iov_base=m_file_address;
                m_iv[1].iov_len=m_file_stat.st_size;
                m_iv_count=2;
                
                bytes_to_send=m_write_index+m_file_stat.st_size;
                return true;
            }
            else
            {
                const char* ok_string="<html><body></body></html>";
                add_headers(strlen(ok_string))
                if(!add_content(ok_string))
                {
                    return false;
                }
            }
            default:
            {
                return false;
            }
            
        }

        m_iv[0]=iov_base=m_write_buf;
        m_iv[0].iov_len=m_write_index;
        m_iv_count=1;
        return true;
    }
}

bool http_connection::write()
{
    int tmp=0;
    int newadd=0;
    if(bytes_to_send==0)
    {
        modify(m_epollfd,m_sockfd,EPOLLIN);
        init();
        return true;
    }
    while(true)
    {
        tmp=writev(m_sockfd,m_iv,m_iv_count);
        if(tmp>0)
        {
            bytes_have_send+=tmp;
            newadd=bytes_have_send-m_write_index;
        }
        if(tmp<=-1)
        {
            //判断缓冲区是否已满
            if(errno==EAGAIN)
            {
                if(bytes_have_send>=m_iv[0].iov_len)
                {
                    m_iv[0].iov_len=0;
                    m_iv[1].iov_base=m_file_address+newadd;
                    m_iv[1].iov_len=bytes_to_send;
                }
                else
                {
                    m_iv[0].iov_base=m_write_buf+bytes_to_send;
                    m_iv[0].iov_len=byte_to_send;
                }
                modify(m_epollfd,m_sockfd,EPOLLOUT);
                return true;
            }
            unmap();
            return false;
        }
        bytes_to_send-=tmp;
        if(bytes_to_send<=0)
        {
            unmap();
            modify(m_epollfd,m_sockfd,EPOLLIN);

            if(m_linger)
            {
                init();
                return true;
            }
            else
            {
                return false;
            }
            
        }
    }
}

void http_connection::initmysql_result(connection_pool *connPool)
{
    MYSQL *mysql =NULL;
    connRAII mysqlconn(&mysql,connPool);
    
    if(mysql_query(mysql,"SELECT username, password FROM user"))
    {
        LOG_ERROR("SELECT error:%s\n",mysql_error(mysql));
    }

    MYSQL_RES* res=mysql_store_result(mysql);

    int num_fields=mysql_num_fields(res);

    MYSQL_FIELD* fields=mysql_fetch_fields(res);

    while(MYSQL_ROW row=mysql_fetch_row(res))
    {
        string tmp1(row[0]);
        string tmp2(row[1]);
        users[tmp1]=tmp2;
    }
}
HTTP_CODE http_connection::parse_content(char* text)
{
    if(m_read_index>=(m_content_length+m_checked_index))
    {
        text[m_content_length]='\0';

        m_string=text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

void http_connection::get_user_info()
{
    char flag=m_url[1];
    char* m_url_real=(char*)malloc(sizeof(char)*200);
    strcpy(m_url_real,"/");
    strcat(m_url_real,m_url+2);

    //len需要改
    strncpy(m_real_file+len,m_url_real,FILENAME_LEN-len-1);
    free(m_url_real);

    char name[100],password[100];
    int i;
    for(i=5;m_string[i]!='&';++i)
    {
        name[i-5]=m_string[i];
    }
    name[i-5]='\0';

    int j=0;
    for(i=i+10;m_string[i]!='\0';++i,++j)
    {
        password[j]=m_string[i];
    }
    password[j]='\0';
}

