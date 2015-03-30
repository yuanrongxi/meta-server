#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <process.h>
#include <io.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <mswsock.h>
#include <direct.h>
#include <time.h>

#define error_no	WSAGetLastError()
#pragma comment(lib, "ws2_32.lib")

#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <signal.h>
#include <string.h>
#include <inttypes.h>
#include <pthread.h>

#define error_no	errno
#endif

#include "cell_codec.h"
#include "cell_msg.h"
#include "meta_rpc.h"

#define TEST_METASERVER "192.168.113.80"
#define TEST_PORT 3200

/*测试通过自定义路径获得文件访问的meta信息*/
static void test_rpc(const char* key)
{
	char* url = NULL;
	int ret;
	ret = rpc_get_meta_by_path(TEST_METASERVER, TEST_PORT, key, &url, 1000);
	//return ; /*功能测试时请注释return!!*/

	if(ret == RPC_SUCC){
		printf("get meta by path, path = %s, url = %s\n", key, url);
		free(url); /*TODO:一定得释放内存*/
	}
	else
		printf("rpc_get_meta failed! err = %d\n", ret);
}

/*用版本号访问文件历史版本元数据例子*/
static void test_version_rpc(const char* key, int version)
{
	char* url = NULL;
	int ret;
	ret = rpc_get_meta_by_version(TEST_METASERVER, TEST_PORT, key, version, 1000, &url);
	if(ret == RPC_SUCC){
		printf("get meta by version success, path = %s.%d, url = %s\n", key, version, url);
		free(url); /*TODO:一定得释放内存*/
	}
	else
		printf("rpc_get_meta failed! err = %d\n", ret);
}

/*通过权限认证获取文件元数据，例如：密码提取、owner访问*/
static void test_auth_rpc(const char* key, const char* user, const char* pwd)
{
	auth_out_info_t res;
	int ret;
	ret = rpc_get_meta_by_auth(TEST_METASERVER, TEST_PORT, key, 2, user, pwd, "", &res, 1000);
	if(ret == RPC_SUCC){
		printf("rpc_get_meta_by_auth success! key = %s, url = %s\n", key, res.url);
	}
	else{
		printf("rpc_get_meta_by_auth failed! key = %s, err = %d\n", key, ret);
	}
}

/*更新文件元数据例子*/
static void test_update_rpc(const char* key)
{
	int ret = rpc_update_meta(TEST_METASERVER, TEST_PORT, key, "test_pool", 0, "passwd", "", 0, 1000);
	//return;/*功能测试时请注释return!!*/

	if(ret == RPC_SUCC)
		printf("update meta key = %s success!\n", key);
	else
		printf("update meta failed, err = %d\n", ret);
}

/*文件替换例子*/
static void test_replace_file(const char* key, int version, const char* url)
{
	int ret = rpc_replace_meta(TEST_METASERVER, TEST_PORT, key, "test_pool", url, "zerok", version, 1000);
	if(ret == RPC_SUCC)
		printf("replace meta key = %s success!\n", key);
	else
		printf("replace meta failed, err = %d\n", ret);
}

/*测试添加一个文件的meta信息*/
static void test_add_rpc(const char* key)
{
	meta_info_t info;
	int ret;

	info.opened = 1;
	strcpy(info.path, key);
	strcpy(info.url, "/fsdf/data/N0/2.jpg");
	strcpy(info.uploader, "zerok");
	strcpy(info.pwd, "passwd");
	info.crpyt_key_size = strlen("abcdefj") + 1;
	strcpy(info.crpyt_key, "abcdefj");

	ret = rpc_add_meta(TEST_METASERVER, TEST_PORT, &info, "test_pool", 1000);
	//return ;/*功能测试时请注释return!!*/

	if(ret == RPC_SUCC)
		printf("add meta key = %s, success\n", info.path);
	else
		printf("add meta failed, err = %d\n", ret);
}
/*测试删除一个文件的meta信息*/
static void test_del_rpc(const char* key)
{
	int ret = rpc_delete_meta(TEST_METASERVER, TEST_PORT, key, "test_pool", 1000);
	//return ;/*功能测试时请注释return!!*/

	if(ret == RPC_SUCC)
		printf("delete meta key = %s success!\n", key);
	else
		printf("delete meta failed, err = %d\n", ret);
}

/*测试添加一个生命周期文件*/
static void test_add_life(const char* url)
{
	int ret = rpc_add_life_cycle(TEST_METASERVER, TEST_PORT, url, 30, 0, 1000);
	if(ret == RPC_SUCC)
		printf("add life sucess! url = %s\n", url);
	else
		printf("add life failed!, err = %d\n", ret);
}

