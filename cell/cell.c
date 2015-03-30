#include <assert.h>
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
#include <time.h>

#include "cell.h"
#include "cell_handler.h"
#include "poll_thread.h"
#include "cell_codec.h"
#include "cell_pool.h"
#include "cell_log.h"
#include "hash.h"
#include "cell_lru_cache.h"
#include "cell_config.h"
#include "cell_zk.h"

cell_pool_t* strm_pool = NULL;
cell_pool_t* handler_pool = NULL;
db_pool_t* db_pool = NULL;

int	daemon_run = 0;

#define DAY_SEC		86400

static void handle_pipe(int sig)
{

}

/*屏蔽pipe信号*/
static void ignore_pipe()
{
	struct sigaction action;
	action.sa_handler = handle_pipe;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	sigaction(SIGPIPE, &action, NULL);
}

/*进程关闭信号处理*/
static void sig_handler(const int sig)
{
	int ret;
	if(sig != SIGTERM && sig != SIGQUIT && sig != SIGINT)
		return ;

	if(daemon_run != 0)
		daemon_run = 0;

	printf("signal(%d) received, try to exit deamon gracefully ..\n",sig);
}

static int32_t server_socket(uint16_t port)
{
	int fd;
	int flags, buf_size;
	struct sockaddr_in addr;
	struct linger ling = {0, 0};

	fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(fd < 0){
		log_fatal("create socket failed!");
		return -1;
	}

	/*设置socket noblock*/
	if ((flags = fcntl(fd, F_GETFL, 0)) < 0 ||
		fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
			log_fatal("setting O_NONBLOCK");
			close(fd);
			return -1;
	}

	flags = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void *)&flags, sizeof(flags));
	setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (void *)&flags, sizeof(flags));
	setsockopt(fd, SOL_SOCKET, SO_LINGER, (void *)&ling, sizeof(ling));

	buf_size = 16 * 1024;
	setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (void *)&buf_size, sizeof(int32_t));
	setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (void *)&buf_size, sizeof(int32_t));

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);

	if(bind(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) < 0){
		log_error("socket bind port failed, port = %d", port);
		close(fd);
		return -1;
	}
	else{
		if(listen(fd, 1024) == -1){
			log_fatal("socket listen failed, err = %d", errno);
			close(fd);
			return -1;
		}
	}

	/*建立一个监听socket,并加入到epoll监视集合中*/
	if(handler_new(fd, HANDLER_LISTENING) == NULL){
		log_error("create hander failed, fd = %d", fd);
		exit(-1);
	}

	log_info("open socket server sucess, port = %d", port);

	return 0;
}

static void cell_init(int nthread, uint16_t port, size_t cache_size)
{
	/*创建bin stream pool*/
	strm_pool = pool_create("bin stream pool", sizeof(bin_stream_t), bin_stream_init, bin_stream_destroy, bin_stream_check, bin_stream_reset);
	if(strm_pool == NULL){
		log_fatal("bin strean pool create failed!");
		exit(-1);
	}

	/*创建handler pool*/
	handler_pool = pool_create("handler pool", sizeof(handler_t), handler_init, handler_destroy, handler_check, handler_reset);
	if(handler_pool == NULL){
		log_fatal("handler pool create failed!");
		exit(-1);
	}

	/*数据库连接信息应该从配置文件中来*/
	db_pool = create_db_pool(nthread + 4, cell_config->db_master, cell_config->db_port, cell_config->user, cell_config->passwd, cell_config->db_name);
	if(db_pool == NULL){
		log_fatal("create db pool failed!");
		exit(-1);
	}

	/*创建lru_cache,最大缓冲先设置为1G，后面通过配置文件来确定*/
	create_cache(cache_size);

	ignore_pipe();

	daemon_run = 1;
	/*设置IO threads属性*/
	thread_init(nthread);

	if(server_socket(port) != 0){
		log_error("server socket error.");
		exit(-1);
	}
}

static void cell_destroy()
{
	print_cache();
	destroy_cache();

	thread_destroy();

	destroy_db_pool(db_pool);

	pool_print(handler_pool);
	pool_destroy(handler_pool);

	pool_print(strm_pool);
	pool_destroy(strm_pool);
}

