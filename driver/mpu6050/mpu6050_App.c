#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "sys/ioctl.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"
#include <poll.h>
#include <sys/select.h>
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>


int main(int argc, char *argv[])
{
	int fd;
	char *filename;
	 short databuf[6];
	 float ax, ay, az,gx, gy, gz;
	int ret = 0;

	if (argc != 2) {
		printf("Error Usage!\r\n");
		return -1;
	}

	filename = argv[1];
	fd = open(filename, O_RDWR);
	if(fd < 0) {
		printf("can't open file %s\r\n", filename);
		return -1;
	}

	while (1) {
		ret = read(fd, databuf, sizeof(databuf));
		if(ret == 0) { 			/* 数据读取成功 */
			ax =  databuf[0]*1.2207e-4f; 	/* ir传感器数据 */
			ay = databuf[1]*1.2207e-4f; 	/* als传感器数据 */
			az =  databuf[2]*1.2207e-4f; 	/* ps传感器数据 */
			gx =  databuf[3]*3.05175e-2f; 	/* ir传感器数据 */
			gy = databuf[4]*3.05175e-2f; 	/* als传感器数据 */
			gz =  databuf[5]*3.05175e-2f; 	/* ps传感器数据 */
			printf("ax = %2f, ay = %2f, az = %2f,gx = %2f, gy = %2f, gz = %2f\r\n", ax, ay, az,gx, gy, gz);
		}
		usleep(200000); /*100ms */
	}
	close(fd);	/* 关闭文件 */	
	return 0;
}

