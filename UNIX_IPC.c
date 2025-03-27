/**************************************
 *  UNIX_IPC.c                          *
 *                                    *
 *  compile: "cc UNIX_IPC.c"            * 
 *  host: os.cs.siue.edu              *
 *  Ryan Bender (bendery13)    *
 ***************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/wait.h>

#define SHM_KEY        7210 	        // shared memory key
#define MSG_KEY	       7211	        // (unique) message queue key
				
#define NUM_REPEATS     200		// number of loops fpr high-priority processes
#define NUM_CHILD      4		// number of the child processes
#define BUFFER_SIZE    1024     	// max message queue size

#define ONE_SECOND     1000             // 1 second
#define THREE_SECONDS  3000             // 3 seconds
// shared memory definition
struct my_mem{
	long int counter;
	int parent;
	int child;
	unsigned int go_flag;
	unsigned int done_flag[4];
	int individual_sum[4];
};

// definition of message
struct message{
	long mtype;
	int mnum;
};

void millisleep(unsigned milli_seconds);		 // implements delay in execution
unsigned int uniform_rand(void);			 // random number generator
void waitSpin(struct my_mem*p_shm, int spinner);	 // Spin until program is ready for next step
void process_C1(struct my_mem*p_shm, int msqid);	 // Child #1
void process_C2(struct my_mem*p_shm, int msqid);	 // Child #2
void process_C3(struct my_mem*p_shm, int msqid);	 // Child #3
void process_C4(struct my_mem*p_shm, int msqid);	 // Child #4
/*  Start of Main   */
int main(){

	pid_t process_id;    
	pid_t child_pid[NUM_CHILD];  // Store Child Process PiDs in array
	int i;              	     // external loop counter
	int j;		    	     // internal loop counter
	int k = 0;	    	     // dumy integer
	
	int ret_val;        	     // system-call return value	
	int sem_id;       	     //
	int shm_id;	  	     // the shared memory id
	int shm_size;	    	     // the size of the shared memory
	struct my_mem * p_shm;	     // pointer to the attached shared memory
	
	int msqid_01; 		     //message queue ID (#1)
	key_t msgkey_01;	     //message-queue key(#1)


	for(int temp = 0; temp < 4; temp++){
		child_pid[temp] = 0;			//initialize all child  pid variables to 0 
	}


	// find the shared memory size in bytes 
	shm_size = sizeof(struct my_mem);
	if (shm_size <= 0){
		fprintf(stderr, "sizeof error in acquiring the shared memory size. Terminating..\n");
		exit(0);
	}

	// create a shared memory 
	shm_id = shmget(SHM_KEY, shm_size, 0666 | IPC_CREAT);
	if (shm_id < 0){
		fprintf(stderr, "Failed to create the shared memory. Terminating.. \n");
		exit(0);
	}
	
	// attach the new shared memory
	p_shm = (struct my_mem *)shmat(shm_id, NULL, 0);
	if (p_shm == (struct my_mem*) -1){
		fprintf(stderr, "Failed to attach the shared memory. Terminating..\n");
		exit(0);
	}

	// initialize the shared memory
	p_shm->counter = 0;
	p_shm->parent  = 0;
	p_shm->child   = 0;
	p_shm->go_flag = 0;
	
	
	// instantiate the message buffer
	struct message buf_01;     //for #1
	msgkey_01 = MSG_KEY;       // the message queue ID key
	
	// create a new message queue
	msqid_01 = msgget(msgkey_01, 0666 | IPC_CREAT);
	if (msqid_01 < 0){
		fprintf(stderr, "Failed to create message queue. Terminating..\n");
		exit(0);
	}
	millisleep(100);
	
	pid_t parent_pid = getpid();		//save parent pid to access after fork()

	/******************************************
	 *					  					  *
	 *	COMPUTATION IS STARTING		 		  *
	 *	SETUP COMPLETE                    	  *
	 *					  					  *
	 * ****************************************/

	printf("=============  The MASTER PROCESS STARTS ==============\n");
	//Create the 4 child processes
	for (i = 0; i < NUM_CHILD; i++){
		if(getpid() == parent_pid){
			if(fork() == 0){
				 child_pid[i] = getpid();			// Store each child's pid in the array to keep track
			}
				if(getpid() != parent_pid){break;}
			if(getpid() == parent_pid){
				p_shm->child += 1; 				// Keep track of how many active child exist 
			}
			if(child_pid[i] < 0){
				fprintf(stderr, "Fork failed...");		// Send error message if fork fails
			}

		}
	}
	
	millisleep(ONE_SECOND);

	//Save pids of each child to access
	if(getpid() == child_pid[0] ){
		process_C1(p_shm, msqid_01);			// Begin Module for Child 1
	}
	
	else if(getpid() == child_pid[1]){
		process_C2(p_shm, msqid_01);			// Begin Module for Child 2
	}
	if(getpid() == parent_pid){
		waitSpin(p_shm, 3);
		while(p_shm->go_flag < 1){}
		printf("-------------------- the master process waits for children to terminate\n\n ");
		p_shm->go_flag +=1;	
	}
	// Save pid for processes 3 and 4	
	 if(getpid() == child_pid[2]){
		 while(p_shm->go_flag < 2){}			// Wait for Child 1 and 2 to finish setting up
		process_C3(p_shm, msqid_01);			//Begin Module for Child 3
		p_shm->go_flag +=1;		
	}
	
	else if(getpid() == child_pid[3]){
		while(p_shm->go_flag < 3){}			// Wait for Children 1 - 3 to finish setting up
		process_C4(p_shm, msqid_01);			// Begin Module for Child 4
		p_shm->go_flag +=1;
	}	

	
	/***************************
	 *	FINAL REPORT	       *
	 *		                   *
	 ***************************/
	if(getpid() == parent_pid){
	while(p_shm->done_flag[0] != 1 || p_shm->done_flag[1] != 1 || p_shm->done_flag[2] != 1 || p_shm->done_flag[3] != 1){}  //Spins untill all processes have been completed 
	// Terminate the final 3 processes after process 1 has finished
	printf("	Child Process #2 is terminating (checksum: %d) ....\n\n", p_shm->individual_sum[1]);
	printf("	Child Process #3 is terminating (checksum: %d) ....\n\n", p_shm->individual_sum[2]);
	printf("	Child Process #4 is terminating (checksum: %d) ....\n\n", p_shm->individual_sum[3]);

	// Calculate the total sum of message send and received
	int sendsum = p_shm->individual_sum[3] + p_shm->individual_sum[2];		// Counts Messages sent
	int recsum = p_shm->individual_sum[1] + p_shm->individual_sum[0];		// Counts Message received
	
	// Final Report to calculate the sums 
	printf("Master Process Report ********************\n");
	printf("  C1 checksum: %d\n", p_shm->individual_sum[0]);
	printf("  C2 checksum: %d\n", p_shm->individual_sum[1]);
	printf("  C3 checksum: %d\n", p_shm->individual_sum[2]);
	printf("  C4 checksum: %d\n", p_shm->individual_sum[3]);
	
	printf("  SEND-CHECKSUM: %d\n", sendsum);
	printf("  RECV-CHECKSUM: %d\n", recsum);
	printf("-------------------- the master process is terminating ...\n\n\n");	
	}
	/*************************
	 *	CLEAN UP TIME	     *
	 * ***********************/
	if(getpid() == parent_pid){
	// Delete message queue
	if(msgctl(msqid_01, IPC_RMID, NULL) == -1){
		fprintf(stderr, "Failed to delete message queue.\n");
	}else{
		//	printf("Message queue deleted successfully.\n");	can turn back on for testing, not needed for execution
	}
	// Delete shared memory
	if(shmctl(shm_id, IPC_RMID, NULL) == -1){
		fprintf(stderr, "Failed to delete shared memory.\n");
	} else{
		//	printf("Shared memory deleted successfully.\n");	can turn back on for testing, not needed for execution
	}	}
	return 0;
}


	
// function millisleep ---------------- 	    		// Tells program to stop for set number of milliseconds to help processes be seen
void millisleep(unsigned milli_seconds){
	usleep(milli_seconds * 1000);
}

