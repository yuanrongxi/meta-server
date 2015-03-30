#ifndef __META_RPC_H_
#define __META_RPC_H_

#include "cell_msg.h"

#define RPC_SUCC				0		/*rpc成功*/
#define RPC_TIMEOUT				1		/*rpc网络超时*/
#define RPC_NO_META				2		/*rpc为查找到对应的meta信息*/
#define RPC_DB_FAILED			3		/*metaserver操作数据库失败*/
#define RPC_METASERVER_ERROR	4		/*metaserver内部错误*/
#define RPC_SOCKET_ERROR		5		/*rpc socket连接metaserver时出错*/	
#define RPC_PACKET_ERROR		6		/*metaserver回应的rpc报文有误*/
#define RPC_PARAM_ERROR			7		/*rpc传入的参数有误*/
#define RPC_FILESPACE_LIMITED	8		/*上传者空间已经达到上限，无法上传更多的文件*/
#define RPC_DISABLE_USER		9		/*上传者被列入黑名单，不允许上传文件*/
#define RPC_PWD_ERROR			10		/*提取码错误*/
#define RPC_NOT_OWNER			11		/*非上传者禁止访问*/



/*meta rpc的初始化和销毁，一般在程序的开始和结束调用*/
void			rpc_init();
void			rpc_destroy();

/*****************************************************************
按照文件更新版本号获取文件
*ip				要访问的meta server ip
*port			要访问的meta server port
*path			文件的url或自定义路径文件名,根据HTTP的内容获取的
*version		文件版本，0表示取最新的，其他表示获取对应版本的文件
*timeout		rpc 调用的超时时间，设置为0，默认为3秒，单位是毫秒
*url			返回的对应文件的fdfs的URL,在RPC内部分配的内存，如果rpc失败，返回是NULL，无需释放
				在使用完需要free(url);
*返回值			0表示成功，1表示超时，2表示meta数据不存在
******************************************************************/
int				rpc_get_meta_by_version(const char* ip, int port, const char* path, int version, int timeout, char** url);

typedef struct auth_out_info_t
{
	char		url[MAX_FILE_NAME];				/*fdfs的文件访问的url*/
	char		encrypt_code[CRPYT_CODE_SIZE];	/*加解密key,如果为""串，表示无需加解密*/

}auth_out_info_t;

/*****************************************************************
按照权限访问方式获取文件的meta信息
*ip				要访问的meta server ip
*port			要访问的meta server port
*path			文件的url或自定义路径文件名,根据HTTP的内容获取的
*auth_level		权限判断的方式，1-内外网隔离，2-只有上传者自己能访问，
				3-提取码访问，4-文件需要解密访问，5-其他服务认证访问
*timeout		rpc 调用的超时时间，设置为0，默认为3秒，单位是毫秒
*res			返回的文件url和解密串结构，解密是在外部nginx模块读取到文件之后进行
*返回值			0表示成功，1表示超时，2表示meta数据不存在
******************************************************************/
int				rpc_get_meta_by_auth(const char* ip, int port, const char* path, int auth_level, const char* user, 
						const char* pwd, const char* cookie, auth_out_info_t* res, int timeout);

/*****************************************************************
按照权限访问方式获取文件的meta信息
*ip				要访问的meta server ip
*port			要访问的meta server port
*path			自定义的文件路径
*url			返回的对应文件的fdfs的URL,在RPC内部分配的内存,
				返回的对应文件的fdfs的URL,在RPC内部分配的内存，如果rpc失败，返回是NULL，无需释放
*返回值			0表示成功，1表示超时，2表示meta数据不存在
******************************************************************/
int				rpc_get_meta_by_path(const char* ip, int port, const char* path, char** url, int timeout);
/****************************************************************
增加一个文件的元信息
*info			要设置的文件元信息
*timeout		rpc调用超时时间
*返回值			0表示成功，其他表示失败
****************************************************************/
int				rpc_add_meta(const char* ip, int port, meta_info_t* info, const char* pool, int timeout);

