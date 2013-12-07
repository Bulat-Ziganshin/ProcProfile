/*  ProcProfile 1.1 - A Command Line Process Profiling Tool For Windows
    Written in 2013 by David Catt
    Modified by Bulat Ziganshin
    Placed into public domain */

#define STDPTIME
/* #define CCTERMINATE */
#define POLLINTERVAL 10
#define CHECKINTERVAL 10
#include <windows.h>
#include <psapi.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef STDPTIME
#include <time.h>
#endif

const char* units[] = {"bytes", "KB", "MB"};
const ULONGLONG shifts[] = {0, 10, 20};
const DWORD wdiffs[] = {6, 3, 0};

PROCESS_INFORMATION pi;
BOOL WINAPI breakHdl(DWORD dwCtrlType) {
#ifdef CCTERMINATE
	GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, pi.dwProcessId);
	/* if(!GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, pi.dwProcessId)) TerminateProcess(pi.hProcess, 1); */
#else
	TerminateProcess(pi.hProcess, 1);
#endif
        return TRUE;
}
BOOL matchArg(LPTSTR cl, LPTSTR arg) {
	while(!((*cl == (TCHAR)'\0') || (*arg == (TCHAR)'\0'))) {
		if(*cl != *arg) return FALSE;
		cl += sizeof(TCHAR);
		arg += sizeof(TCHAR);
	}
	return (*cl == (TCHAR)'\0') || (*cl == (TCHAR)' ');
}
BOOL matchArgPart(LPTSTR cl, LPTSTR arg) {
	while(!((*cl == (TCHAR)'\0') || (*arg == (TCHAR)'\0'))) {
		if(*cl != *arg) return FALSE;
		cl += sizeof(TCHAR);
		arg += sizeof(TCHAR);
	}
	return TRUE;
}
void nextArg(LPTSTR* cl) {
	BOOL inq=FALSE;
	while((inq || !(**cl == (TCHAR)' ')) && !(**cl == (TCHAR)'\0')) {
		if(**cl == '"') inq = !inq;
		*cl += sizeof(TCHAR);
	}
	while((**cl == (TCHAR) ' ') && (**cl != (TCHAR) '\0')) *cl += sizeof(TCHAR);
}

