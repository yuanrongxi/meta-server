#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef WIN32
#include <windows.h>
#else
#include <sys/time.h>
#include <inttypes.h>
#endif

#include "cell_codec.h"

typedef struct test_msg_s
{
	uint8_t		u8;
	int16_t		i16;
	uint32_t	u32;
	uint64_t	u64;

	char		data[1200];
}test_msg_t;

void msg_encode(bin_stream_t* strm, test_msg_t* msg)
{
	bin_stream_rewind(strm, 1);

	mach_uint8_write(strm, msg->u8);
	mach_uint16_write(strm, msg->i16);
	mach_uint32_write(strm, msg->u32);
	mach_uint64_write(strm, msg->u64);
	mach_data_write(strm, (uint8_t *)(msg->data), strlen(msg->data));
}

void msg_decode(bin_stream_t* strm, test_msg_t* msg)
{
	uint32_t len;
	mach_uint8_read(strm, &(msg->u8));
	mach_int16_read(strm, &(msg->i16));
	mach_uint32_read(strm, &(msg->u32));
	mach_uint64_read(strm, &(msg->u64));
	len = mach_data_read(strm, (uint8_t*)(msg->data), sizeof(msg->data) - 1);
	msg->data[len] = 0;
}

void msg_print(test_msg_t* msg)
{
#ifdef WIN32
	printf("u8 = %d, i16 = %d, u32 = %d, u64 = %lld", msg->u8, msg->i16, msg->u32, msg->u64);
	printf(", data = %s\n", msg->data);
#else
	printf("u8 = %d, i16 = %d, u32 = %d, u64 = %"PRIu64",data = %s\n", msg->u8, msg->i16, msg->u32, msg->u64, msg->data);
#endif
	
}

int main(int argc, char* argv[])
{
	bin_stream_t* strm;
	test_msg_t msg1, msg2;
	int i;
#ifdef WIN32
	int b, s;
#else
	struct timeval b, s;
#endif

	strm = (bin_stream_t *)malloc(sizeof(bin_stream_t));
	if(bin_stream_init(strm) == -1){
		free(strm);
		return 0;
	}

	bin_stream_reset(strm, 1);

	msg1.u8 = 10;
	msg1.i16 = -100; 
	msg1.u32 = 1000;
	msg1.u64 = 10000;
	strcpy(msg1.data, "zerok775zerok775zerok775zerok775zerok775zerok775zerok775zerok775zerok775");

	msg_print(&msg1);
#ifdef WIN32
	b = GetTickCount();
#else
	gettimeofday(&b, NULL);
#endif
	for(i = 0; i < 1000000; i ++){
		msg_encode(strm, &msg1);
		msg_decode(strm, &msg2);
	}
#ifdef WIN32
	s = GetTickCount();
	printf("delay = %dms\n", s - b);
#else
	gettimeofday(&s, NULL);
	printf("deay = %dms\n", ((s.tv_sec - b.tv_sec) * 1000 + (s.tv_usec - b.tv_usec) / 1000));
#endif

	msg_print(&msg2);

	bin_stream_reset(strm, 0);
	bin_stream_destroy(strm);

	free(strm);

	return 0;
}



