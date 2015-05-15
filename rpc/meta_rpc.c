#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
/*WINDOWS下做测试用*/
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

#define LOCK(q)
#define UNLOCK(q)

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
#define closesocket	close	

#define LOCK(q) while(__sync_lock_test_and_set(q, 1)){}
#define UNLOCK(q) __sync_lock_release((q))
#endif

#include "cell_codec.h"
#include "meta_rpc.h"

static uint32_t sid_seed = 0;

typedef struct rpc_socket_t
{
	int			fd;
	int			busy;
	uint32_t	ipaddr;
}rpc_socket_t;

/*socket pool是为了防止过多的TIME_WAIT,也为了复用socket,减少connect的时间和系统消耗*/
typedef struct rpc_socket_pool_t
{
	rpc_socket_t**	arr;
	size_t			arr_size;
	int				lock;
}rpc_socket_pool_t;

#define MAX_SIZE		128		/*池中最大socket缓冲个数*/
#define MAX_PRE_META	8		/*每个metaserver最多保持8个socket在pool中*/

static rpc_socket_pool_t socket_pool;

static void init_socket_pool()
{
	socket_pool.arr = calloc(1, MAX_SIZE * sizeof(rpc_socket_t*));
	socket_pool.arr_size = MAX_SIZE;
	socket_pool.lock = 0;

	sid_seed = rand() % 65536;
}

static void destroy_socket_pool()
{
	if(socket_pool.arr != NULL){
		size_t i;
		for(i = 0; i < socket_pool.arr_size; i ++){
			if(socket_pool.arr[i] != NULL){
				if(socket_pool.arr[i]->fd != -1){
					closesocket(socket_pool.arr[i]->fd);
					socket_pool.arr[i]->fd = -1;
				}

				free(socket_pool.arr[i]);
			}
		}

		free(socket_pool.arr);
		socket_pool.arr = NULL;
	}
}

static int check_socket(int fd)
{
	fd_set read_set, ex_set;
	struct timeval tv;

	tv.tv_sec = 0;
	tv.tv_usec = 100; /*select等待100微秒，用于检测fd是否正常*/

	FD_ZERO(&read_set);
	FD_SET(fd, &read_set);

	FD_ZERO(&ex_set);
	FD_SET(fd, &ex_set);

	if(select(fd + 1, &read_set, NULL, &ex_set, &tv) != 0)
		return -1;

	return 0;
}

static int get_socket(uint32_t ip)
{
	int ret = -1;
	size_t i;

	assert(socket_pool.arr != NULL);

	LOCK(&(socket_pool.lock));

	for(i = 0; i < socket_pool.arr_size; i ++){
		if(socket_pool.arr[i] != NULL && socket_pool.arr[i]->fd != -1 
			&& socket_pool.arr[i]->busy == 0 && socket_pool.arr[i]->ipaddr == ip){
				socket_pool.arr[i]->busy = 1;
				ret = socket_pool.arr[i]->fd;
				break;
		}
	}

	UNLOCK(&(socket_pool.lock));

	/*因为socket放在pool可能很长时间，这段时间有可能socket异常,在使用前必须进行检查，防止rpc失败*/
	if(ret != -1 && check_socket(ret) == -1){
		LOCK(&(socket_pool.lock));
		/*关闭socket并且释放占用的pool槽位*/
		closesocket(socket_pool.arr[i]->fd);
		free(socket_pool.arr[i]);
		socket_pool.arr[i] = NULL;

		UNLOCK(&(socket_pool.lock));

		ret = -1;
	}

	return ret;
}

static void release_socket(int fd, uint32_t ip)
{
	int count = 0, pos = -1;
	int release = 0;
	rpc_socket_t* s;
	size_t i;

	assert(socket_pool.arr != NULL && fd != -1);
	
	LOCK(&(socket_pool.lock));

	for(i = 0; i < socket_pool.arr_size; i++){
		if(socket_pool.arr[i] == NULL){ /*前面有空位*/
			if(pos == -1)
				pos = i;
			continue;
		}

		if(socket_pool.arr[i]->ipaddr == ip){
			count ++;
			if(socket_pool.arr[i]->fd == fd){
				socket_pool.arr[i]->busy = 0;
				UNLOCK(&(socket_pool.lock));

				return ;
			}
		}
	}

	if(count < MAX_PRE_META && pos >= 0){
		s = calloc(1, sizeof(rpc_socket_t));
		s->fd = fd;
		s->busy = 0;
		s->ipaddr = ip;
		socket_pool.arr[pos] = s;
	}
	else
		closesocket(fd);

	UNLOCK(&(socket_pool.lock));
}

