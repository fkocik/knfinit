/*
 * KNF Init system for multi-service Docker container
 * copyright (c) Fabien KOCIK
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public Licence as published by the Free Software
 * Foundation; either version 2 of the Licence, or
 * any later version.
 *
 * This program is distributed in the hope that it will be
 * usefull but WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. See the GNU General Public Licence
 * for more details.
 *
 * You should have received a copy of the GNU Général
 * Public Licence along with this program; if not, write
 * to the Free Software Foundation, Inc., 59 Temple Place
 * Suite 330, Boston, MA 021111-1307, USA.
 */


/* Meta-data */
#ifndef VERSION
#define VERSION SNAPSHOT
#endif
#ifndef BUILD_DATE
#define BUILD_DATE time(NULL)
#endif
#define STRINGIZE(x)		#x
#define STRINGIZE_VALUE(x)	STRINGIZE(x)
#define STR_VERSION		STRINGIZE_VALUE(VERSION)

/* Some behavoral parameters */
#ifndef SHUTDOWN_TIMEOUT
#define SHUTDOWN_TIMEOUT	60
#endif
#ifndef LOOP_INTERVAL
#define LOOP_INTERVAL		300
#endif
#ifndef KILL_INTERVAL
#define KILL_INTERVAL		10
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>

#include <time.h>
#include <errno.h>
#include <libgen.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

/* Monitored services descriptor */
typedef struct {
	char* command;
	pid_t pid;
} Service;

/* Global service table */
struct {
	Service* p;
	int sz;
} services;

/* Shutdown request flag */
int shutdown = 0;

/* Format and returns the BUILD_DATE meta data */
char* build_date() {
	time_t d = (time_t) BUILD_DATE;
	char* c = asctime(localtime(&d));
	int len = strlen(c);
	while (len>0 && isspace(c[len-1])) c[--len]=(char)0x0;
	return c;
}

/* Logging */
void dolog(char* format, ...) {
	char buffer[2*BUFSIZ];
	va_list args;

	va_start(args,format);
	vsnprintf(buffer,sizeof(buffer),format,args);
	va_end(args);

	fprintf(stderr,"INIT: %s\n",buffer);
}

/* Signal handler */
void sig_handler(int sig) {
	int status, pid, i;

	if (signal(sig,sig_handler) == SIG_ERR) {
		dolog("Failed to rearm %d handler: %s",sig,strerror(errno));
		exit(126);
	}
	if ((sig == SIGINT) || (sig == SIGTERM)) {
		dolog("Signal %d received: shutting down ...",sig);
		shutdown = 1;
	} else if (sig == SIGCHLD) {
		if ((pid = waitpid(-1,&status,WNOHANG)) > 0) {
			for (i=0; i < services.sz; i++) if (services.p[i].pid == pid) break;
			if (i < services.sz) {
				if (WIFEXITED(status)) {
					dolog("Service %s (%d) exited status %d",services.p[i].command,pid,WEXITSTATUS(status));
				} else {
					dolog("Service %s (%d) interrupted by signal %d",services.p[i].command,pid,WTERMSIG(status));
				}
				services.p[i].pid = -1;
			} else {
				dolog("Child process %d died",pid);
			}
		} else {
			dolog("Child signal received but wait failed: %s",strerror(errno));
		}
	}
}

/* Starts or restarts the given service */
int start(Service* service) {
	service->pid = fork();
	if (service->pid < 0) {
		dolog("Unable to spawn a new process: %s",strerror(errno));
		return -1;
	} else if (service->pid == 0) {
		if (execlp(service->command,service->command,(char*)NULL) != 0) {
			dolog("Failed to load program %s: %s",service->command,strerror(errno));
			return -2;
		}
	} else {
		return 0;
	}
}

/* Main daemon loop */
int loop() {
	int i;

	while (!shutdown) {
		sleep(1); /* Let's some oxygen to system on signal bursts */
		for (i = 0; i<services.sz; i++) {
			if (services.p[i].pid < 0) {
				if (start(&services.p[i]) == 0) {
					dolog("Started service %s: %d",services.p[i].command,services.p[i].pid);
				} else {
					return 1;
				}
			}
		}
		sleep(LOOP_INTERVAL); /* Not infinite to refresh state even if a signal was missed */
	}
	return 0;
}

/* Process monitor bootstrap */
int main(int argc, char* argv[]) {
	int i, shutdown_complete, sig;
	int sigs[] = { SIGINT, SIGTERM, SIGCHLD };
	pid_t pid;
	time_t start;

	dolog("KNF Init System %s (Built %s)",STR_VERSION,build_date());
	if (argc < 2) {
		fprintf(stderr,"Usage: %s <executable>+\n",basename(argv[0]));
		return 1;
	}
	services.sz = argc - 1;
	services.p = (Service*) malloc (services.sz * sizeof(Service) );
	if (services.p == NULL) {
		dolog("Not enough memory !");
		return 2;
	}
	for (i=1; i < argc; i++) {
		services.p[i-1].command = argv[i];
		services.p[i-1].pid = -1;
	}
	for (i=0; i < sizeof(sigs) / sizeof(int); i++) if (signal(sigs[i],sig_handler) == SIG_ERR) {
		dolog("Failed to install %d signal handler: %s",sigs[i],strerror(errno));
		free(services.p);
		services.p = NULL;
		return 3;
	}
	if (loop() != 0) {
		free(services.p);
		services.p = NULL;
		return 4;
	}

	start = time(NULL);
	sig = SIGTERM;
	dolog("Initiating shutdown with %d signal for %d seconds ...",sig,SHUTDOWN_TIMEOUT);
	do {
		shutdown_complete = 1;
		for (i=0; i<services.sz; i++) {
			pid = services.p[i].pid;
			if (pid > 0) {
				shutdown_complete = 0;
				if (time(NULL) - start > SHUTDOWN_TIMEOUT + (KILL_INTERVAL * 2)) {
					dolog("Too many errors ! Aborting ...");
					free(services.p);
					services.p = NULL;
					return 5;
				} else if (time(NULL) - start > SHUTDOWN_TIMEOUT) {
					dolog("Now killing processes ...");
					sig = SIGKILL;
				}
				if (kill(pid,sig) != 0) {
					if (errno == ESRCH) {
						dolog("Service %s (%d) seems to be down",services.p[i].command,pid);
						services.p[i].pid = -1;
					} else {
						dolog("Failed to send %d signal to %s (%d): %s",sig,services.p[i].command,pid,strerror(errno));
					}
				} else {
					dolog("Sent %d signal to service %s (%d)",sig,services.p[i].command,pid);
				}
			}
		}
		if (!shutdown_complete) sleep(KILL_INTERVAL);
	} while (!shutdown_complete);
	free(services.p);
	services.p = NULL;
	dolog("All services terminated: shutdown complete.");
	return 0;
}