/*构建一个生命周期的KEY*/
static size_t create_life_key(const char* url, char* key, size_t key_size, uint32_t days)
{
	unsigned int hash;
	hash = murMurHash(url, strlen(url));
	/*可能要考虑hash的碰撞!?*/
	unix_time_2_datetime(time(NULL) + (days + 2) * DAY_SEC, key, key_size - 10);
	sprintf(key + strlen(key), "%u", hash);

	return strlen(key);
}

static void add_life_cycle(handler_t* h, add_life_cycle_t* msg)
{
	db_helper_t* help;
	char key[MAX_KEY_SIZE];
	meta_kv_t kv;

	add_life_cycle_ack_t ack;

	kv.key = key;
	kv.key_size = create_life_key(msg->url, key, MAX_KEY_SIZE, msg->days);
	kv.value = msg->url;
	kv.value_size = strlen(msg->url) + 1;

	help = get_helper(db_pool);
	if(help != NULL && replace_kv(help, &kv, "dfs_lifecycle") == 0)
		ack.result = NET_SUCC;
	else
		ack.result = ADD_LIFE_FAILED;
	release_helper(db_pool, help);

	ack.sid = msg->sid;
	hander_send(h, ADD_LIFE_CYCLE_ACK, &ack);
}

static void add_meta(handler_t* h, add_meta_t* m)
{
	meta_kv_t kv;
	bin_stream_t* strm;
	db_helper_t* help;
	add_meta_ack_t ack;

	kv.key = m->path;
	kv.key_size = strlen(m->path);
	
	ack.sid = m->sid;
	ack.result = 0;
	
	strm = pool_alloc(strm_pool);
	if(strm == NULL){
		log_warn("alloc bin stream failed!");
		ack.result = SERVER_ERROR;
		hander_send(h, ADD_META_ACK, &ack);
		return;
	}
	

	help = get_helper(db_pool);
	if(help == NULL){
		ack.result = NO_DB_HELPER;
	}
	else{
		mach_data_write(strm, (uint8_t*)(m->url), strlen(m->url) + 1);
		mach_data_write(strm, (uint8_t*)(m->uploader), strlen(m->uploader) + 1);
		mach_data_write(strm, (uint8_t*)(m->pwd), strlen(m->pwd) + 1);
		mach_data_write(strm, (uint8_t*)(m->crpyt_key), m->crpyt_key_size); 
		mach_uint32_write(strm, m->opened);

		kv.value = (char*)(strm->rptr);
		kv.value_size = strm->used - strm->rsize;
		if(insert_kv(help, &kv, m->pool) == 0){
			ack.result = NET_SUCC;
			/*update lru_cache*/
			insert_cache((uint8_t*)(kv.key), kv.key_size, (uint8_t*)(kv.value), kv.value_size);
		}
		else
			ack.result = INSERT_FAILED;
	}	

	release_helper(db_pool, help);
	pool_free(strm_pool, strm);

	hander_send(h, ADD_META_ACK, &ack);
}
 
static void get_db_meta(db_helper_t* help, const char* key, const char* pool, get_meta_ack_t* ack, int32_t cache_flag)
{
	meta_kv_t kv;
	bin_stream_t* strm;
	cache_item_t* item = NULL;

	int32_t rc = -1;

	memset(ack, 0x00, sizeof(get_meta_ack_t));

	kv.key = (char *)key;
	kv.key_size = strlen(key);

	strm = pool_alloc(strm_pool);
	if(strm == NULL){
		log_warn("alloc bin stream failed!");
		ack->result = SERVER_ERROR;
		return;
	}

	kv.free_flag = FREE_NONE;

	/*查询的时候用的cache做辅助,更新时不用，cache如果一致性在多个metaserver
	  之间掉线然后忽然上线的情况，可能有隐患！！！*/
	if(cache_flag){
		item = get_cache((uint8_t*)(kv.key), kv.key_size);
		if(item != NULL){
			kv.value_size = item->vsize;
			kv.value = (char *)(ITEM_VALUE(item));
			rc = 0;
		}
		else
			rc = get_kv(help, &kv, pool);
	}
	else
		rc = get_kv(help, &kv, pool);

	if(rc < 0){
		log_warn("db have not key, key = %s", kv.key);
		ack->result = KEY_NOT_EXIST;
	}
	else{
		/*解析VALUE*/
		bin_stream_rewind(strm, 1);
		bin_stream_resize(strm, kv.value_size);
		memcpy(strm->wptr, kv.value, kv.value_size);
		strm->wptr += kv.value_size;
		strm->used += kv.value_size;

		if(item != NULL)
			release_cache(item);
		else
			insert_cache((uint8_t*)(kv.key), kv.key_size, (uint8_t*)(kv.value), kv.value_size);

		/*释放内存*/
		destroy_meta_kv(&kv);

		if(mach_data_read(strm, (uint8_t*)(ack->url), MAX_FILE_NAME) == READ_DATA_ERROR){
			log_warn("kv value is error, key = %s", key);
			ack->result = KV_VAULE_ERROR;
			goto END_OF;
		}

		if(mach_data_read(strm, (uint8_t*)(ack->uploader), USERNAME_SIZE) == READ_DATA_ERROR){
			log_warn("kv value is error, key = %s", key);
			ack->result = KV_VAULE_ERROR;
			goto END_OF;
		}

		if(mach_data_read(strm, (uint8_t*)(ack->pwd), USERNAME_SIZE) == READ_DATA_ERROR){
			log_warn("kv value is error, key = %s", key);
			ack->result = KV_VAULE_ERROR;
			goto END_OF;
		}

		ack->crpyt_key_size = mach_data_read(strm, (uint8_t*)(ack->crpyt_key), CRPYT_CODE_SIZE);
		if(ack->crpyt_key_size == READ_DATA_ERROR){
			log_warn("kv value is error, key = %s", key);
			ack->result = KV_VAULE_ERROR;
			goto END_OF;
		}

		mach_uint32_read(strm, &(ack->opened));
		strcpy(ack->path, key);

		ack->result = NET_SUCC;
	}

END_OF:
	pool_free(strm_pool, strm);
}

