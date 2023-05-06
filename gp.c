#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdbool.h>
#include "gp.h"


// 定义一个函数，根据请求方法、URL和参数发送HTTP请求,并将响应和异常写入传入的缓冲区中
int sendHttpRequest(char *method, char *url, char *params,GP_HTTP_RESPONSE *res,char *err_message) {
    // 创建一个URL结构体，解析URL中的主机名、端口号、路径等信息
    struct URL {
        char *protocol;
        char *host;
        int port;
        char *path;
    };
    struct URL u;   //定义请求信息结构体变量
    SSL *ssl;       //定义SSL结构体变量
    bool is_https = false;  //记录请求是不是https
    char *response_buffer = (char *)malloc(GP_RESPONSE_BUFFER_LEN); //用于接收请求的临时缓冲区

    //初始化ssl
    SSL_CTX *ctx;
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    // 定义一个包含随机数据的缓冲区作为ssl加密随机数
    static const char rnd_seed[] = "string to make the random number generator think it has entropy";
	// 生成随机数
    RAND_seed(rnd_seed, sizeof rnd_seed);		
    // 创建一个SSL_CTX对象，使用SSLv23_client_method方法
    ctx = SSL_CTX_new(SSLv23_client_method());
    if(ctx == NULL)
    {
        strcmp(err_message,"初始化ssl失败");
        free(response_buffer);
        return 1;
    }

    //允许请求方法使用小写
    char *real_method;
    if (strcmp(method,"GET") == 0 || strcmp(method,"get") == 0) {
        method = "GET";
    } else if (strcmp(method,"POST") == 0 || strcmp(method,"post") == 0) {
        method = "POST";
    } else {
        method = method;
    }

    // 参数异常判断
    if (method == NULL || url == NULL){
        char *err = "lost method or url";
        strcpy(err_message,err);
        free(response_buffer);
        return 1;
    }

    // 使用strtok函数分割URL字符串
    int url_len = strlen(url); // 计算url字符串的长度
    char *url_cut_host_protocol_buffer = malloc(url_len + 1); // 分配字符串空间用于分割，多出一个字节用来存储'\0'
    memset(url_cut_host_protocol_buffer, 0, sizeof(url_cut_host_protocol_buffer)); // 初始化为0
    strcpy(url_cut_host_protocol_buffer, url); //复制url的值到url字符切割缓冲区
    
    //获取协议名
    char *token = strtok(url_cut_host_protocol_buffer, ":/"); // 按照:和/任意有一个就替换分割
    if ((token == NULL) || (strcmp(token,"http") != 0 && strcmp(token,"https")!=0)  ){
        char *err = "Invalid URL";
        strcpy(err_message,err);
        free(response_buffer);
        free(url_cut_host_protocol_buffer); //释放用于分割协议和host的内存    
        return 1;
    }
    u.protocol = token; // 第一个分割出来的是协议
    //获取主机名
    token = strtok(NULL, ":/"); // 再次分割
    if (strcmp(token, "") == 0) { // 如果分割出来的是空字符串，说明有//
        token = strtok(NULL, ":/"); // 再次分割
        u.host = token; // 这次分割出来的是主机名
    } else {
        u.host = token; // 否则直接是主机名
    }

    //获取uri路径   
    token = strtok(NULL, ":/"); //继续切割
    char *path_cut_uri_buffer; //如果uri路径不是'/',就定义切割缓冲区
    path_cut_uri_buffer = malloc(url_len + 1); // 分配字符串空间用于分割，多出一个字节用来存储'\0'
    memset(path_cut_uri_buffer, 0, sizeof(path_cut_uri_buffer)); // 初始化为0

    if (token == NULL){
        u.path="/";
    }else {
        while (token != NULL) //循环切割路径
        {
            sprintf(path_cut_uri_buffer + strlen(path_cut_uri_buffer), "/%s", token); //拼接路径
            token = strtok(NULL, ":/");
        }
        u.path = path_cut_uri_buffer;
    }

    //获取端口号
    char *url_cut_port_buffer = malloc(url_len + 1); // 分配字符串空间用于分割，多出一个字节用来存储'\0'
    memset(url_cut_port_buffer, 0, sizeof(url_cut_port_buffer)); // 初始化为0
    strcpy(url_cut_port_buffer, url); // 复制path到字符切割缓冲区
    token = strtok(url_cut_port_buffer, ":"); // 按照:分割
    token = strtok(NULL, ":"); // 再次分割
    token = strtok(NULL, ":"); // 再次分割
    if (token == NULL) { // 如果没有分割出来，说明没有指定端口号
        if (strcmp(u.protocol, "http") == 0) { // 如果协议是http，默认端口为80
            u.port = 80;
        } else if (strcmp(u.protocol, "https") == 0) { // 如果协议是https，默认端口为443
            u.port = 443;
            is_https = true;
        } else { // 否则抛出异常
            char *err = "Invalid Protocol";
            strcpy(err_message,err);
            free(response_buffer);
            free(url_cut_host_protocol_buffer); //释放用于分割协议和host的内存    
            free(url_cut_port_buffer);// 释放用于分割端口号的内存  
            free(path_cut_uri_buffer); //释放用于分割uri的内存
            return 1;
        }
    } else {
        u.port = atoi(token); // 否则把分割出来的字符串转换成整数作为端口号
    }

    //构建服务器的信息体
    struct sockaddr_in server_addr; // 定义一个服务器地址结构体
    memset(&server_addr, 0, sizeof(server_addr)); // 初始化为0
    server_addr.sin_family = AF_INET; // 地址族为IPv4
    server_addr.sin_port = htons(u.port); // 端口号，注意转换成网络字节序

    struct hostent *host = gethostbyname(u.host); // 根据主机名获取IP地址
    if (host == NULL) { // 如果获取失败，抛出异常
        char *err = "gethostbyname error";
        strcpy(err_message,err);
        free(response_buffer);
        free(url_cut_host_protocol_buffer); //释放用于分割协议和host的内存    
        free(url_cut_port_buffer);// 释放用于分割端口号的内存  
        free(path_cut_uri_buffer); //释放用于分割uri的内存
        return 1;
    }

    memcpy(&server_addr.sin_addr.s_addr, host->h_addr_list[0], host->h_length); // 把IP地址复制到服务器地址结构体中


    // 创建一个Socket对象，连接到指定的主机和端口
    int sock = socket(AF_INET, SOCK_STREAM, 0); // 创建一个IPv4的TCP套接字
    if (sock < 0) { // 如果创建失败，抛出异常
        char *err ="socket error";
        strcpy(err_message,err);
        free(response_buffer);
        free(url_cut_host_protocol_buffer); //释放用于分割协议和host的内存    
        free(url_cut_port_buffer);// 释放用于分割端口号的内存  
        free(path_cut_uri_buffer); //释放用于分割uri的内存
        return 1;
    }

    //创建socket连接
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) { // 连接到服务器，如果失败，抛出异常
        char *err = "connect error";
        strcpy(err_message,err);
        free(response_buffer);
        free(url_cut_host_protocol_buffer); //释放用于分割协议和host的内存    
        free(url_cut_port_buffer);// 释放用于分割端口号的内存  
        free(path_cut_uri_buffer); //释放用于分割uri的内存
        return 1;
        }

    if(is_https){
        // 创建一个SSL对象
        ssl = SSL_new(ctx);
        if(ssl == NULL)
        {
            // 如果创建失败，打印错误信息并返回-1
            ERR_print_errors_fp(stderr);
            free(response_buffer);
            free(url_cut_host_protocol_buffer); //释放用于分割协议和host的内存    
            free(url_cut_port_buffer);// 释放用于分割端口号的内存  
            free(path_cut_uri_buffer); //释放用于分割uri的内存
            return -2;
        }
        // 将SSL对象和socket绑定
        SSL_set_fd(ssl, sock);
        // 建立SSL连接
        if(SSL_connect(ssl) == -1)
        {
            // 如果连接失败，打印错误信息并返回-1
            ERR_print_errors_fp(stderr);
            free(response_buffer);
            free(url_cut_host_protocol_buffer); //释放用于分割协议和host的内存    
            free(url_cut_port_buffer);// 释放用于分割端口号的内存  
            free(path_cut_uri_buffer); //释放用于分割uri的内存
            return -3;
        }        
    }

    // 创建一个字符串缓冲区，用于拼接HTTP请求报文
    char request[1024];
    memset(request, 0, sizeof(request)); // 初始化为0

    // 计算路径和参数加起来的长度用于申请网站路径拼接的缓冲区内存
    int path_len = strlen(u.path); 
    int params_len = 0;
    if (params != NULL){
        params_len =strlen(params);
    }
    
    char *path_with_params_buffer = malloc(path_len + params_len + 1); // 分配22个字节的空间，多出一个字节用来存储'\0'
    memset(path_with_params_buffer, 0, sizeof(path_with_params_buffer)); // 初始化为0
    strcpy(path_with_params_buffer, u.path); // 复制path值到带参路径拼接缓冲区
    // 根据请求方法的不同，拼接不同的请求行
    if (strcmp(method, GP_HTTP_GET) == 0) {
        // 如果是GET方法，需要把参数拼接到URL后面
        if (params != NULL && strlen(params) > 0) {
            strcat(path_with_params_buffer, "?");
            strcat(path_with_params_buffer, params);
        }
        sprintf(request, "%s %s %s%s", GP_HTTP_GET, path_with_params_buffer, GP_HTTP_VERSION, GP_CRLF);
    } else if (strcmp(method, GP_HTTP_POST) == 0) {
        // 如果是POST方法，需要把参数的长度写到请求头中
        sprintf(request, "%s %s %s%s", GP_HTTP_POST, u.path, GP_HTTP_VERSION, GP_CRLF);
        if (params != NULL){
            sprintf(request + strlen(request), "Content-Length: %ld%s", strlen(params), GP_CRLF);
        }
        sprintf(request + strlen(request), "Content-Type: %s%s", "application/x-www-form-urlencoded", GP_CRLF);
    } else {
        // 如果既不是GET也不是POST，抛出异常
        char *err ="Invalid HTTP method";
        strcpy(err_message,err);
        free(response_buffer);
        free(url_cut_host_protocol_buffer); //释放用于分割协议和host的内存    
        free(path_with_params_buffer); //释放用于拼接uri和参数的内存  
        free(url_cut_port_buffer);// 释放用于分割端口号的内存  
        free(path_cut_uri_buffer); //释放用于分割uri的内存
        return 1;
    }

    // 拼接通用的请求头，如主机名、连接方式等
    sprintf(request + strlen(request), "Host: %s%s", u.host, GP_CRLF);
    sprintf(request + strlen(request), "Connection: close%s", GP_CRLF); // 使用短连接，请求结束后关闭连接

    // 拼接一个空行，表示请求头结束
    sprintf(request + strlen(request), "%s", GP_CRLF);

    // 如果是POST方法，还需要拼接请求体，即参数
    if (strcmp(method, GP_HTTP_POST) == 0 && params != NULL ) {
        sprintf(request + strlen(request), "%s", params,GP_CRLF);
    }

    if(is_https)
    {
        SSL_write(ssl, request, strlen(request));
    }
    else
    {
        // 把字符串缓冲区的内容发送到输出流中
        write(sock, request, strlen(request));
    }

    // 响应接收缓冲区清空
    memset(response_buffer, 0, sizeof(response_buffer));

    // 定义一个整数，用于记录读取的字节数
    int bytes = 0;

    // 定义一个布尔值，用于标记是否已经读取到响应报文的头部
    bool header_read = false;

    if (is_https){
        while ((bytes = SSL_read(ssl, response_buffer + strlen(response_buffer), GP_RESPONSE_BUFFER_LEN - strlen(response_buffer))) > 0) {
            // 如果读取成功，就继续拼接到字符串缓冲区中
            if (bytes < 0) { // 如果读取失败，抛出异常
                char *err = "read error";
                strcpy(err_message,err);
                free(response_buffer);
                free(url_cut_host_protocol_buffer); //释放用于分割协议和host的内存    
                free(path_with_params_buffer); //释放用于拼接uri和参数的内存  
                free(url_cut_port_buffer);// 释放用于分割端口号的内存  
                free(path_cut_uri_buffer); //释放用于分割uri的内存
                return 1;
            } else if (bytes == 0) { // 如果读取到末尾，跳出循环
                break;
            }       
        }
    } else {
        // 循环读取输入流中的数据，直到读到末尾或者读完主体
        while ((bytes = read(sock, response_buffer + strlen(response_buffer), GP_RESPONSE_BUFFER_LEN - strlen(response_buffer))) > 0) {
            // 如果读取成功，就继续拼接到字符串缓冲区中
            if (bytes < 0) { // 如果读取失败，抛出异常
                char *err = "read error";
                strcpy(err_message,err);
                free(response_buffer);
                free(url_cut_host_protocol_buffer); //释放用于分割协议和host的内存    
                free(path_with_params_buffer); //释放用于拼接uri和参数的内存  
                free(url_cut_port_buffer);// 释放用于分割端口号的内存  
                free(path_cut_uri_buffer); //释放用于分割uri的内存
                return 1;
            } else if (bytes == 0) { // 如果读取到末尾，跳出循环
                break;
            }
        }

    }

    char *body_location = strstr(response_buffer, "\r\n\r\n");
    int body_deviation = body_location - response_buffer ;

    strcpy(res->body,body_location +4);
    sprintf (res->headers, "%.*s", body_deviation+1, response_buffer); // 拷贝 arr1 的前 num 个    // 如果是https请求，就需要释放SSL对象
    if(is_https)
    {
        SSL_free(ssl);
    }

    // 关闭套接字
    close(sock);
    free(url_cut_host_protocol_buffer); //释放用于分割协议和host的内存    
    free(path_with_params_buffer); //释放用于拼接uri和参数的内存  
    free(url_cut_port_buffer);// 释放用于分割端口号的内存  
    free(path_cut_uri_buffer); //释放用于分割uri的内存
    free(response_buffer);
    SSL_CTX_free(ctx); //清理https
    // 返回结果
    return 0;
}