/*测试文件操作日志*/
static void test_write_log(const char* url)
{
	int ret = rpc_add_file_log(TEST_METASERVER, TEST_PORT, "zerok", "test_pool", url, 0, "192.168.112.80", 1000);
	if(ret == RPC_SUCC)
		printf("write log sucess! url = %s\n", url);
	else
		printf("write log failed!, err = %d\n", ret);
}

/*解禁用户上传文件*/
static void test_enable_user()
{
	int ret;
	if((ret = rpc_enable_user(TEST_METASERVER, TEST_PORT, "zerok", 1000)) == 0)
		printf("enable %s OK!\n", "zerok");
	else
		printf("rpc_enable_user failed!, err = %d\n", ret);
}
/*禁止用户文件上传*/
static void test_disable_user()
{
	int ret;
	if((ret = rpc_disable_user(TEST_METASERVER, TEST_PORT, "zerok", 1000)) == 0)
		printf("disable %s OK!\n", "zerok");
	else
		printf("rpc_disable_user failed!, err = %d\n", ret);
}

static void test_report_upload()
{
	int ret = rpc_report_upload_file(TEST_METASERVER, TEST_PORT, "zerok", 3000, 1000);
	if(ret == 0)
		printf("report_upload_file succ, user=%s\n", "zerok");
	else
		printf("report_upload_file failed, err= %d\n", ret);
}

static void test_check_upload()
{
	int ret = rpc_is_upload_file(TEST_METASERVER, TEST_PORT, "zerok", "html", 2000, 1000);
	if(ret == 0)
		printf("%s can upload file!\n", "zerok");
	else
		printf("%s can not upload file!, err = %d\n", "zerok", ret);
}

#define KEY_SEED "/home/yuanrx/jpg-%d"

static int server_id = 0;

/*用于压力测试*/
#ifndef WIN32

#define THREAD_N	4
static uint32_t	op_arr[4] = {80, 90, 96, 99};

static int get_op_type()
{
	int i, v;
	v = rand() % 100;
	for(i = 0; i < 4; i ++){
		if(v <= op_arr[i])
			return i;
	}

	return 3;
}

static int key_seed = 1;

static int get_count = 0;
static int add_count = 0;
static int update_count = 0;
static int del_count = 0;
static int count = 0;

static void* pt_func(void* arg)
{
	int i = 0;
	char key[256];
	struct timeval b, e;
	int scale = (server_id - 1) * 100000;

	gettimeofday(&b, NULL);
	while(i < 100000){
		count ++;

		switch(get_op_type()){
		case 0:
			{
				sprintf(key, KEY_SEED, rand() % key_seed + scale);
				test_rpc(key);
				get_count ++;
			}
			break;

		case 1:
			{
				sprintf(key, KEY_SEED, key_seed + scale);
				__sync_add_and_fetch(&key_seed, 1);
				test_add_rpc(key);

				add_count ++;
			}
			break;

		case 2:
			{
				sprintf(key, KEY_SEED, rand() % key_seed + scale);
				test_update_rpc(key);
				update_count ++;
			}
			break;

		case 3:
			{
				sprintf(key, KEY_SEED, rand() % key_seed + scale);
				test_rpc(key);
				del_count ++;
			}
			break;
		}
		i ++;
	}

	gettimeofday(&e, NULL);
	printf("muti_tread get delay = %d ms \n", (e.tv_sec - b.tv_sec) * 1000 + (e.tv_usec - b.tv_usec)/1000);
}

void test_muti_thread()
{
	int i;
	pthread_t id[10];

	for(i = 0; i < THREAD_N; i++){
		pthread_create(&id[i], NULL, pt_func, NULL);
	}

	for(i = 0; i < THREAD_N; i++)
		pthread_join(id[i], NULL);

	printf("finish muti thread test,\n\tmeta count = %d\n\tget count = %d\n\tadd_count = %d, update_count = %d\n\tdel_count = %d!!\n", count,
		get_count, add_count, update_count, del_count);
}

#endif


int main(int argc, char* argv[])
{
	int i = 0;
	char key[256];

	if(argc == 2){
		server_id = atoi(argv[1]);
		printf("server_id = %d\n", server_id);
	}

	srand(time(NULL));

	rpc_init();

	sprintf(key, KEY_SEED, 100000);
	//test_replace_file(key, 2, "/fsdf/data/N09/4.jpg");
	//test_update_rpc(key);
	/*test_add_rpc(key);*/
	//test_rpc("/home/yuanrx/102.jpg");
	//test_version_rpc(key, 1);
	//test_auth_rpc("/home/yuanrx/102.jpg.1", "zerok", "abcd");
	//test_add_life(key);

	//test_write_log(key);

	//test_enable_user();
	//test_disable_user();
	//test_report_upload();
	test_check_upload();

#ifndef WIN32
	//test_muti_thread();
#endif
	getchar();
	rpc_destroy();
}