static void get_meta(handler_t* h, get_meta_t* m)
{
	get_meta_ack_t ack;
	db_helper_t* help;

	help = get_helper(db_pool);
	if(help != NULL){
		get_db_meta(help, m->path, m->pool, &ack, 1);
		release_helper(db_pool, help);
	}
	else
		ack.result = NO_DB_HELPER;

	ack.sid = m->sid;
	hander_send(h, GET_META_ACK, &ack);
}

static void update_meta(handler_t* h, update_meta_t* m)
{
	get_meta_ack_t meta;
	db_helper_t* help;
	bin_stream_t* strm;
	meta_kv_t kv;
	update_meta_ack_t ack;

	ack.sid = m->sid;

	help = get_helper(db_pool);
	if(help != NULL){
		get_db_meta(help, m->path, m->pool, &meta, 0);
		if(meta.result == NET_SUCC){
			strm = pool_alloc(strm_pool);
			if(strm == NULL){
				release_helper(db_pool, help);

				log_warn("alloc bin stream failed!");
				ack.result = SERVER_ERROR;
				hander_send(h, UPDATE_META_ACK, &ack);
				return ;
			}
			/*对信息进行更新*/
			if((m->op & SET_PWD) && strlen(m->pwd) < USERNAME_SIZE)
				strcpy(meta.pwd, m->pwd);
			if(m->op & SET_OPEN)
				meta.opened = m->opened;
			if(m->op & SET_CRPYT && strlen(m->crpyt_key) < CRPYT_CODE_SIZE)
				strcpy(meta.crpyt_key, m->crpyt_key);

			/*进行文件属性序列化*/
			mach_data_write(strm, (uint8_t*)(meta.url), strlen(meta.url) + 1);
			mach_data_write(strm, (uint8_t*)(meta.uploader), strlen(meta.uploader) + 1);
			mach_data_write(strm, (uint8_t*)(meta.pwd), strlen(m->pwd) + 1);
			mach_data_write(strm, (uint8_t*)(meta.crpyt_key), m->crpyt_key_size); 
			mach_uint32_write(strm, meta.opened);

			kv.key = m->path;
			kv.key_size = strlen(m->path);
			kv.value = (char*)(strm->rptr);
			kv.value_size = strm->used - strm->rsize;

			if(update_kv(help, &kv, m->pool) == 0){
				ack.result = NET_SUCC;
				/*update lru_cache*/
				insert_cache((uint8_t *)kv.key, kv.key_size, (uint8_t*)(kv.value), kv.value_size);
			}
			else
				ack.result = UPDATE_KV_ERROR;

			pool_free(strm_pool, strm);
		}
		else/*cache和数据库的一致性同步*/
			erase_cache((uint8_t*)m->path, strlen(m->path));

		release_helper(db_pool, help);
	}
	else
		ack.result = NO_DB_HELPER;
	
	hander_send(h, UPDATE_META_ACK, &ack);
}