void rpc_init()
{

#ifdef WIN32
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;

	wVersionRequested = MAKEWORD(1, 1);
	err = WSAStartup(wVersionRequested, &wsaData);
	srand((uint32_t)time(NULL));
#else
	srand((uint32_t)time(NULL));
#endif

	init_socket_pool();

}

void rpc_destroy()
{
	destroy_socket_pool();

#ifdef WIN32
	WSACleanup();
#endif
}

/*创建一个socket,并且连接metaserver*/
static int connect_metaserver(const char* ip, uint16_t port)
{
	int fd;
	struct sockaddr_in addr;
	int buf_size = 64 * 1024;

	fd = get_socket(inet_addr(ip));
	if(fd == -1){
		fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if(fd < 0){
			printf("create socket failed\n");
			return -1;
		}

		setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (void *)&buf_size, sizeof(int32_t));
		setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (void *)&buf_size, sizeof(int32_t));

		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = inet_addr(ip);
		addr.sin_port = htons(port);

		if(connect(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) != 0){
			printf("connect server failed, err = %d\n", error_no);
			closesocket(fd);

			return -1;
		}
	}

	return fd;
}

/*send packet to metaserver*/
static void send_packet(int fd, uint16_t id, void* body)
{
	bin_stream_t* strm;
	uint8_t* head;
	uint32_t len = 0;

	strm = (bin_stream_t *)malloc(sizeof(bin_stream_t));
	if(bin_stream_init(strm) == -1){
		free(strm);
		return ;
	}
	bin_stream_reset(strm, 1);

	head = strm->wptr;
	mach_uint32_write(strm, len);
	encode_msg(strm, id, body);
	/*设置报文长度*/
	len = strm->wptr - head - sizeof(uint32_t);
	mach_put_4(head, len);

	send(fd, (char* )(strm->rptr), strm->used - strm->rsize, 0);

	bin_stream_reset(strm, 0);
	bin_stream_destroy(strm);

	free(strm);
}

static bin_stream_t* recv_packet(int fd, int timeout)
{
	bin_stream_t* strm;
	struct sockaddr_in addr;
	int rc, addr_size;
	fd_set	read_set;
	int max_fd;
	struct timeval tv;
	uint32_t len;

	strm = (bin_stream_t *)malloc(sizeof(bin_stream_t));
	if(bin_stream_init(strm) == -1){
		free(strm);
		return NULL;
	}

	bin_stream_reset(strm, 1);
	bin_stream_resize(strm, strm->size * 4); /*4k*/

loop:
	FD_ZERO(&read_set);
	FD_SET(fd, &read_set);
	max_fd = fd + 1;

	tv.tv_sec = timeout / 1000;
	tv.tv_usec = (timeout % 1000) * 1000;

	if(select(max_fd, &read_set, NULL, NULL, &tv) > 0 &&  FD_ISSET(fd, &read_set)){
		addr_size = sizeof(struct sockaddr_in);
		rc = recvfrom(fd, (char *)strm->wptr, strm->size - strm->used, 0, (struct sockaddr *)&addr, &addr_size);
		if(rc <= 0){
			bin_stream_destroy(strm);
			free(strm);
			return NULL;
		}
		else{
			strm->used += rc;
			strm->wptr += rc;

			len = mach_get_4(strm->rptr);
			if(len <= strm->used - strm->rsize - 4) /*收齐了响应包，返回继续处理,响应只有一个逻辑报文*/
				return strm;
			else{ /*响应包没收完整，继续收直到收完整*/
				if(strm->size <= strm->used) /*扩大binstream的大小，一遍存储更大的报文*/
					bin_stream_resize(strm, strm->size * 2);

				goto loop;
			}
		}
	}
	else{ /*超时了！！！*/
		bin_stream_destroy(strm);
		free(strm);
		return NULL;
	}

	return NULL;
}

