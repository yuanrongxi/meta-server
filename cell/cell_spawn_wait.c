#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sysexits.h>

#include <assert.h>

static int caught = 0;

static void caught_signal(int sig)
{
	caught = sig;
}

static int wait_for_process(pid_t pid)
{
	struct sigaction sig_handler;
	int rv = -1;
	int stats = 0;
	int i;
	int sig = -1;

	sig_handler.sa_handler = caught_signal;
	sig_handler.sa_flags = 0;

	sigaction(SIGALRM, &sig_handler, NULL);
	sigaction(SIGHUP, &sig_handler, NULL);
	sigaction(SIGINT, &sig_handler, NULL);
	sigaction(SIGTERM, &sig_handler, NULL);

	for(i = 0;;i++){
		pid_t p = waitpid(pid, &stats, 0);
		if(p == pid){
			if(WIFEXITED(stats) && sig != -1) /*父亲进程kill子进程*/
				rv = -1;
			else
				rv = 0;
			break;
		}
		else{
			sig = (i > 1) ? SIGKILL : SIGINT;
			if(kill(pid, sig) < 0)
				perror("lost child when trying to kill");
			/*等待5秒*/
			alarm(5);
		}
	}

	return rv;
}

int spawn_and_wait()
{
	for(;;){
		pid_t pid = fork();
		if(pid < 0){
			perror("fork");
			return -1;
		}
		else if(pid == 0)
			return 0;
		else{
			if(wait_for_process(pid) == -1){
				return -1;
			}
		}
	}

	return 0;
}