static void replace_meta(handler_t* h, replace_meta_ver_t* m)
{
	get_meta_ack_t meta;
	db_helper_t* help;
	bin_stream_t* strm;
	meta_kv_t kv;
	replace_meta_ack_t ack;
	char path_ver[MAX_FILE_NAME];

	ack.sid = m->sid;

	help = get_helper(db_pool);
	if(help != NULL){
		strm = pool_alloc(strm_pool);
		if(strm == NULL){
			release_helper(db_pool, help);
			log_warn("alloc bin stream failed!");
			ack.result = SERVER_ERROR;
			hander_send(h, REPLACE_META_VER_ACK, &ack);
			return ;
		}

		get_db_meta(help, m->path, m->pool, &meta, 0);
		if(meta.result == NET_SUCC){
			/*进行文件属性序列化,先替换原来的meta信息到数据库中*/
			mach_data_write(strm, (uint8_t*)(m->url), strlen(m->url) + 1);
			mach_data_write(strm, (uint8_t*)(meta.uploader), strlen(meta.uploader) + 1);
			mach_data_write(strm, (uint8_t*)(meta.pwd), strlen(meta.pwd) + 1);
			mach_data_write(strm, (uint8_t*)(meta.crpyt_key), meta.crpyt_key_size); 
			mach_uint32_write(strm, meta.opened);

			kv.key = m->path;
			kv.key_size = strlen(m->path);
			kv.value = (char*)(strm->rptr);
			kv.value_size = strm->used - strm->rsize;

			if(update_kv(help, &kv, m->pool) == 0){
				ack.result = NET_SUCC;
				insert_cache((uint8_t *)kv.key, kv.key_size, (uint8_t*)(kv.value), kv.value_size);

				if(m->version > 0){
					/*在增加一个基于版本号旧文件meta信息*/
					bin_stream_rewind(strm, 1);
					mach_data_write(strm, (uint8_t*)(meta.url), strlen(meta.url) + 1);
					mach_data_write(strm, (uint8_t*)(meta.uploader), strlen(meta.uploader) + 1);
					mach_data_write(strm, (uint8_t*)(meta.pwd), strlen(meta.pwd) + 1);
					mach_data_write(strm, (uint8_t*)(meta.crpyt_key), meta.crpyt_key_size); 
					mach_uint32_write(strm, meta.opened);

					/*构建一个旧版本的meta key*/
					sprintf(path_ver, "%s.%d", m->path, m->version);

					kv.key = path_ver;
					kv.key_size = strlen(path_ver);
					kv.value = (char*)(strm->rptr);
					kv.value_size = strm->used - strm->rsize;
					if(replace_kv(help, &kv, m->pool) != 0)
						log_error("replace kv failed, key = %s", kv.key);
				}
			}
			else
				ack.result = UPDATE_KV_ERROR;
		}
		else{ /*原来的meta不存在，新建一个默认的meta信息*/
			mach_data_write(strm, (uint8_t*)(m->url), strlen(m->url) + 1);
			mach_data_write(strm, (uint8_t*)(m->user), strlen(m->user) + 1);
			mach_data_write(strm, (uint8_t*)(""), strlen("") + 1);
			mach_data_write(strm, (uint8_t*)(""), 0); 
			mach_uint32_write(strm, 1);

			kv.key = m->path;
			kv.key_size = strlen(m->path);
			kv.value = (char*)(strm->rptr);
			kv.value_size = strm->used - strm->rsize;
			if(insert_kv(help, &kv, m->pool) == 0){ 
				ack.result = NET_SUCC;
				insert_cache((uint8_t*)(kv.key), kv.key_size, (uint8_t*)(kv.value), kv.value_size); /*update lru_cache*/
			}
			else
				ack.result = INSERT_FAILED;
		}

		pool_free(strm_pool, strm);

		release_helper(db_pool, help);
	}

	hander_send(h, REPLACE_META_VER_ACK, &ack);
}