static int rpc_get_meta(const char* ip, int port, const char* path, const char* pool, int timeout, meta_info_t* info)
{
	int fd;
	int ret;

	bin_stream_t* strm;
	get_meta_t body;

	assert(info != NULL && ip != NULL);
	if(strlen(pool) >= POOL_NAME_SIZE || strlen(path) >= MAX_FILE_NAME)
		return RPC_PARAM_ERROR;

	fd = connect_metaserver(ip, port);
	if(fd < 0){
		printf("rpc: connect metaserver(%s:%d) failed!\n", ip, port);
		return RPC_SOCKET_ERROR;
	}

	body.sid =  sid_seed ++;
	strncpy(body.path, path, MAX_FILE_NAME);
	strncpy(body.pool, pool, POOL_NAME_SIZE);

	send_packet(fd, GET_META, &body);

	strm = recv_packet(fd, timeout <= 0 ? 3000 : timeout);
	if(strm == NULL)
		ret = RPC_TIMEOUT;
	else{
		uint16_t msg_id;
		uint32_t len;

		mach_uint32_read(strm, &len);
		mach_uint16_read(strm, &msg_id);
		if(msg_id == GET_META_ACK && get_meta_ack_decode(strm, info) != -1 && body.sid == info->sid){
			if(info->result == 0)
				ret = RPC_SUCC;
			else
				ret = RPC_DB_FAILED;
		}
		else
			ret = RPC_PACKET_ERROR;

		bin_stream_destroy(strm);
		free(strm);
	}

	release_socket(fd, inet_addr(ip));

	return ret;
}

int rpc_get_meta_by_version(const char* ip, int port, const char* path, int version, int timeout, char** url)
{
	int ret;
	meta_info_t info;
	char key[MAX_FILE_NAME];
	char* fdfs_url;

	sprintf(key, "%s.%d", path, version);
	ret = rpc_get_meta(ip, port, key, "test_pool", timeout, &info);
	if(ret == RPC_SUCC){
		if(info.opened == 0) /*禁止外部访问*/
			return RPC_NOT_OWNER;

		if(strlen(info.pwd) > 0) /*提取码访问*/
			return RPC_PWD_ERROR;

		fdfs_url = calloc(1, strlen(info.url) + 1);
		strcpy(fdfs_url, info.url);
		*url = fdfs_url;
	}
	else
		*url = NULL;

	return ret;
}

int rpc_get_meta_by_auth(const char* ip, int port, const char* path, int auth_level, const char* user, 
							const char* pwd, const char* cookie, auth_out_info_t* res, int timeout)
{
	int ret;
	meta_info_t info;

	assert(res != NULL && ip != NULL);

	ret = rpc_get_meta(ip, port, path, "test_pool", timeout, &info);
	if(ret == RPC_SUCC){
		if(info.opened == 0 && (user == NULL || strcmp(user, info.uploader) != 0)) /*禁止外部访问*/
			return RPC_NOT_OWNER;

		if(strlen(info.pwd) > 0 && (pwd == NULL || strcmp(pwd, info.pwd) != 0)) /*提取码访问*/
			return RPC_PWD_ERROR;

		strncpy(res->url, info.url, MAX_FILE_NAME);
		memcpy(res->encrypt_code, info.crpyt_key, info.crpyt_key_size > CRPYT_CODE_SIZE ? CRPYT_CODE_SIZE : info.crpyt_key_size);
	}

	return ret;
}

int rpc_get_meta_by_path(const char* ip, int port, const char* path, char** url, int timeout)
{
	int ret;
	meta_info_t info;
	char* fdfs_url;

	assert(ip != NULL && path != NULL);

	ret = rpc_get_meta(ip, port, path, "test_pool", timeout, &info);
	if(ret == RPC_SUCC){
		if(info.opened == 0) /*禁止外部访问*/
			return RPC_NOT_OWNER;

		if(strlen(info.pwd) > 0) /*提取码访问*/
			return RPC_PWD_ERROR;

		fdfs_url = calloc(1, strlen(info.url) + 1);
		strcpy(fdfs_url, info.url);
		*url = fdfs_url;
	}
	else
		*url = NULL;

	return ret;
}