// function uniform_rand -------------		   		// Creates a uniform random number
unsigned int uniform_rand(void){
/*  generates a random number 0 ~ 999  */
	unsigned int my_rand;
	my_rand = rand() % 1000;
	return(my_rand);
}

// function waitSpin -------------				// Tells process to idle until program is ready for it
void waitSpin(struct my_mem * p_shm, int spinner){
	int dummy_count = 0;
	while(p_shm->counter < spinner){
		dummy_count++;					// Spins for fun until condition is fulfulled
	}
	p_shm->counter += 1;
}



// Process C1 ================					// First Child - Consumer 1
void process_C1(struct my_mem * p_shm, int msqid){
	int i;				// the loop counter
	int status; 			// result status code
	unsigned int my_rand;		// a random number
	unsigned int checksum = 0;      // the local checksum
	struct message msg;		// message to store received variable

	// Required output #1 -------------------------
	printf("	Child Process #1 is created ....\n");

	printf("	I am the first consumer ....\n\n");
	
	p_shm->counter +=1;
	 waitSpin(p_shm, 2);	
	millisleep(50);
	p_shm->go_flag = 1;

	while(p_shm->go_flag < 6){}				// idles until ready

	for(i = 0; i < NUM_REPEATS; ++i){
		msgrcv(msqid, &msg, sizeof(msg.mnum), 1, 0);	// Read 200 messages and sum them up
		checksum += msg.mnum;
	}
	p_shm->go_flag += 1;


	// Wait 3 seconds
	millisleep (THREE_SECONDS);
	
	while (p_shm->go_flag < 8){}
	//Required output #2 ------------------------
	printf("	Child Process #1 is terminating (checksum: %d) ....\n\n", checksum);
	//raise my "Done_Flag" ----------------------
	p_shm->individual_sum[0] = checksum;				// Submit checksum to parent process
	p_shm-> done_flag[0] = 1;              				// Tells parent that process is complete
	p_shm->child -= 1;						// Subtracts itself from child count
}	

