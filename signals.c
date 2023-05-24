#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
// #include <sys/sem.h> // uncomment if synch of processes is required!


int SIGCALL = 13;
int SIGEOF = 14;
int shared_sema_id; // use this if it is required to synch processes!


char FILENAME[] = "/root/temp.txt";

struct stops { // interface for signal target
	unsigned requires_start_stop, requires_end_stop;
};

unsigned req_stop_start(struct stops stop_requirements) {
	return stop_requirements.requires_start_stop;
}

unsigned req_stop_end(struct stops stop_requirements) {
        return stop_requirements.requires_end_stop;
}

sigset_t initialize_sigset(int signals[], int count){
	sigset_t set;
    	sigemptyset(&set);
	for (size_t i = 0; i < count; i++){
		sigaddset(&set, signals[i]);
	}
	return set;
}

void send_signal(pid_t addr, int signal){
	if(addr) {
		kill(addr, signal);
	}
}

void read_file_target(FILE* cur_file, int start_cursor, pid_t call_next, struct stops stop_req) {
        /*
		M->2->3->M->2->...
		1. Check work regime. 
			0 1 stop_req = nofreeze at start but freeze at next iter
			1 1 stop_req = freeze at start and at the end
			1 0 stop_req = freeze start nofreeze end
		2. Listen to signals
			if sigcall then continue
			if sigeof -> one of processes reached EOF, kill process and trigger next, which does the same
		3. Recall self, perform next iteration
		4. Issue of multiprocessing: 
			Processes are not synchronized and stdout mixed with data from different processes. 
			How to fix: use semaphore.
	*/

	int *inp;
	int signals[] = {SIGCALL, SIGEOF};
        sigset_t required_signals = initialize_sigset(signals, 2);
        if(req_stop_start(stop_req) == 0){
                sigwait(&required_signals, inp);
		if (*signal == SIGEOF) { // idk how to do this better
			return;
        	}
        }
        char cur_symb;
        fseek(cur_file, start_cursor, SEEK_SET);
	if((cur_symb = fgetc(cur_file)) == EOF){
		send_signal(call_next, SIGEOF);
		return;
	}
	putchar(cur_symb);
	putchar('\n');
        send_signal(call_next, SIGCALL);
        if(req_stop_end(stop_req) == 1) {
                sigwait(&required_signals, inp);
		if (*signal == SIGEOF) { // idk how to do this better
                        return;
                }
        }
	start_cursor += 3;
        read_file_target(cur_file, start_cursor, call_next, stop_req);
}

void read_file_task(int start_cursor, pid_t call_next, struct stops stop_req) { // context manager
	// handles file opening and closing
	FILE* cur_file = fopen(FILENAME, "rb");
	if (!cur_file) {
		perror("File error!");
		exit(-2);
	}
	read_file_target(cur_file, start_cursor, call_next, stop_req);
        fclose(cur_file);
}

void* execute_in_process(pid_t work_pid, void* (*func)(void*), void *args){
	// wraps process target
	// IT IS COMPLETELY RIGHT YOU MORRON
	if (getpid() == work_pid){

		void* res = (*func)(args);
		return res;
	}
	return NULL;
}

pid_t spawn_process() {
	// spawns processes
	// IT IS COMPLETELY RIGHT DONT TOUCH IT YOU MORRON
	pid_t spawned = fork();
	pid_t child;
	switch(spawned) {
		case -1:
			perror("forking failed");
			exit(-1);
		case 0:
			child = getpid();
			return child;
		default:
			return -1;
	}
}


int main() {
	pid_t main_pid = getpid();
	
	pid_t process_pid_1 = (pid_t)execute_in_process(main_pid, spawn_process, NULL);
	pid_t process_pid_2 = (pid_t)execute_in_process(main_pid, spawn_process, NULL);
	// init arguments

	struct stops stop_req[3] = {
		{0, 1}, {1, 1}, {1, 0}
	};
	void *args1 = {0, process_pid_1, stop_req[0]};
	void *args2 = {1, process_pid_2, stop_req[1]};
	void *args3 = {2, main_pid, stop_req[2]};
	execute_in_process(main_pid, read_file_task, args1);
	execute_in_process(process_pid_1, read_file_task, args2);
	execute_in_process(process_pid_2, read_file_task, args3);
	wait(NULL);
}
