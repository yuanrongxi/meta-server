#include "cell_msg.h"
#include <malloc.h>
#include <string.h>

void encode_msg(bin_stream_t* strm, uint16_t id, void* ptr)
{
	mach_uint16_write(strm, id);
	switch(id){
	case TEST_MSG:
		test_msg_encode(strm, (test_msg_body_t*)ptr);
		break;

	case ADD_META:
		add_meta_encode(strm, (add_meta_t*)ptr);
		break;

	case ADD_META_ACK:
		ack_encode(strm, (add_meta_ack_t*)ptr);
		break;

	case UPDATE_META:
		update_meta_encode(strm, (update_meta_t*)ptr);
		break;

	case UPDATE_META_ACK:
		ack_encode(strm, (update_meta_ack_t*)ptr);
		break;

	case ADD_LIFE_CYCLE:
		add_life_cycle_encode(strm, (add_life_cycle_t*)ptr);
		break;

	case ADD_LIFE_CYCLE_ACK:
		ack_encode(strm, (add_life_cycle_ack_t*)ptr);
		break;

	case GET_META:
		get_meta_encode(strm, (get_meta_t*)ptr);
		break;

	case GET_META_ACK:
		get_meta_ack_encode(strm, (get_meta_ack_t*)ptr);
		break;

	case DELETE_META:
		del_meta_encode(strm, (del_meta_t *)ptr);
		break;

	case DELETE_META_ACK:
		ack_encode(strm, (del_meta_ack_t*)ptr);
		break;

	case UPLAOD_FILE:
		upload_file_encode(strm, (upload_file_t*)ptr);
		break;

	case UPLOAD_FILE_ACK:
		ack_encode(strm, (upload_file_ack_t*)ptr);
		break;

	case ADD_FILE_LOG:
		add_log_encode(strm, (add_log_t*)ptr);
		break;

	case ADD_FILE_LOG_ACK:
		ack_encode(strm, (add_log_ack_t*)ptr);
		break;

	case CHECK_UPLOAD:
		check_upload_encode(strm, (check_upload_t*)ptr);
		break;

	case CHECK_UPLOAD_ACK:
		ack_encode(strm, (check_upload_ack_t*)ptr);
		break;

	case SET_USER_FLAG:
		user_flag_encode(strm, (user_flag_t*)ptr);
		break;

	case SET_USER_FLAG_ACK:
		ack_encode(strm, (user_flag_ack_t *)ptr);
		break;

	case REPLACE_META_VER:
		replace_meta_encode(strm, (replace_meta_ver_t*)ptr);
		break;

	case REPLACE_META_VER_ACK:
		ack_encode(strm, (replace_meta_ack_t *)ptr);
		break;
	}
}

void test_msg_encode(bin_stream_t* strm, test_msg_body_t* body)
{
	mach_uint16_write(strm, body->id);
	mach_uint32_write(strm, body->key);
	mach_data_write(strm, (uint8_t*)(body->value), strlen(body->value));
}

void test_msg_decode(bin_stream_t* strm, test_msg_body_t* body)
{
	int len;

	mach_uint16_read(strm, &(body->id));
	mach_uint32_read(strm, &(body->key));
	len = mach_data_read(strm, (uint8_t*)(body->value), sizeof(body->value));
	body->value[len] = 0; 
}

void add_meta_encode(bin_stream_t* strm, add_meta_t* meta)
{
	mach_uint32_write(strm, meta->sid);
	mach_data_write(strm, (uint8_t*)(meta->path), strlen(meta->path) + 1);
	mach_data_write(strm, (uint8_t*)(meta->url), strlen(meta->url) + 1);
	mach_data_write(strm, (uint8_t*)(meta->pool), strlen(meta->pool) + 1);
	mach_data_write(strm, (uint8_t*)(meta->uploader), strlen(meta->uploader) + 1);
	mach_data_write(strm, (uint8_t*)(meta->pwd), strlen(meta->pwd) + 1);
	mach_data_write(strm, (uint8_t*)(meta->crpyt_key), meta->crpyt_key_size);
	mach_uint32_write(strm, meta->opened);
}

