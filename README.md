# c_get_posst
# c语言实现get post http客户端（适用于arm x86）
用的库比较底层,各个平台应该都通用

## 目前测试过可用平台

> x86_64 debian11

> arm(s3c2440) linux2.26

## 目前测试过可用编译器

> gcc10(x86)

> gcc4(x86)

> gcc8(arm)

> gcc3(arm)

## 用法
直接gcc -o c_get_post.c -o getPost

//get
getPost GET http://127.0.01/hello name=test&message=hello

//post
getPost GET http://127.0.01/hello name=test&message=hello
