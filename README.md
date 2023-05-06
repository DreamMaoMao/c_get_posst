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
gcc -o gp gp.c  -I/home/usr/openssl/include/ -lssl -lcrypto  -ldl -L/home/usr/openssl/li

### get
gp get http://127.0.01/hello "name=test&message=hello" h

or

gp get http://127.0.01/hello/?name=test&message=hello h

or 

gp get http://127.0.01/hello/?name=test&message=hello "" h

### post
gp post http://127.0.01/hello "name=test&message=hello" h

# ssl支持版本支持 需要编译安装openssl库拷贝到板子，运行和编译都要用到(旧设备和没有/dev/random也可用)
```
由于我的嵌入式板子内核没有把/dev/random放进去,我增加了自定义随机数
到下一版本 就强制用这个/dev/random真随机数了

我代码上改成了自己生成的随机数，1.1.0版本ssl的公钥证书已经不可用
1.1.1.d又自定义不了随机数，所以我卡了1.1.1c版本的

wget https://www.openssl.org/source/old/1.1.1/openssl-1.1.1c.tar.gz

tar xvf openssl-1.1.1c.tar.gz

cd openssl-1.1.1c

mkdir /home/user/openssl

./Configure linux-generic32 no-asm shared no-async --prefix=/home/user/openssl CROSS_COMPILE=arm-linux- CC=gcc -D_XOPEN_SOURCE=700

make -j4

sudo su

make install

编译
arm-linux-gcc -o getpost get_post_with_ssl.c  -I/home/user/openssl/include/ -lssl -lcrypto  -ldl -L/home/user/openssl/lib


把/home/user/openssl/lib 下的所有文件拷贝到嵌入式系统的/lib下
```