// Process C2 ===============				// Second Child - Consumer 2
void process_C2(struct my_mem * p_shm, int msqid){
	int i; 				// the loop counter
	int status; 			// result status code
	unsigned int my_rand; 		// a random number
	unsigned int checksum = 0; 	// the local checksum
	struct message msg; 		// message to store received variable	

	waitSpin(p_shm, 1);						// idles

	printf("	Child Process #2 is created ....\n");
	printf("	I am the second consumer ....\n\n");
	while(p_shm->go_flag < 6){}
	
	for(i = 0; i < NUM_REPEATS; ++i){
		msgrcv(msqid, &msg, sizeof(msg.mnum), 1, 0);		// Read 200 messages and sum them up
		checksum += msg.mnum;
	}
		p_shm->go_flag += 1;

	//Wait 3 seconds
	millisleep(THREE_SECONDS);

//	while(p_shm->done_flag[0] != 1){}
//	printf("	Child Process #2 is terminating (checksum: %d) ....\n\n", checksum);

	p_shm->individual_sum[1] = checksum;				// Submit checksum to parent process
	p_shm->done_flag[1] = 1;					// Tells parent that process is complete
	p_shm->child -= 1;						// Subtracts itself from child count
}


// Process C3 ============				// Third Child - Producer 1
void process_C3(struct my_mem * p_shm, int msqid){
	int i; 				// the loop counter
	int status;			// result status code
	unsigned int my_rand;		// a random number
	unsigned int checksum = 0;	// the local checksum
	struct message msg;		// message to send to queue

	waitSpin(p_shm, 4);				// idles

	printf("	Child Process #3 created ....\n");
	printf("	I am the first producer ....\n\n");
	srand(time(0));
	
	msg.mtype = 1;
	for (i = 0; i < NUM_REPEATS; ++i){
		msg.mnum = uniform_rand();
		checksum += msg.mnum;
		msgsnd(msqid, &msg, sizeof(msg.mnum), 0);		// Send 200 messages and keep track of sum
	}

	p_shm->go_flag += 1;

	//Wait 3 seconds
	millisleep(THREE_SECONDS);	
	
//	while(p_shm->done_flag[1] != 1){}
//	printf("	Child Process #3 is terminating (checksum: %d) ....\n\n", checksum);

	p_shm->individual_sum[2] = checksum;				// Submit checksum to parent process
	p_shm->done_flag[2] = 1;					// Tells parent that process is complete
	p_shm->child -= 1;						// Subtracts itself from child count

}
// Process C4 ===========
void process_C4(struct my_mem * p_shm, int msqid){	// Fourth Child - Producer 2
	int i;				// the loop counter
	int status;			// result status code
	unsigned int my_rand;		// a random number
	unsigned int checksum = 0;	// the local checksum
	struct message msg;		// message to send to queue

	waitSpin(p_shm, 5);				//idles
	printf("	Child Process #4 created ....\n");
	printf("	I am the second producer ....\n\n");
	
	msg.mtype = 1;
	for(i = 0; i < NUM_REPEATS; ++i){
		msg.mnum = uniform_rand();
		checksum += msg.mnum;
		msgsnd(msqid, &msg, sizeof(msg.mnum), 0);		// Send 200 messages and keep track of sum
	}
	
	p_shm->go_flag += 1;

	//Wait 3 seconds
	millisleep(ONE_SECOND);
//	while(p_shm->done_flag[2] != 1){}
//	printf("	Child Process #4 is terminating (checksum: %d) ....\n\n", checksum);
	p_shm->individual_sum[3] = checksum;				// Submit checksum to parent process
	p_shm->done_flag[3] = 1;					// Tells parent that process is complete

}