/***************************************************************
修改一个文件的元信息(提取码、加解密KEY、是否对外开放等元信息)
*path			文件的url或自定义路径文件名
*pool			操作的pool名，可以填写""
*opened			0表示对外禁止访问，其他表示无限制访问, -1表示不设置此项，
*pwd			空字符串表示无密码，其他表示有密码(一定是可输入的字符串)，
				NULL表示不设置此项
*crpyt_key		文件加密的key，NULL表示不设置此项
*timeout		rpc调用超时时间
*返回值			0表示成功，其他表示失败
***************************************************************/
int				rpc_update_meta(const char* ip, int port, const char* path, const char* pool, int opened, const char* pwd,
								unsigned char* crpyt_key, size_t crpyt_size, int timeout);

/*****************************************************************
替换一个拥有meta信息的文件，并根据文件版本保存原来文件的meta信息,
如果没有旧文件的meta，会添加一个meta信息，相当于默认的rpc_add_meta
*path			文件的url或自定义路径文件名
*pool			操作的pool名，可以填写""
*url			新的用于替换的文件的fdfs访问url
*user			操作者用户名
*version		文件版本号，用于保存旧文件的meta信息,如果version <= 0,表示不保存旧文件的meta
*timeout		rpc调用超时时间
*返回值			0表示成功，其他表示失败
***************************************************************/
int				rpc_replace_meta(const char* ip, int port, const char* path, const char* pool, const char* url, 
								const char* user, int version, int timeout);

/***************************************************************
删除一个文件的元信息
*path			文件的url或自定义路径文件名
*timeout		rpc调用超时时间
*返回值			0表示成功，其他表示失败
***************************************************************/
int				rpc_delete_meta(const char* ip, int port, const char* path, const char* pool, int timeout);

/***************************************************************
增加一个文件的生命周期
*url			fdfs对应的文件url
*day			存储在fdfs上的周期时间，以天为单位
*cold			0表示周期过后删除，1表示周期过后转为冷数据存储
*timeout		rpc调用超时时间
*返回值			0表示成功，其他表示失败
***************************************************************/
int				rpc_add_life_cycle(const char* ip, int port, const char* url, int day, int cold, int timeout);

/***************************************************************
判断用户是否可以上传文件
*user			上传用户名
*exname			上传文件的扩展名
*file_size		上传文件的大小			
*timeout		rpc调用超时时间
*返回值			0表示允许上传，2表示没有多余空间上传，3表示user列
				入黑名单，其他表示不允许上传
***************************************************************/
int				rpc_is_upload_file(const char* ip, int port, const char* user, const char* exname, int file_size, int timeout);

/***************************************************************
upload 上传文件成功后，报告给metaserver进行用户空间信息更新
*user			上传用户名
*file_size		上传文件的大小			
*timeout		rpc调用超时时间
*返回值			0-可以上传文件，8-空间限制，9-用户上传文件权限被限制，1 ~ 7是普通RPC错误
***************************************************************/
int				rpc_report_upload_file(const char* ip, int port, const char* user, int file_size, int timeout);

/***************************************************************
文件进行添加、删除、修改、元数据操作后必须向meta提交操作日志
*user			操作者用户名
*pool			应用名			
*url			fdfs文件的url
*type			操作类型，0表示上传一个新文件，1表示删除一个fdfs文件，2表示更改了文件元数据
*ip				操作者ip地址
*timeout		rpc调用超时时间
*返回值			0
***************************************************************/
int				rpc_add_file_log(const char* ip, int port, const char* user, const char* pool, const char* url, int type, const char* op_ip, int timeout);

/***************************************************************
允许用户上传
*user			上传用户名
*timeout		rpc调用超时时间
*返回值			0-成功
***************************************************************/
int				rpc_enable_user(const char* ip, int port, const char* user, int timeout);

/***************************************************************
禁止用户上传
*user			上传用户名
*timeout		rpc调用超时时间
*返回值			0-成功
***************************************************************/
int				rpc_disable_user(const char* ip, int port, const char* user, int timeout);

#endif