int add_meta_decode(bin_stream_t* strm, add_meta_t* meta)
{
	mach_uint32_read(strm, &(meta->sid));

	if(mach_data_read(strm, (uint8_t*)(meta->path), MAX_FILE_NAME) == READ_DATA_ERROR)
		return -1;

	if(mach_data_read(strm, (uint8_t*)(meta->url), MAX_FILE_NAME) == READ_DATA_ERROR)
		return -1;

	if(mach_data_read(strm, (uint8_t*)(meta->pool), POOL_NAME_SIZE) == READ_DATA_ERROR)
		return -1;

	if(mach_data_read(strm, (uint8_t*)(meta->uploader), USERNAME_SIZE) == READ_DATA_ERROR)
		return -1;

	if(mach_data_read(strm, (uint8_t*)(meta->pwd), USERNAME_SIZE) == READ_DATA_ERROR)
		return -1;

	meta->crpyt_key_size = mach_data_read(strm, (uint8_t*)(meta->crpyt_key), CRPYT_CODE_SIZE);
	if(meta->crpyt_key_size == READ_DATA_ERROR)
		return -1;

	mach_uint32_read(strm, &(meta->opened));

	return 0;
}

void ack_encode(bin_stream_t* strm, add_meta_ack_t* ack)
{
	mach_uint32_write(strm, ack->sid);
	mach_uint32_write(strm, ack->result);
}

int ack_decode(bin_stream_t* strm, add_meta_ack_t* ack)
{
	mach_uint32_read(strm, &(ack->sid));
	mach_uint32_read(strm, &(ack->result));
	return 0;
}

void update_meta_encode(bin_stream_t* strm, update_meta_t* meta)
{
	mach_uint32_write(strm, meta->sid);
	mach_data_write(strm, (uint8_t*)(meta->path), strlen(meta->path) + 1);
	mach_data_write(strm, (uint8_t*)(meta->pool), strlen(meta->pool) + 1);
	mach_uint32_write(strm, meta->op);
	mach_data_write(strm, (uint8_t*)(meta->pwd), strlen(meta->pwd) + 1);
	mach_data_write(strm, (uint8_t*)(meta->crpyt_key), meta->crpyt_key_size);
	mach_uint32_write(strm, meta->opened);
} 

int update_meta_decode(bin_stream_t* strm, update_meta_t* meta)
{
	mach_uint32_read(strm, &(meta->sid));

	if(mach_data_read(strm, (uint8_t*)meta->path, MAX_FILE_NAME) == READ_DATA_ERROR)
		return -1;

	if(mach_data_read(strm, (uint8_t*)meta->pool, POOL_NAME_SIZE) == READ_DATA_ERROR)
		return -1;

	mach_uint32_read(strm, &(meta->op));

	if(mach_data_read(strm, (uint8_t*)meta->pwd, POOL_NAME_SIZE) == READ_DATA_ERROR)
		return -1;

	meta->crpyt_key_size = mach_data_read(strm, (uint8_t*)(meta->crpyt_key), CRPYT_CODE_SIZE);
	if(meta->crpyt_key_size == READ_DATA_ERROR)
		return -1;

	mach_uint32_read(strm, &(meta->opened));

	return 0;
}

void add_life_cycle_encode(bin_stream_t* strm, add_life_cycle_t* data)
{
	mach_uint32_write(strm, data->sid);
	mach_data_write(strm, (uint8_t*)(data->url), strlen(data->url) + 1);
	mach_uint32_write(strm, data->days);
	mach_int32_write(strm, data->cold);
}

int add_life_cycle_decode(bin_stream_t* strm, add_life_cycle_t* data)
{
	mach_uint32_read(strm, &(data->sid));
	if(mach_data_read(strm, (uint8_t *)(data->url), MAX_FILE_NAME) == READ_DATA_ERROR)
		return -1;

	mach_uint32_read(strm, &(data->days));
	mach_int32_read(strm, &(data->cold));

	return 0;
}

