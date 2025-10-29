#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define DEVICE_PATH "/dev/ossim"

// Define IOCTL command (must match kernel driver's definition)
#define MY_IOCTL_CMD _IOW('M', 1, int) // Replace with your actual command

int main()
{
	int fd = open(DEVICE_PATH, O_RDWR);
	if (fd < 0) {
		perror("Failed to open device");
		return EXIT_FAILURE;
	}

	int value = 42; // Example argument
	int ret = ioctl(fd, MY_IOCTL_CMD, &value);
	if (ret < 0) {
		perror("ioctl failed");
		close(fd);
		return EXIT_FAILURE;
	}

	printf("ioctl call succeeded\n");
	close(fd);
	return EXIT_SUCCESS;
}
