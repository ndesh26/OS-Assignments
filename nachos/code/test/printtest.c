/* printtest.c
 *	Simple program to test whether printing from a user program works.
 *	
 *	Just do a "syscall" that shuts down the OS.
 *
 * 	NOTE: for some reason, user programs with global data structures 
 *	sometimes haven't worked in the Nachos environment.  So be careful
 *	out there!  One option is to allocate data structures as 
 * 	automatics within a procedure, but if you do this, you have to
 *	be careful to allocate a big enough stack to hold the automatics!
 */

#include "syscall.h"

int
main()
{
    int i = system_call_Fork();
    system_call_PrintInt(i);
    if(i==0)
    {
        system_call_PrintString("ChildPid: ");
        system_call_PrintInt(system_call_GetPID());
        system_call_PrintChar('\n');
   
    }
    else {
        system_call_PrintString("ParentPid: ");
        system_call_PrintInt(system_call_GetPID());
        system_call_PrintChar('\n');
    }
    system_call_Exit(0);
}