/*处理通用的ack消息*/
static int process_ack(uint32_t msg_id, uint32_t sid, int fd, int timeout, char** err)
{
	bin_stream_t* strm;
	int ret = RPC_TIMEOUT;
	char* info;
	*err = NULL;

	strm = recv_packet(fd, timeout <= 0 ? 3000 : timeout);
	if(strm != NULL){
		uint16_t msg_id;
		uint32_t len;
		add_meta_ack_t ack;

		mach_uint32_read(strm, &len);
		mach_uint16_read(strm, &msg_id);
		if(msg_id == msg_id && ack_decode(strm, &ack) != -1 && ack.sid == sid){
			if(ack.result > 7) 
				ret = ack.result;
			else
				ret = (ack.result == 0) ? RPC_SUCC : RPC_DB_FAILED;
		}
		else
			ret = RPC_PACKET_ERROR;

		info = calloc(1, strlen(ack.err) + 1);
		strcpy(info, ack.err);
		*err = info;

		bin_stream_destroy(strm);
		free(strm);
	}

	return ret;
}

int	rpc_add_meta(const char* ip, int port, meta_info_t* info, const char* pool, int timeout, char** err)
{
	int fd;
	int ret;
	add_meta_t body;

	assert(info != NULL && ip != NULL);

	if(strlen(info->path) >= MAX_FILE_NAME || strlen(info->url) >= MAX_FILE_NAME || strlen(info->uploader) >= USERNAME_SIZE
		|| info->crpyt_key_size > CRPYT_CODE_SIZE || strlen(pool) >= POOL_NAME_SIZE || strlen(info->pwd) >= USERNAME_SIZE)
		return RPC_PARAM_ERROR;

	fd = connect_metaserver(ip, port);
	if(fd < 0){
		printf("rpc: connect metaserver(%s:%d) failed!\n", ip, port);
		return RPC_SOCKET_ERROR;
	}

	body.sid = sid_seed ++;
	body.opened = info->opened;
	body.crpyt_key_size = info->crpyt_key_size;
	memcpy(body.crpyt_key, info->crpyt_key, info->crpyt_key_size);
	strncpy(body.path, info->path, MAX_FILE_NAME);
	strncpy(body.url, info->url, MAX_FILE_NAME);
	strncpy(body.uploader, info->uploader, USERNAME_SIZE);
	strncpy(body.pwd, info->pwd, USERNAME_SIZE);
	strncpy(body.pool, pool, POOL_NAME_SIZE);

	send_packet(fd, ADD_META, &body);

	ret = process_ack(ADD_META_ACK, body.sid, fd, timeout, err);

	release_socket(fd, inet_addr(ip));

	return ret;
}

int rpc_update_meta(const char* ip, int port, const char* path, const char* pool, int opened, const char* pwd, 
					unsigned char* crpyt_key, size_t crpyt_size, int timeout, char** err)
{
	int fd;
	int ret;
	update_meta_t body;

	assert(ip != NULL);

	if(strlen(path) >= MAX_FILE_NAME || strlen(pool) >= POOL_NAME_SIZE )
		return RPC_PARAM_ERROR;

	memset(&body, 0, sizeof(update_meta_t));

	if(opened >= 0){
		body.op |= SET_OPEN;
		body.opened = opened;
	}
	if(pwd != NULL){
		body.op |= SET_PWD;
		strncpy(body.pwd, pwd, USERNAME_SIZE);
	}
	if(crpyt_size > 0 && crpyt_key != NULL){
		body.op |= SET_CRPYT;
		body.crpyt_key_size = crpyt_size;
		memcpy(body.crpyt_key, crpyt_key, crpyt_size);
	}
	/*传入的参数根本没有设置update meta的数据*/
	if(body.op == 0)
		return RPC_PARAM_ERROR;

	fd = connect_metaserver(ip, port);
	if(fd < 0){
		printf("rpc: connect metaserver(%s:%d) failed!\n", ip, port);
		return RPC_SOCKET_ERROR;
	}

	strncpy(body.pool, pool, POOL_NAME_SIZE);
	strncpy(body.path, path, MAX_FILE_NAME);
	body.sid = sid_seed ++;

	send_packet(fd, UPDATE_META, &body);

	ret = process_ack(UPDATE_META_ACK, body.sid, fd, timeout, err);

	release_socket(fd, inet_addr(ip));

	return ret;
}