// 定义一个主函数，用于测试
int main(int argc, char *argv[]) {
    // 测试用
    char *url = "https://www.baidu.com/";
    char *method = "get";
    char *params = "h";
    char *content = NULL;    
    //获取请求方法
    // char *method = argv[1];
    // // 从命令行参数中获取URL字符串
    // char *url = argv[2];
    // // 从命令行参数中获取参数字符串
    // char *params = argv[3];
    // // 从命令行参数中获取参数字符串
    // char *content = argv[4];
    //body缓冲区
    char *body = (char *)malloc(40960);
    //header缓冲区
    char *headers = (char *)malloc(40960);
   
    //定义错误信息返回
    char err_message[50];
    memset(err_message, 0, sizeof(err_message)); // 初始化为0
    GP_HTTP_RESPONSE *res= (GP_HTTP_RESPONSE *)calloc(1, sizeof(GP_HTTP_RESPONSE));
    // 调用函数发送请求，并打印响应结果
    // POST请求的Conten-Type默认是application/x-www-form-urlencoded
    res->body = body;
    res->headers = headers;

    int ok = sendHttpRequest(method,url, params,res,err_message);

    if (ok == 0) {
        if (content == NULL){
            printf("\n%s\n", res->body);
        }else if (strcmp(content,"h")==0){
            printf("\n%s\n", res->headers);
        }
        else if (strcmp(content,"b")==0){
            printf("\n%s\n", res->body);
        }else {
            printf("\n%s\n", res->body);
        }

    } else {
        printf("请求失败:%s  %d\n",err_message,ok);
    }
    // 调用清理内存
    free(body);
    free(headers);
    free(res);
    return 0;
 }