static void del_meta(handler_t* h, del_meta_t* m)
{
	del_meta_ack_t ack;
	db_helper_t* help;
	meta_kv_t kv;

	kv.key = m->path;
	kv.key_size = strlen(m->path);

	help = get_helper(db_pool);
	if(help == NULL){
		ack.result = NO_DB_HELPER;
	}
	else{
		/*将kv从lru cache中删除*/
		erase_cache((uint8_t*)kv.key, kv.key_size);

		if(delete_kv(help, m->path, strlen(m->path), m->pool) != 0)
			ack.result = DELETE_KV_ERROR;
		else
			ack.result = NET_SUCC;

		release_helper(db_pool, help);
	}

	ack.sid = m->sid;
	hander_send(h, DELETE_META_ACK, &ack);
}

static void add_file_log(handler_t* h, add_log_t* m)
{
	add_log_ack_t ack;
	db_helper_t* help;

	ack.result = NET_SUCC;
	help = get_helper(db_pool);
	if(help != NULL){
		if(insert_dfs_log(help, m) != 0)
			ack.result = INSERT_FAILED;

		release_helper(db_pool, help);
	}
	else
		ack.result = NO_DB_HELPER;

	ack.sid = m->sid;
	hander_send(h, ADD_FILE_LOG_ACK, &ack);
}

static void check_upload(handler_t* h, check_upload_t* m)
{
	check_upload_ack_t ack;
	db_helper_t* help;

	ack.result = NET_SUCC;
	help = get_helper(db_pool);
	if(help != NULL){
		int64_t total_size, used_size;
		int64_t	day_size, day_used;
		int flag;
		int ret = get_dfs_user_info(help, m->user, &total_size, &used_size, &day_size, &day_used, &flag);
		if(ret == -1) /*数据库操作失败*/
			ack.result = KEY_NOT_EXIST;
		else if(ret == -2) /*用户不存在*/
			ack.result = 9;
		else{
			if(!(used_size + m->file_size < total_size && day_size > day_used + m->file_size))
				ack.result = 8; /*空间上限*/
			else if(flag == 1)	/*禁止上传*/
				ack.result = 9;

		}
		release_helper(db_pool, help);
	}
	else
		ack.result = NO_DB_HELPER;

	ack.sid = m->sid;
	hander_send(h, CHECK_UPLOAD_ACK, &ack);
}

static void update_upload_info(handler_t* h, upload_file_t* m)
{
	upload_file_ack_t ack;
	db_helper_t* help;

	ack.result = NET_SUCC;
	help = get_helper(db_pool);
	if(help != NULL){
		if(update_dfs_user_info(help, m->user, m->file_size) != 0)
			ack.result = UPDATE_KV_ERROR;

		release_helper(db_pool, help);
	}
	else
		ack.result = NO_DB_HELPER;

	ack.sid = m->sid;
	hander_send(h, UPLOAD_FILE_ACK, &ack);
}

static void set_user_flag(handler_t* h, user_flag_t* m)
{
	upload_file_ack_t ack;
	db_helper_t* help;

	ack.result = NET_SUCC;
	help = get_helper(db_pool);
	if(help != NULL){
		if(update_dfs_user_flag(help, m->user, m->flag) != 0)
			ack.result = UPDATE_KV_ERROR;

		release_helper(db_pool, help);
	}
	else
		ack.result = NO_DB_HELPER;

	ack.sid = m->sid;
	hander_send(h, SET_USER_FLAG_ACK, &ack);
}