void get_meta_encode(bin_stream_t* strm, get_meta_t* m)
{
	mach_uint32_write(strm, m->sid);
	mach_data_write(strm, (uint8_t*)m->path, strlen(m->path) + 1);
	mach_data_write(strm, (uint8_t*)m->pool, strlen(m->pool) + 1);
}

int get_meta_decode(bin_stream_t* strm, get_meta_t* m)
{
	mach_uint32_read(strm, &(m->sid));
	if(mach_data_read(strm, (uint8_t*)(m->path), MAX_FILE_NAME) == READ_DATA_ERROR)
		return -1;

	if(mach_data_read(strm, (uint8_t*)(m->pool), POOL_NAME_SIZE) == READ_DATA_ERROR)
		return -1;

	return 0;
}

void get_meta_ack_encode(bin_stream_t* strm, get_meta_ack_t* ack)
{
	mach_uint32_write(strm, ack->sid);
	mach_uint32_write(strm, ack->result);
	mach_data_write(strm, (uint8_t*)(ack->path), strlen(ack->path) + 1);
	mach_data_write(strm, (uint8_t*)(ack->url), strlen(ack->url) + 1);
	mach_data_write(strm, (uint8_t*)(ack->uploader), strlen(ack->uploader) + 1);
	mach_data_write(strm, (uint8_t*)(ack->pwd), strlen(ack->pwd) + 1);
	mach_data_write(strm, (uint8_t*)(ack->crpyt_key), ack->crpyt_key_size);
	mach_uint32_write(strm, ack->opened);
}

int get_meta_ack_decode(bin_stream_t* strm, get_meta_ack_t* ack)
{
	mach_uint32_read(strm, &(ack->sid));
	mach_uint32_read(strm, &(ack->result));

	if(mach_data_read(strm, (uint8_t*)(ack->path), MAX_FILE_NAME) == READ_DATA_ERROR)
		return -1;

	if(mach_data_read(strm, (uint8_t*)(ack->url), MAX_FILE_NAME) == READ_DATA_ERROR)
		return -1;

	if(mach_data_read(strm, (uint8_t*)(ack->uploader), USERNAME_SIZE) == READ_DATA_ERROR)
		return -1;

	if(mach_data_read(strm, (uint8_t*)(ack->pwd), USERNAME_SIZE) == READ_DATA_ERROR)
		return -1;

	ack->crpyt_key_size = mach_data_read(strm, (uint8_t*)(ack->crpyt_key), CRPYT_CODE_SIZE);
	if(ack->crpyt_key_size == READ_DATA_ERROR)
		return -1;

	mach_uint32_read(strm, &(ack->opened));

	return 0;
}

void del_meta_encode(bin_stream_t* strm, del_meta_t* m)
{
	mach_uint32_write(strm, m->sid);
	mach_data_write(strm, (uint8_t*)m->path, strlen(m->path) + 1);
	mach_data_write(strm, (uint8_t*)m->pool, strlen(m->pool) + 1);
}

int del_meta_decode(bin_stream_t* strm, del_meta_t* m)
{
	mach_uint32_read(strm, &(m->sid));
	if(mach_data_read(strm, (uint8_t*)(m->path), MAX_FILE_NAME) == READ_DATA_ERROR)
		return -1;

	if(mach_data_read(strm, (uint8_t*)(m->pool), POOL_NAME_SIZE) == READ_DATA_ERROR)
		return -1;

	return 0;
}

void upload_file_encode(bin_stream_t* strm, upload_file_t* m)
{
	mach_uint32_write(strm, m->sid);
	mach_data_write(strm, (uint8_t*)m->user, strlen(m->user) + 1);
	mach_uint32_write(strm, m->file_size);
}

int upload_file_decode(bin_stream_t* strm, upload_file_t* m)
{
	mach_uint32_read(strm, &(m->sid));
	if(mach_data_read(strm, (uint8_t*)(m->user), USERNAME_SIZE) == READ_DATA_ERROR)
		return -1;

	mach_uint32_read(strm, &(m->file_size));

	return 0;
}


