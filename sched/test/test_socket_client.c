/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Unix socket client for testing scx_ossim scheduler
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

#define SOCKET_PATH "/tmp/scx_ossim.sock"
#define BUFFER_SIZE 1024

/* Send a command to the server and print the response */
static int send_command(const char *command) {
	int sock_fd;
	struct sockaddr_un addr;
	char buffer[BUFFER_SIZE];
	ssize_t bytes;

	/* Create stream socket */
	sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock_fd < 0) {
		perror("socket");
		return -1;
	}

	/* Connect to server */
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

	if (connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("connect");
		fprintf(stderr, "Make sure scx_ossim is running\n");
		close(sock_fd);
		return -1;
	}

	/* Send command */
	if (write(sock_fd, command, strlen(command)) < 0) {
		perror("write");
		close(sock_fd);
		return -1;
	}

	/* Read response */
	bytes = read(sock_fd, buffer, sizeof(buffer) - 1);
	if (bytes < 0) {
		perror("read");
		close(sock_fd);
		return -1;
	}

	buffer[bytes] = '\0';
	printf("%s", buffer);

	/* Close connection */
	close(sock_fd);
	return 0;
}

/* Interactive mode */
static void interactive_mode(void) {
	char command[256];

	printf("scx_ossim socket client - Interactive mode\n");
	printf("Type 'help' for available commands, 'quit' to exit\n\n");

	while (1) {
		printf("> ");
		fflush(stdout);

		if (fgets(command, sizeof(command), stdin) == NULL) {
			break;
		}

		/* Remove trailing newline */
		size_t len = strlen(command);
		if (len > 0 && command[len - 1] == '\n') {
			command[len - 1] = '\0';
		}

		/* Check for exit commands */
		if (strcmp(command, "quit") == 0 || strcmp(command, "exit") == 0) {
			break;
		}

		/* Skip empty commands */
		if (strlen(command) == 0) {
			continue;
		}

		/* Send command */
		if (send_command(command) < 0) {
			fprintf(stderr, "Failed to communicate with server\n");
			break;
		}
	}

	printf("Goodbye!\n");
}

/* Monitor mode - continuously query stats */
static void monitor_mode(int interval) {
	printf("Monitoring stats every %d second(s). Press Ctrl+C to exit.\n\n", interval);

	while (1) {
		printf("=== %ld ===\n", (long)time(NULL));
		if (send_command("stats") < 0) {
			fprintf(stderr, "Failed to get stats\n");
			break;
		}
		printf("\n");
		sleep(interval);
	}
}

static void print_usage(const char *prog_name) {
	printf("Usage: %s [OPTIONS] [COMMAND]\n", prog_name);
	printf("\nOptions:\n");
	printf("  -i              Interactive mode (default if no command given)\n");
	printf("  -m INTERVAL     Monitor mode - query stats every INTERVAL seconds\n");
	printf("  -h              Show this help\n");
	printf("\nCommands (for one-shot mode):\n");
	printf("  stats           Get both local and global enqueue counts\n");
	printf("  local           Get local enqueue count\n");
	printf("  global          Get global enqueue count\n");
	printf("  shutdown        Shutdown the scheduler\n");
	printf("  help            Show available server commands\n");
	printf("\nExamples:\n");
	printf("  %s stats                  # Query stats once and exit\n", prog_name);
	printf("  %s -i                     # Interactive mode\n", prog_name);
	printf("  %s -m 2                   # Monitor stats every 2 seconds\n", prog_name);
	printf("  %s shutdown               # Shutdown the scheduler\n", prog_name);
}

int main(int argc, char **argv) {
	int opt;
	int interactive = 0;
	int monitor_interval = 0;

	/* Parse options */
	while ((opt = getopt(argc, argv, "im:h")) != -1) {
		switch (opt) {
		case 'i':
			interactive = 1;
			break;
		case 'm':
			monitor_interval = atoi(optarg);
			if (monitor_interval <= 0) {
				fprintf(stderr, "Invalid interval: %s\n", optarg);
				return 1;
			}
			break;
		case 'h':
			print_usage(argv[0]);
			return 0;
		default:
			print_usage(argv[0]);
			return 1;
		}
	}

	/* Monitor mode */
	if (monitor_interval > 0) {
		monitor_mode(monitor_interval);
		return 0;
	}

	/* One-shot command mode */
	if (optind < argc) {
		/* Concatenate remaining arguments as command */
		char command[256] = "";
		for (int i = optind; i < argc; i++) {
			if (i > optind) strcat(command, " ");
			strncat(command, argv[i], sizeof(command) - strlen(command) - 1);
		}

		return send_command(command) == 0 ? 0 : 1;
	}

	/* Default to interactive mode */
	interactive_mode();
	return 0;
}
