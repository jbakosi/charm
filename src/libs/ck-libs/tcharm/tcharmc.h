/*
User-callable C API for TCHARM library
Orion Sky Lawlor, olawlor@acm.org, 11/20/2001
*/
#ifndef __TCHARM_H
#define __TCHARM_H

#include "pup_c.h"

#ifdef __cplusplus
extern "C" {
#endif


/*User callbacks: you define these functions*/
void TCHARM_User_node_setup(void);
void TCHARM_User_setup(void);


/**** Routines you can call from UserNodeSetup: ****/

/*Register readonly global variables-- these will be broadcast
after init and available in driver.*/
typedef void (*TCpupReadonlyGlobal)(pup_er p);
void TCHARM_Readonly_globals(TCpupReadonlyGlobal fn);

/**** Routines you can call from UserSetup: ****/

/*Set the size of the thread stack*/
void TCHARM_Set_stack_size(int newStackSize);

/*Create a new array of threads, which will be bound to by subsequent libraries*/
typedef void (*TCHARM_Thread_start_fn)(void);
void TCHARM_Create(int nThreads,TCHARM_Thread_start_fn threadFn);

/*As above, but pass along (arbitrary) data to thread*/
typedef void (*TCHARM_Thread_data_start_fn)(void *threadData);
void TCHARM_Create_data(int nThreads,TCHARM_Thread_data_start_fn threadFn,
		  void *threadData,int threadDataLen);

/*Get the number of chunks we expect based on the command line*/
int TCHARM_Get_num_chunks(void);


/**** Routines you can call from the thread (driver) ****/
int TCHARM_Element(void);
int TCHARM_Num_elements(void);
void TCHARM_Barrier(void);
void TCHARM_Migrate(void);
void TCHARM_Migrate_to(int destPE);
void TCHARM_Done(void);
void TCHARM_Yield(void);

/* Set/get thread-private ("thread global") data. */
typedef void (*TCHARM_Pup_fn)(pup_er p,void *data);
int TCHARM_Register(void *data,TCHARM_Pup_fn pfn);
void *TCHARM_Get_userdata(int id);

/* Alternate API for Set/get thread-private ("thread global") data. */
typedef void (*TCHARM_Pup_global_fn)(pup_er p);
void TCHARM_Set_global(int globalID,void *new_value,TCHARM_Pup_global_fn pup_or_NULL);
void *TCHARM_Get_global(int globalID);


/*Get the local wall clock.  Unlike CkWalltimer, is
  monotonically increasing, even with migration and 
  unsynchronized clocks. */
double TCHARM_Wall_timer(void);


/*Standalone startup routine*/
void TCHARM_Init(int *argc,char ***argv);

/*Internal library routine*/
void TCHARM_In_user_setup(void);

#ifdef __cplusplus
};
#endif
#endif /*def(thisHeader)*/