void check_upload_encode(bin_stream_t* strm, check_upload_t* m)
{
	mach_uint32_write(strm, m->sid);
	mach_data_write(strm, (uint8_t*)m->user, strlen(m->user) + 1);
	mach_data_write(strm, (uint8_t*)m->exname, strlen(m->exname) + 1);
	mach_uint32_write(strm, m->file_size);
}

int check_upload_decode(bin_stream_t* strm, check_upload_t* m)
{
	mach_uint32_read(strm, &(m->sid));
	if(mach_data_read(strm, (uint8_t*)(m->user), USERNAME_SIZE) == READ_DATA_ERROR)
		return -1;

	if(mach_data_read(strm, (uint8_t*)(m->exname), USERNAME_SIZE) == READ_DATA_ERROR)
		return -1;

	mach_uint32_read(strm, &(m->file_size));

	return 0;
}

void user_flag_encode(bin_stream_t* strm, user_flag_t* m)
{
	mach_uint32_write(strm, m->sid);
	mach_data_write(strm, (uint8_t*)m->user, strlen(m->user) + 1);
	mach_uint32_write(strm, m->flag);
}

int user_flag_decode(bin_stream_t* strm, user_flag_t* m)
{
	mach_uint32_read(strm, &(m->sid));
	if(mach_data_read(strm, (uint8_t*)(m->user), USERNAME_SIZE) == READ_DATA_ERROR)
		return -1;

	mach_uint32_read(strm, &(m->flag));

	return 0;
}


void add_log_encode(bin_stream_t* strm, add_log_t* m)
{
	mach_uint32_write(strm, m->sid);
	mach_data_write(strm, (uint8_t*)m->user, strlen(m->user) + 1);
	mach_data_write(strm, (uint8_t*)m->path, strlen(m->path) + 1);
	mach_data_write(strm, (uint8_t*)m->ip, strlen(m->ip) + 1);
	mach_data_write(strm, (uint8_t*)m->pool, strlen(m->pool) + 1);
	mach_int32_write(strm, m->type);
}

int	add_log_decode(bin_stream_t* strm, add_log_t* m)
{
	mach_uint32_read(strm, &(m->sid));
	if(mach_data_read(strm, (uint8_t*)(m->user), USERNAME_SIZE) == READ_DATA_ERROR)
		return -1;

	if(mach_data_read(strm, (uint8_t*)(m->path), MAX_FILE_NAME) == READ_DATA_ERROR)
		return -1;

	if(mach_data_read(strm, (uint8_t*)(m->ip), USERNAME_SIZE) == READ_DATA_ERROR)
		return -1;

	if(mach_data_read(strm, (uint8_t*)(m->pool), POOL_NAME_SIZE) == READ_DATA_ERROR)
		return -1;

	mach_int32_read(strm, &(m->type));

	return 0;
}

void replace_meta_encode(bin_stream_t* strm, replace_meta_ver_t* m)
{
	mach_uint32_write(strm, m->sid);
	mach_data_write(strm, (uint8_t*)m->path, strlen(m->path) + 1);
	mach_data_write(strm, (uint8_t*)m->url, strlen(m->url) + 1);
	mach_data_write(strm, (uint8_t*)m->user, strlen(m->user) + 1);
	mach_data_write(strm, (uint8_t*)m->pool, strlen(m->pool) + 1);
	mach_int32_write(strm, m->version);
}

int	replace_meta_decode(bin_stream_t* strm, replace_meta_ver_t* m)
{
	mach_uint32_read(strm, &(m->sid));
	
	if(mach_data_read(strm, (uint8_t*)(m->path), MAX_FILE_NAME) == READ_DATA_ERROR)
		return -1;

	if(mach_data_read(strm, (uint8_t*)(m->url), MAX_FILE_NAME) == READ_DATA_ERROR)
		return -1;

	if(mach_data_read(strm, (uint8_t*)(m->user), USERNAME_SIZE) == READ_DATA_ERROR)
		return -1;

	if(mach_data_read(strm, (uint8_t*)(m->pool), POOL_NAME_SIZE) == READ_DATA_ERROR)
		return -1;

	mach_int32_read(strm, &(m->version));

	return 0;
}
