int main() {
	/* Declare variables */
	LPTSTR cl,cm;
	STARTUPINFO si;
	PROCESS_MEMORY_COUNTERS mc;
	IO_COUNTERS ic;
#ifdef STDPTIME
	clock_t bt,ft;
#endif
	FILETIME ct,et,kt,ut;
	ULONGLONG ctv,etv,ktv,utv;
	DWORD tec=0,pec=0;
	DWORD exc;
	DWORD u=1,p=0;
	/* Get command line and strip to only arguments */
	cm = cl = GetCommandLine();
	nextArg(&cl);
	/* Parse program arguments */
	while(*cl != (TCHAR) '\0') {
		if(matchArg(cl, "-b")) {
			u = 0;
		} else if(matchArg(cl, "-k")) {
			u = 1;
		} else if(matchArg(cl, "-m")) {
			u = 2;
		} else if(matchArg(cl, "-w")) {
			p = 0;
		} else if(matchArg(cl, "-p")) {
			p = 1;
		} else if(matchArg(cl, "-x")) {
			p = 2;
		} else if(matchArg(cl, "--")) {
			nextArg(&cl);
			break;
		} else {
			break;
		}
		nextArg(&cl);
	}
	/* Print help on empty command line */
	if(*cl == (TCHAR) '\0') {
		fprintf(stdout, "ProcProfile    V1.1\n\n");
		fprintf(stdout, "usage: ProcProfile [arguments] [commandline]\n\n");
		fprintf(stdout, "arguments:\n");
		fprintf(stdout, "   -b   - Output results in bytes\n");
		fprintf(stdout, "   -k   - Output results in kilobytes (default)\n");
		fprintf(stdout, "   -m   - Output results in megabytes\n");
		fprintf(stdout, "   -w   - Wait for exit with infinite wait (default)\n");
		fprintf(stdout, "   -p   - Wait for exit with process polling\n");
		fprintf(stdout, "   -x   - Wait for exit with exit code polling\n");
		fprintf(stdout, "   --   - Stop parsing arguments\n");
		return 1;
	}
	/* Setup structures */
	GetStartupInfo(&si);
	si.cb = sizeof(STARTUPINFO);
	mc.cb = sizeof(PROCESS_MEMORY_COUNTERS);
	/* Create process */
	if(CreateProcess(NULL, cl, NULL, NULL, 0, 0, NULL, NULL, &si, &pi)) {
		/* Retrieve start time */
#ifdef STDPTIME
		bt = clock();
#endif
		/* Add special control handler */
		SetConsoleCtrlHandler(breakHdl, 1);
		/* Wait for process exit */
		switch(p) {
			case 0:
				WaitForSingleObject(pi.hProcess, INFINITE);
				break;
			case 1:
				while(WaitForSingleObject(pi.hProcess, CHECKINTERVAL) == WAIT_TIMEOUT) Sleep(POLLINTERVAL);
				break;
			case 2:
				while(GetExitCodeProcess(pi.hProcess, &exc)) {
					if(exc != STILL_ACTIVE) break;
					Sleep(POLLINTERVAL);
				}
				break;
		}		
		/* Retrieve end time */
#ifdef STDPTIME
		ft = clock();
#endif
		/* Get process information */
		GetProcessMemoryInfo(pi.hProcess, &mc, sizeof(PROCESS_MEMORY_COUNTERS));
		GetProcessIoCounters(pi.hProcess, &ic);
		GetProcessTimes(pi.hProcess, &ct, &et, &kt, &ut);
		GetExitCodeProcess(pi.hProcess, &pec);
		GetExitCodeThread(pi.hThread, &tec);
		/* Convert times into integers */
#ifdef STDPTIME
		ctv = bt;
		etv = ft;
#else
		ctv = ct.dwLowDateTime | ((ULONGLONG)ct.dwHighDateTime << 32);
		etv = et.dwLowDateTime | ((ULONGLONG)et.dwHighDateTime << 32);
#endif
		ktv = kt.dwLowDateTime | ((ULONGLONG)kt.dwHighDateTime << 32);
		utv = ut.dwLowDateTime | ((ULONGLONG)ut.dwHighDateTime << 32);
		/* Convert times into miliseconds */
#ifdef STDPTIME
		ctv = (ctv * 1000) / CLOCKS_PER_SEC;
		etv = (etv * 1000) / CLOCKS_PER_SEC;
#else
		ctv /= 10000;
		etv /= 10000;
#endif
		ktv /= 10000;
		utv /= 10000;
		/* Fix time disorder */
		if(etv < ctv) etv = ctv;
		/* Print process information */
		fprintf(stderr, "\n");
		fprintf(stderr, "Process ID       : %d\n", pi.dwProcessId);
		fprintf(stderr, "Thread ID        : %d\n", pi.dwThreadId);
		fprintf(stderr, "Process Exit Code: %d\n", pec);
		fprintf(stderr, "Thread Exit Code : %d\n", tec);
		/* fprintf(stderr, "\n");
		fprintf(stderr, "Start Date: \n");
		fprintf(stderr, "End Date  : \n"); */
		fprintf(stderr, "\n");
		fprintf(stderr, "User Time        : %*lld.%03llds\n", 8+wdiffs[u], utv/1000, utv%1000);
		fprintf(stderr, "Kernel Time      : %*lld.%03llds\n", 8+wdiffs[u], ktv/1000, ktv%1000);
		fprintf(stderr, "Process Time     : %*lld.%03llds\n", 8+wdiffs[u], (utv+ktv)/1000, (utv+ktv)%1000);
		fprintf(stderr, "Clock Time       : %*lld.%03llds\n", 8+wdiffs[u], (etv-ctv)/1000, (etv-ctv)%1000);
		fprintf(stderr, "\n");
		fprintf(stderr, "Working Set      : %*lld %s\n", 12+wdiffs[u], (ULONGLONG)mc.PeakWorkingSetSize>>shifts[u], units[u]);
		fprintf(stderr, "Paged Pool       : %*lld %s\n", 12+wdiffs[u], (ULONGLONG)mc.QuotaPeakPagedPoolUsage>>shifts[u], units[u]);
		fprintf(stderr, "Nonpaged Pool    : %*lld %s\n", 12+wdiffs[u], (ULONGLONG)mc.QuotaPeakNonPagedPoolUsage>>shifts[u], units[u]);
		fprintf(stderr, "Pagefile         : %*lld %s\n", 12+wdiffs[u], (ULONGLONG)mc.PeakPagefileUsage>>shifts[u], units[u]);
		fprintf(stderr, "Page Fault Count : %d\n", mc.PageFaultCount);
		fprintf(stderr, "\n");
		fprintf(stderr, "IO Read          : %*lld %s (in %15lld reads )\n", 12+wdiffs[u], ic.ReadTransferCount>>shifts[u], units[u], ic.ReadOperationCount);
		fprintf(stderr, "IO Write         : %*lld %s (in %15lld writes)\n", 12+wdiffs[u], ic.WriteTransferCount>>shifts[u], units[u], ic.WriteOperationCount);
		fprintf(stderr, "IO Other         : %*lld %s (in %15lld others)\n", 12+wdiffs[u], ic.OtherTransferCount>>shifts[u], units[u], ic.OtherOperationCount);
		fprintf(stderr, "\n");
		/* Close process and thread handles */
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
		/* Remove the special control handler */
		SetConsoleCtrlHandler(NULL, 0);
		LocalFree(cm);
		return 0;
	} else {
		/* An error occured, print the error */
		fprintf(stderr, "Failed to start process, error code %d.\n", GetLastError());
		LocalFree(cm);
		return 1;
	}
}