int rpc_replace_meta(const char* ip, int port, const char* path, const char* pool, const char* url, 
			const char* user, int version, int timeout, char** err)
{
	int fd;
	int ret;
	replace_meta_ver_t body;

	assert(ip != NULL && path != NULL && pool != NULL && url != NULL && user != NULL);
	if(strlen(path) >= MAX_FILE_NAME || strlen(pool) >= POOL_NAME_SIZE || strlen(user) >= USERNAME_SIZE || strlen(url) > MAX_FILE_NAME)
		return RPC_PARAM_ERROR;

	fd = connect_metaserver(ip, port);
	if(fd < 0){
		printf("rpc: connect metaserver(%s:%d) failed!\n", ip, port);
		return RPC_SOCKET_ERROR;
	}

	body.sid = sid_seed ++;
	body.version = version;
	strncpy(body.path, path, MAX_FILE_NAME);
	strncpy(body.url, url, MAX_FILE_NAME);
	strncpy(body.pool, pool, POOL_NAME_SIZE);
	strncpy(body.user, user, USERNAME_SIZE);

	send_packet(fd, REPLACE_META_VER, &body);

	ret = process_ack(REPLACE_META_VER_ACK, body.sid, fd, timeout, err);

	release_socket(fd, inet_addr(ip));

	return ret;
}

int rpc_delete_meta(const char* ip, int port, const char* path, const char* pool, int timeout, char** err)
{
	int fd;
	int ret;
	del_meta_t body;

	assert(path != NULL && pool != NULL && ip != NULL);
	if(strlen(path) >= MAX_FILE_NAME || strlen(pool) >= POOL_NAME_SIZE)
		return RPC_PARAM_ERROR;

	fd = connect_metaserver(ip, port);
	if(fd < 0){
		printf("rpc: connect metaserver(%s:%d) failed!\n", ip, port);
		return RPC_SOCKET_ERROR;
	}

	body.sid = sid_seed ++;
	strncpy(body.pool, pool, POOL_NAME_SIZE);
	strncpy(body.path, path, MAX_FILE_NAME);
	
	send_packet(fd, DELETE_META, &body);

	ret = process_ack(DELETE_META_ACK, body.sid, fd, timeout, err);

	release_socket(fd, inet_addr(ip));
	return ret;
}

int rpc_add_life_cycle(const char* ip, int port, const char* url, int day, int cold, int timeout, char** err)
{
	int fd;
	int ret;
	add_life_cycle_t body;

	assert(url != NULL);

	if(strlen(url) >= MAX_FILE_NAME)
		return RPC_PARAM_ERROR;

	fd = connect_metaserver(ip, port);
	if(fd < 0){
		printf("rpc: connect metaserver(%s:%d) failed!\n", ip, port);
		return RPC_SOCKET_ERROR;
	}

	body.sid = sid_seed ++;
	strncpy(body.url, url, MAX_FILE_NAME);
	body.days = day;
	body.cold= cold;

	send_packet(fd, ADD_LIFE_CYCLE, &body);
	ret = process_ack(ADD_LIFE_CYCLE_ACK, body.sid, fd, timeout, err);

	release_socket(fd, inet_addr(ip));

	return ret;
}

