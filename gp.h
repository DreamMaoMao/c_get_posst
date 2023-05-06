#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/aes.h>
#include <openssl/ec.h>

// 定义一些常量
#define GP_HTTP_GET "GET"
#define GP_HTTP_POST "POST"
#define GP_HTTP_VERSION "HTTP/1.1"
#define GP_CRLF "\r\n" // 回车换行符
#define GP_RESPONSE_BUFFER_LEN 40960 //返回体接受的缓冲区长度，太小接受不了过长的返回


struct gp_http_response {  //定义响应体
    char *body;
    char *headers;
};

typedef struct gp_http_response GP_HTTP_RESPONSE;