void process(uint16_t msg_id, handler_t* h)
{
	switch(msg_id){
	case TEST_MSG:
		{
			test_msg_body_t test_msg;
			test_msg_decode(h->rstrm, &test_msg);
			log_info("recv = %d, body.id = %d, body.key = %d, body.value = %s", msg_id, test_msg.id, test_msg.key, test_msg.value);
			hander_send(h, msg_id, &test_msg);
		}
		break;

	case ADD_META:
		{
			add_meta_t msg;
			if(add_meta_decode(h->rstrm, &msg) != 0){
				log_error("decode ADD_META msg failed!");
				return ;
			}
			add_meta(h, &msg);
		}
		break;

	case UPDATE_META:
		{
			update_meta_t msg;
			if(update_meta_decode(h->rstrm, &msg) != 0){
				log_error("decode UPDATE_META msg failed!");
				return ;
			}
			update_meta(h, &msg);
		}
		break;

	case REPLACE_META_VER:
		{
			replace_meta_ver_t msg;
			if(replace_meta_decode(h->rstrm, &msg) != 0){
				log_error("decode REPLACE_META_VER msg failed!");
				return ;
			}

			replace_meta(h, &msg);
		}
		break;

	case GET_META:
		{	
			get_meta_t msg;
			if(get_meta_decode(h->rstrm, &msg) != 0){
				log_error("decode GET_META msg failed!");
				return ;
			}

			get_meta(h, &msg);
		}
		break;

	case DELETE_META:
		{
			del_meta_t msg;
			if(del_meta_decode(h->rstrm, &msg) != 0){
				log_error("decode DELETE_META msg failed!");
				return ;
			}

			del_meta(h, &msg);
		}
		break;

	case ADD_LIFE_CYCLE:
		{
			add_life_cycle_t msg;
			if(add_life_cycle_decode(h->rstrm, &msg) != 0){
				log_error("decode ADD_LIFE_CYCLE failed!");
				return ;
			}

			add_life_cycle(h, &msg);
		}
		break;

	case CHECK_UPLOAD:
		{
			check_upload_t msg;
			if(check_upload_decode(h->rstrm, &msg) != 0){
				log_error("decode CHECK_UPLOAD faield!");
				return ;
			}

			check_upload(h, &msg);
		}
		break;

	case UPLAOD_FILE:
		{
			upload_file_t msg;
			if(upload_file_decode(h->rstrm, &msg) != 0){
				log_error("decode UPLAOD_FILE faield!");
				return ;
			}

			update_upload_info(h, &msg);
		}
		break;

	case SET_USER_FLAG:
		{
			user_flag_t msg;
			if(user_flag_decode(h->rstrm, &msg) != 0){
				log_error("decode SET_USER_FLAG failed!");
				return ;
			}

			set_user_flag(h, &msg);
		}
		break;

	case ADD_FILE_LOG:
		{
			add_log_t msg;
			if(add_log_decode(h->rstrm, &msg) != 0){
				log_error("decode ADD_FILE_LOG failed!");
				return ;
			}

			add_file_log(h, &msg);
		}
		break;

	default:
		log_error("unknown message!\n, id = %d\n", msg_id);
	}
}

char* check_zookeeper_path(char* path){
	if(zk_create_node("/DFS", "", 0) == 0 && zk_create_node("/DFS/metaserver", "", 0) == 0 
		&& zk_create_node("/DFS/metaserver/hosts", "", 0) == 0){
			sprintf(path, "/DFS/metaserver/hosts/%s", cell_config->name);
			
			if(zk_node_exists(path) == 0)
				zk_delete_node(path);

			return path;
	}

	return NULL;
}

void connect_zookeeper()
{
	zhandle_t* h = NULL;
	char path[1280];

	h = zk_init(cell_config->zk_host);
	if(h == NULL){
		printf("connect zookeeper failed!\n");
		exit(-1);
	}

	/*创建一个临时节点，并将数据（ip:port）存入临时节点上*/
	if(check_zookeeper_path(path) != NULL && zk_create_node(path, "", 1) == 0){
		char value[VALUE_SIZE];
		sprintf(value, "%s:%d", cell_config->listen_ip, cell_config->listen_port);
		if(zk_set_node(path, value) != 0){
			zk_delete_node(path);
			zk_destroy();

			printf("regist zookeeper failed!, zookeeper host = %s\n", cell_config->zk_host);
			exit(-1);
		}
	}
}

void disconnect_zookeeper()
{
	char path[1280];
	sprintf(path, "/DFS/metaserver/hosts/%s", cell_config->name);

	zk_delete_node(path);
	zk_destroy();
}


int main(int argc, const char* argv[])
{

	if(argc == 2)
		load_config(argv[1]);
	else
		load_config("/etc/cell.conf");

	print_config();

	/*初始化日志*/
	init_log();
	if(open_log(cell_config->log_path, LEVEL_DEBUG) != 0){
		printf("open log failed!\n");
		exit(-1);
	}

	/*add signal callback*/
	if (signal(SIGTERM, sig_handler) == SIG_ERR)
		log_warn("can not catch SIGTERM");
	if (signal(SIGQUIT, sig_handler) == SIG_ERR)
		log_warn("can not catch SIGQUIT");
	if (signal(SIGINT,  sig_handler) == SIG_ERR)
		log_warn("can not catch SIGINT");

	/*注册ZK*/
	connect_zookeeper();

	cell_init(cell_config->thread_n, cell_config->listen_port, cell_config->cache_size);

	loop_run();

	cell_destroy();
	/*注销ZK*/
	disconnect_zookeeper();

	close_log();
	close_config();

	return 0;
}