int rpc_add_file_log(const char* ip, int port, const char* user, const char* pool, const char* url, int type,
		const char* op_ip, int timeout, char** err)
{
	int fd;
	int ret;
	add_log_t body;

	assert(url != NULL && pool != NULL && user != NULL && ip != NULL);

	if(strlen(user) >= USERNAME_SIZE || strlen(url) >= MAX_FILE_NAME || strlen(ip) >= USERNAME_SIZE || strlen(pool) > POOL_NAME_SIZE)
		return RPC_PARAM_ERROR;

	fd = connect_metaserver(ip, port);
	if(fd < 0){
		printf("rpc: connect metaserver(%s:%d) failed!\n", ip, port);
		return RPC_SOCKET_ERROR;
	}

	body.sid = sid_seed ++;
	body.type = type;
	strncpy(body.path, url, MAX_FILE_NAME);
	strncpy(body.user, user, USERNAME_SIZE);
	strncpy(body.ip, op_ip, USERNAME_SIZE);
	strncpy(body.pool, pool, POOL_NAME_SIZE);

	send_packet(fd, ADD_FILE_LOG, &body);

	ret = process_ack(ADD_FILE_LOG_ACK, body.sid, fd, timeout, err);

	release_socket(fd, inet_addr(ip));

	return ret;
}

int rpc_is_upload_file(const char* ip, int port, const char* user, const char* exname, int file_size, int timeout, char** err)
{
	int ret;
	int fd;
	check_upload_t body;

	assert(user != NULL && exname != 0);
	if(strlen(user) >= USERNAME_SIZE || strlen(exname) >= USERNAME_SIZE)
		return RPC_PARAM_ERROR;

	fd = connect_metaserver(ip, port);
	if(fd < 0){
		printf("rpc: connect metaserver(%s:%d) failed!\n", ip, port);
		return RPC_SOCKET_ERROR;
	}

	body.sid = sid_seed ++;
	body.file_size = file_size;
	strncpy(body.exname, exname, USERNAME_SIZE);
	strncpy(body.user, user, USERNAME_SIZE);

	send_packet(fd, CHECK_UPLOAD, &body);

	ret = process_ack(CHECK_UPLOAD_ACK, body.sid, fd, timeout, err);

	release_socket(fd, inet_addr(ip));

	return ret;
}

int rpc_report_upload_file(const char* ip, int port, const char* user, int file_size, int timeout, char** err)
{
	int ret;
	int fd;
	upload_file_t body;

	assert(user != NULL);

	if(strlen(user) >= USERNAME_SIZE)
		return RPC_PARAM_ERROR;

	fd = connect_metaserver(ip, port);
	if(fd < 0){
		printf("rpc: connect metaserver(%s:%d) failed!\n", ip, port);
		return RPC_SOCKET_ERROR;
	}

	body.sid = sid_seed ++;
	body.file_size = file_size;
	strncpy(body.user, user, USERNAME_SIZE);

	send_packet(fd, UPLAOD_FILE, &body);

	ret = process_ack(UPLOAD_FILE_ACK, body.sid, fd, timeout, err);

	release_socket(fd, inet_addr(ip));

	return ret;
}

static int set_user_upload(const char* ip, int port, const char* user, uint32_t flag, int timeout, char** err)
{
	int fd;
	int ret;
	user_flag_t body;

	assert(user != NULL);

	if(strlen(user) >= USERNAME_SIZE)
		return RPC_PARAM_ERROR;

	fd = connect_metaserver(ip, port);
	if(fd < 0){
		printf("rpc: connect metaserver(%s:%d) failed!\n", ip, port);
		return RPC_SOCKET_ERROR;
	}

	body.sid = sid_seed ++;
	body.flag = flag;
	strncpy(body.user, user, USERNAME_SIZE);

	send_packet(fd, SET_USER_FLAG, &body);

	ret = process_ack(SET_USER_FLAG_ACK, body.sid, fd, timeout, err);

	release_socket(fd, inet_addr(ip));

	return ret;
}

int rpc_enable_user(const char* ip, int port, const char* user, int timeout, char** err)
{
	return set_user_upload(ip, port, user, 0, timeout, err);
}

int rpc_disable_user(const char* ip, int port, const char* user, int timeout, char** err)
{
	return set_user_upload(ip, port, user, 1, timeout, err);
}



