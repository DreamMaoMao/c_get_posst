#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdbool.h>
// 定义一些常量
#define HTTP_GET "GET"
#define HTTP_POST "POST"
#define HTTP_VERSION "HTTP/1.1"
#define CRLF "\r\n" // 回车换行符

// 定义一个函数，根据请求方法、URL和参数发送HTTP请求，并返回响应结果
char *sendHttpRequest(char *method, char *url, char *params) {
    // 创建一个URL结构体，解析URL中的主机名、端口号、路径等信息
    struct URL {
        char *protocol;
        char *host;
        int port;
        char *path;
    };
    struct URL u;

    // 使用strtok函数分割URL字符串
    int url_len = strlen(url); // 计算字符串的长度
    char *new_url = malloc(url_len + 1); // 分配字符串空间用于分割，多出一个字节用来存储'\0'
    strcpy(new_url, url); // 复制url到new_url
    
    char *token = strtok(new_url, ":/"); // 按照:和/分割
    u.protocol = token; // 第一个分割出来的是协议
    token = strtok(NULL, ":/"); // 再次分割
    if (strcmp(token, "") == 0) { // 如果分割出来的是空字符串，说明有//
        token = strtok(NULL, ":/"); // 再次分割
        u.host = token; // 这次分割出来的是主机名
    } else {
        u.host = token; // 否则直接是主机名
    }
    token = strtok(NULL, ":/"); // 再次分割
    if (token == NULL) { // 如果没有分割出来，说明没有指定端口号
        if (strcmp(u.protocol, "http") == 0) { // 如果协议是http，默认端口为80
            u.port = 80;
        } else if (strcmp(u.protocol, "https") == 0) { // 如果协议是https，默认端口为443
            u.port = 443;
        } else { // 否则抛出异常
            perror("Invalid protocol");
            exit(1);
        }
        u.path = "/"; // 路径默认为/
    } else {
        u.port = atoi(token); // 否则把分割出来的字符串转换成整数作为端口号
        // u.port = 80;
        token = strtok(NULL, ":/"); // 再次分割
        if (token == NULL) { // 如果没有分割出来，说明没有指定路径
            u.path = "/"; // 路径默认为/
        } else {
            u.path = token; // 否则把分割出来的字符串作为路径
        }
    }
    // 创建一个Socket对象，连接到指定的主机和端口
    int sock = socket(AF_INET, SOCK_STREAM, 0); // 创建一个IPv4的TCP套接字
    if (sock < 0) { // 如果创建失败，抛出异常
        perror("socket error");
        exit(1);
    }

    struct sockaddr_in server_addr; // 定义一个服务器地址结构体
    memset(&server_addr, 0, sizeof(server_addr)); // 初始化为0
    server_addr.sin_family = AF_INET; // 地址族为IPv4
    server_addr.sin_port = htons(u.port); // 端口号，注意转换成网络字节序

    struct hostent *host = gethostbyname(u.host); // 根据主机名获取IP地址
    if (host == NULL) { // 如果获取失败，抛出异常
        perror("gethostbyname error");
        exit(1);
    }

    memcpy(&server_addr.sin_addr.s_addr, host->h_addr_list[0], host->h_length); // 把IP地址复制到服务器地址结构体中

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) { // 连接到服务器，如果失败，抛出异常
        perror("connect error");
        exit(1);
        }

    // 创建一个字符串缓冲区，用于拼接HTTP请求报文
    char request[1024];
    memset(request, 0, sizeof(request)); // 初始化为0


    int path_len = strlen(u.path); // len = 21
    char *new_path = malloc(path_len + 1); // 分配22个字节的空间，多出一个字节用来存储'\0'
    strcpy(new_path, u.path); // 复制url到new_url
    // 根据请求方法的不同，拼接不同的请求行
    if (strcmp(method, HTTP_GET) == 0) {
        // 如果是GET方法，需要把参数拼接到URL后面
        if (params != NULL && strlen(params) > 0) {
            strcat(new_path, "?");
            strcat(new_path, params);
        }
        sprintf(request, "%s %s %s%s", HTTP_GET, new_path, HTTP_VERSION, CRLF);
    } else if (strcmp(method, HTTP_POST) == 0) {
        // 如果是POST方法，需要把参数的长度写到请求头中
        sprintf(request, "%s %s %s%s", HTTP_POST, u.path, HTTP_VERSION, CRLF);
        sprintf(request + strlen(request), "Content-Length: %ld%s", strlen(params), CRLF);
sprintf(request + strlen(request), "Content-Type: %s%s", "application/x-www-form-urlencoded", CRLF);
    } else {
        // 如果既不是GET也不是POST，抛出异常
        perror("Invalid HTTP method");
        exit(1);
    }

    // 拼接通用的请求头，如主机名、连接方式等
    sprintf(request + strlen(request), "Host: %s%s", u.host, CRLF);
    sprintf(request + strlen(request), "Connection: close%s", CRLF); // 使用短连接，请求结束后关闭连接

    // 拼接一个空行，表示请求头结束
    sprintf(request + strlen(request), "%s", CRLF);

    // 如果是POST方法，还需要拼接请求体，即参数
    if (strcmp(method, HTTP_POST) == 0) {
        sprintf(request + strlen(request), "%s", params,CRLF);
    }

    // 把字符串缓冲区的内容发送到输出流中
    write(sock, request, strlen(request));

    //定义缓冲区长度
    int buffer_len= 4096;

    // 创建一个字符串缓冲区，用于存储响应报文
    char *response = malloc(buffer_len + 1); // 分配22个字节的空间，多出一个字节用来存储'\0'

    // 初始化为0
    memset(response, 0, sizeof(response));

    // 定义一个整数，用于记录读取的字节数
    int bytes = 0;

    // 定义一个整数，用于记录响应报文的主体长度
    int content_length = 0;

    // 定义一个布尔值，用于标记是否已经读取到响应报文的头部
    bool header_read = false;

    // 循环读取输入流中的数据，直到读到末尾或者读完主体
    while ((bytes = read(sock, response + strlen(response), buffer_len - strlen(response))) > 0) {
        // 如果读取成功，就继续拼接到字符串缓冲区中
        if (bytes < 0) { // 如果读取失败，抛出异常
            perror("read error");
            exit(1);
        } else if (bytes == 0) { // 如果读取到末尾，跳出循环
            break;
        }
        char *separator;
        // 如果还没有读取到响应报文的头部，就尝试解析头部
        if (!header_read) {
            // 查找头部和主体之间的分隔符\r\n\r\n
            separator = strstr(response, "\r\n\r\n");
            if (separator != NULL) { // 如果找到了分隔符，就表示已经读取到了头部
                header_read = true; // 标记已经读取到了头部

                // 在分隔符之前查找Content-Length字段，获取主体的长度
                char *content_length_field = strcasestr(response, "Content-Length:");
                if (content_length_field != NULL && content_length_field < separator) { // 如果找到了Content-Length字段，并且在分隔符之前，就表示有主体长度信息
                    // 跳过字段名和空格，获取字段值
                    content_length_field += strlen("Content-Length:");
                    while (*content_length_field == ' ') {
                        content_length_field++;
                    }

                    // 将字段值转换为整数，并赋值给content_length变量
                    content_length = atoi(content_length_field);
                }
            }
        }

        // 如果已经读取到了响应报文的头部，并且有主体长度信息，就判断是否已经读完了主体
        if (header_read && content_length > 0) {
            // 计算已经读取的主体长度
            int body_read = strlen(response) - (separator - response) - 4;

            // 如果已经读取的主体长度等于或大于主体长度，就表示已经读完了主体，跳出循环
            if (body_read >= content_length) {
                break;
            }
        }
    }

    // 关闭套接字
    close(sock);
    free(new_url); //释放用于分割字符串的内存    
    free(new_path); //释放用于分割字符串的内存    

    // 返回响应报文
    return response;
}

// 定义一个主函数，用于测试
int main(int argc, char *argv[]) {
    // 检查命令行参数是否正确
    if (argc < 3) {
        printf("Usage: %s url params\n", argv[0]);
        return 1;
    }
    // 获取请求方法
    char * method = argv[1];

    // 从命令行参数中获取URL字符串
    char *url = argv[2];
    // 从命令行参数中获取参数字符串
    char *params = argv[3];
    // 定义一个URL字符串
    // char *url = "http://127.0.0.1/";
    // // 定义一个参数字符串
    // char *params = "name=bing&message=hello";
    // 调用函数发送HTTP POST请求，并打印响应结果
    // GET方法把下面的HTTP_POST改成HTTP_GET
    // POST请求的Conten-Type默认是application/x-www-form-urlencoded
    char *response = sendHttpRequest(method, url, params);
    printf("%s\n", response);

    free(response);
    return 0;
}