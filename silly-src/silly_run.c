#include "silly_conf.h"
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "silly.h"
#include "compiler.h"
#include "silly_log.h"
#include "silly_env.h"
#include "silly_malloc.h"
#include "silly_timer.h"
#include "silly_socket.h"
#include "silly_worker.h"
#include "silly_daemon.h"

#include "silly_run.h"

struct {
	int exit;
	int run;
	const struct silly_config *conf;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
} R;


static void *
thread_timer(void *arg)
{
	(void)arg;
	for (;;) {
		silly_timer_update();
		if (R.exit)
			break;
		usleep(5000);
		if (silly_worker_msgsize() > 0)
			pthread_cond_signal(&R.cond);
	}
	silly_socket_terminate();
	return NULL;
}


static void *
thread_socket(void *arg)
{
	(void)arg;
	for (;;) {
		int err = silly_socket_poll();
		if (err < 0)
			break;
		pthread_cond_signal(&R.cond);
	}
	pthread_mutex_lock(&R.mutex);
	R.run = 0;
	pthread_cond_signal(&R.cond);
	pthread_mutex_unlock(&R.mutex);
	return NULL;
}

static void *
thread_worker(void *arg)
{
	const struct silly_config *c;
	c = (struct silly_config *)arg;
	silly_worker_start(c);
	while (R.run) {
		silly_worker_dispatch();
		if (!R.run)
			break;
		//allow spurious wakeup, it's harmless
		pthread_mutex_lock(&R.mutex);
		if (R.run)
			pthread_cond_wait(&R.cond, &R.mutex);
		pthread_mutex_unlock(&R.mutex);
	}
	return NULL;
}

static void
thread_create(pthread_t *tid, void *(*start)(void *), void *arg, int cpuid)
{
	int err;
	err = pthread_create(tid, NULL, start, arg);
	if (unlikely(err < 0)) {
		silly_log("thread create fail:%d\n", err);
		exit(-1);
	}
#ifdef USE_CPU_AFFINITY
	if (cpuid < 0)
		return ;
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(cpuid, &cpuset);
	pthread_setaffinity_np(*tid, sizeof(cpuset), &cpuset);
#else
	(void)cpuid;
#endif
	return ;
}

static void
signal_term(int sig)
{
	(void)sig;
	R.exit = 1;
}

static void
signal_usr1(int sig)
{
	(void)sig;
	silly_daemon_sigusr1(R.conf);
}

static int
signal_init()
{
	signal(SIGPIPE, SIG_IGN);
	signal(SIGTERM, signal_term);
	signal(SIGINT, signal_term);
	signal(SIGUSR1, signal_usr1);
	return 0;
}

void
silly_run(const struct silly_config *config)
{
	int i;
	int err;
	pthread_t pid[3];
	R.run = 1;
	R.exit = 0;
	R.conf = config;
	pthread_mutex_init(&R.mutex, NULL);
	pthread_cond_init(&R.cond, NULL);
	silly_daemon_start(config);
	silly_log_start();
	signal_init();
	silly_timer_init();
	err = silly_socket_init();
	if (unlikely(err < 0)) {
		silly_log("%s socket init fail:%d\n", config->selfname, err);
		silly_daemon_stop(config);
		exit(-1);
	}
	silly_worker_init();
	srand(time(NULL));
	thread_create(&pid[0], thread_socket, NULL, config->socketaffinity);
	thread_create(&pid[1], thread_timer, NULL, config->timeraffinity);
	thread_create(&pid[2], thread_worker, (void *)config, config->workeraffinity);
	silly_log("%s %s is running ...\n", config->selfname, SILLY_RELEASE);
	silly_log("cpu affinity setting, timer:%d, socket:%d, worker:%d\n",
		config->timeraffinity, config->socketaffinity, config->workeraffinity);
	for (i = 0; i < 3; i++)
		pthread_join(pid[i], NULL);
	silly_daemon_stop(config);
	pthread_mutex_destroy(&R.mutex);
	pthread_cond_destroy(&R.cond);
	silly_worker_exit();
	silly_timer_exit();
	silly_socket_exit();
	silly_log("%s has already exit...\n", config->selfname);
	return ;
}

void
silly_exit()
{
	R.exit = 1;
}


