// stats.h 
//	Routines for managing statistics about Nachos performance.
//
// DO NOT CHANGE -- these stats are maintained by the machine emulation.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "utility.h"
#include "stats.h"

//----------------------------------------------------------------------
// Statistics::Statistics
// 	Initialize performance metrics to zero, at system startup.
//----------------------------------------------------------------------

Statistics::Statistics()
{
    totalTicks = idleTicks = systemTicks = userTicks = 0;
    maxWaitingTime = maxCpuBurst = maxThreadCompletionTime = 0;
    noCpuBursts = threadsCompleted = 0;
    totalCpuBurst = totalWaitingTime = totalThreadCompletionTime = 0;
    squareThreadCompletionTime = 0;
    estimationError = 0.0;
    minWaitingTime = minCpuBurst = minThreadCompletionTime = 100000;
    numDiskReads = numDiskWrites = 0;
    numConsoleCharsRead = numConsoleCharsWritten = 0;
    numPageFaults = numPacketsSent = numPacketsRecvd = 0;
}

//----------------------------------------------------------------------
// Statistics::Print
// 	Print performance metrics, when we've finished everything
//	at system shutdown.
//----------------------------------------------------------------------

void
Statistics::Print()
{
    long long int average = totalThreadCompletionTime/threadsCompleted;
    long long int square = squareThreadCompletionTime/threadsCompleted;
    printf("Total CPU busy time: %d\n", cpuBusyTime);
    printf("Total execution time: %d\n", totalTicks);
    printf("CPU utilization: %3f\n", (float)cpuBusyTime/totalTicks);
    printf("CPU bursts: average %d, max %d, min %d\n", totalCpuBurst/noCpuBursts, maxCpuBurst, minCpuBurst);
    printf("Total non zero bursts: %d\n", noCpuBursts);
    printf("Waiting time: average %d, max %d, min %d\n", totalWaitingTime/threadsCompleted, maxWaitingTime, minWaitingTime);
    printf("Total Number of threads: %d\n", threadsCompleted);
    printf("Thread completion time: average %lld, max %d, min %d variance %lld\n", average, maxThreadCompletionTime, minThreadCompletionTime, square - average*average);
    //if (schedulerType == 1)
        printf("Error estiamtion: %lf\n", estimationError/totalCpuBurst);
    printf("\nTicks: total %d, idle %d, system %d, user %d\n", totalTicks,
	idleTicks, systemTicks, userTicks);
    printf("Disk I/O: reads %d, writes %d\n", numDiskReads, numDiskWrites);
    printf("Console I/O: reads %d, writes %d\n", numConsoleCharsRead, 
	numConsoleCharsWritten);
    printf("Paging: faults %d\n", numPageFaults);
    printf("Network I/O: packets received %d, sent %d\n", numPacketsRecvd, 
	numPacketsSent);
}
