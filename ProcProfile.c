/*  ProcProfile 1.3 - A Command Line Process Profiling Tool For Windows
    Written in 2013 by David Catt
    Modified by Bulat Ziganshin
    Placed into public domain */

#define PSAPI_VERSION 1
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
const char* unitslow[] = {"bytes", "kb", "mb"};
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
		if(**cl == (TCHAR)'"') inq = !inq;
		*cl += sizeof(TCHAR);
	}
	while((**cl == (TCHAR) ' ') && (**cl != (TCHAR) '\0')) *cl += sizeof(TCHAR);
}
void printSpeed(ULONGLONG bps, DWORD du) {
	if((du >= 0) && (du <= 2)) {
		fprintf(stderr, "%lld %s/s", bps>>shifts[du], unitslow[du]);
	} else if(bps > (3<<20)) {
		fprintf(stderr, "%lld mb/s", bps>>20);
	} else if(bps > (3<<10)) {
		fprintf(stderr, "%lld kb/s", bps>>10);
	} else {
		fprintf(stderr, "%lld bytes/s", bps);
	}
}
void printSVal(ULONGLONG val, DWORD pad, BOOL comma) {
	char *tp,*vb = malloc((pad+28) * sizeof(char));
	DWORD cc = -1;
	if(!vb) return;
	tp = vb + pad + 27;
	*tp = '\0';
	if(!val) *(--tp) = '0';
	while(val) {
		if(comma) if(++cc >= 3) { *(--tp) = ','; cc = 0; }
		*(--tp) = '0' + (val % 10);
		val /= 10;
	}
	while(pad) { *(--tp) = ' '; --pad; }
	fprintf(stderr, "%s", tp);
	free(vb);
}
void printSValWidth(ULONGLONG val, int width, BOOL comma) {
	DWORD sz = width>27?width:27;
	char *tp,*vb = malloc((sz+1) * sizeof(char));
	DWORD cc = -1;
	if(!vb) return;
	tp = vb + sz;
	*tp = '\0';
	if(!val) *(--tp) = '0';
	while(val) {
		if(comma) if(++cc >= 3) { *(--tp) = ','; --width; cc = 0; }
		*(--tp) = '0' + (val % 10); --width;
		val /= 10;
	}
	while(width > 0) { *(--tp) = ' '; --width; }
	fprintf(stderr, "%s", tp);
	free(vb);
}

void printHelp(void) {
	fprintf(stdout, "ProcProfile    V1.3\n\n");
	fprintf(stdout, "usage: ProcProfile [arguments] [commandline]\n\n");
	fprintf(stdout, "arguments:\n");
	fprintf(stdout, "   -b   - Output results in bytes\n");
	fprintf(stdout, "   -k   - Output results in kilobytes (default)\n");
	fprintf(stdout, "   -m   - Output results in megabytes\n");
	fprintf(stdout, "   -w   - Wait for exit with infinite wait (default)\n");
	fprintf(stdout, "   -p   - Wait for exit with process polling\n");
	fprintf(stdout, "   -x   - Wait for exit with exit code polling\n");
	fprintf(stdout, "   -u   - Output status in left aligned format\n");
	fprintf(stdout, "   -r...- Select stat output types\n");
	fprintf(stdout, "   -s   - Print output types and exit\n");
	fprintf(stdout, "   -p_  - Start process with a given priority\n");
	fprintf(stdout, "   -i   - Print possible priorities and exit\n");
	fprintf(stdout, "   -a...- Set process affinity to the given hex string\n");
	fprintf(stdout, "   -t...- Select output templates\n");
	fprintf(stdout, "   -o   - Print available templates and exit\n");
	fprintf(stdout, "   --   - Stop parsing arguments\n");
}
int main(void) {
	/* Declare variables */
	LPTSTR cl,cm,tc;
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
	DWORD cf=0;
	DWORD u=1,p=0,al=-1,ce=-1,t=1,du=-1;
	DWORD_PTR pa=-1;
	BOOL inq=FALSE;
	/* Get command line and strip to only arguments */
	cm = cl = GetCommandLine();
	nextArg(&cl);
	/* Parse program arguments */
	while(*cl != (TCHAR) '\0') {
		if(matchArg(cl, "--help")) {
			printHelp();
			return 1;
		} else if(matchArg(cl, "/?")) {
			printHelp();
			return 1;
		} else if(matchArg(cl, "-b")) {
			u = 0;
			du = 0;
		} else if(matchArg(cl, "-k")) {
			u = 1;
			du = 1;
		} else if(matchArg(cl, "-m")) {
			u = 2;
			du = 2;
		} else if(matchArg(cl, "-w")) {
			p = 0;
		} else if(matchArg(cl, "-p")) {
			p = 1;
		} else if(matchArg(cl, "-x")) {
			p = 2;
		} else if(matchArg(cl, "-u")) {
			al = 0;
		} else if(matchArgPart(cl, "-r")) {
			ce = 0;
			tc = cl + (sizeof(TCHAR)<<1);
			while((inq || !(*tc == (TCHAR)' ')) && !(*tc == (TCHAR)'\0')) {
				switch(*tc) {
					case (TCHAR)'"': inq = !inq; break;
					case (TCHAR)'0': ce |= 1; break;
					case (TCHAR)'1': ce |= 2; break;
					case (TCHAR)'2': ce |= 4; break;
					case (TCHAR)'3': ce |= 8; break;
					case (TCHAR)'4': ce |= 16; break;
					case (TCHAR)'5': ce |= 32; break;
					case (TCHAR)'6': ce |= 64; break;
					case (TCHAR)'7': ce |= 128; break;
					case (TCHAR)'8': ce |= 256; break;
					case (TCHAR)'9': ce |= 512; break;
					case (TCHAR)'a': ce |= 1024; break;
					case (TCHAR)'b': ce |= 2048; break;
					case (TCHAR)'c': ce |= 4096; break;
					case (TCHAR)'d': ce |= 8192; break;
					case (TCHAR)'e': ce |= 16384; break;
					case (TCHAR)'f': ce |= 32768; break;
					case (TCHAR)'i': ce |= 15; break;
					case (TCHAR)'t': ce |= 240; break;
					case (TCHAR)'m': ce |= 7936; break;
					case (TCHAR)'o': ce |= 57344; break;
				}
				tc += sizeof(TCHAR);
			}
		} else if(matchArg(cl, "-s")) {
			fprintf(stdout, "Statistic Output Types:\n");
			fprintf(stdout, "  0 - Process ID\n");
			fprintf(stdout, "  1 - Thread ID\n");
			fprintf(stdout, "  2 - Process Exit Code\n");
			fprintf(stdout, "  3 - Thread Exit Code\n");
			fprintf(stdout, "  4 - User Time\n");
			fprintf(stdout, "  5 - Kernel Time\n");
			fprintf(stdout, "  6 - Process Time\n");
			fprintf(stdout, "  7 - Clock Time\n");
			fprintf(stdout, "  8 - Working Set\n");
			fprintf(stdout, "  9 - Paged Pool\n");
			fprintf(stdout, "  a - Nonpaged Pool\n");
			fprintf(stdout, "  b - Pagefile\n");
			fprintf(stdout, "  c - Page Fault Count\n");
			fprintf(stdout, "  d - IO Read\n");
			fprintf(stdout, "  e - IO Write\n");
			fprintf(stdout, "  f - IO Other\n\n");
			fprintf(stdout, "Statistic Groups:\n");
			fprintf(stdout, "  i - Process/Thread Info\n");
			fprintf(stdout, "  t - Process Times\n");
			fprintf(stdout, "  m - Memory Info\n");
			fprintf(stdout, "  o - IO Info\n");
			return 1;
		} else if(matchArg(cl, "-pi")) {
			cf = IDLE_PRIORITY_CLASS;
		} else if(matchArg(cl, "-pb")) {
			cf = BELOW_NORMAL_PRIORITY_CLASS;
		} else if(matchArg(cl, "-pn")) {
			cf = NORMAL_PRIORITY_CLASS;
		} else if(matchArg(cl, "-pa")) {
			cf = ABOVE_NORMAL_PRIORITY_CLASS;
		} else if(matchArg(cl, "-ph")) {
			cf = HIGH_PRIORITY_CLASS;
		} else if(matchArg(cl, "-pr")) {
			cf = REALTIME_PRIORITY_CLASS;
		} else if(matchArg(cl, "-i")) {
			fprintf(stdout, "Priority Types:\n");
			fprintf(stdout, "  i  - Idle priority\n");
			fprintf(stdout, "  b  - Below normal priority\n");
			fprintf(stdout, "  n  - Normal priority\n");
			fprintf(stdout, "  a  - Above normal priority\n");
			fprintf(stdout, "  h  - High priority\n");
			fprintf(stdout, "  r  - Realtime priority\n");
			return 1;
		} else if(matchArgPart(cl, "-a")) {
			pa = 0;
			tc = cl + (sizeof(TCHAR)<<1);
			while((inq || !(*tc == (TCHAR)' ')) && !(*tc == (TCHAR)'\0')) {
				if((*tc >= '0') && (*tc <= '9')) { pa <<= 4; pa += *tc - '0'; }
				if((*tc >= 'a') && (*tc <= 'f')) { pa <<= 4; pa += *tc - 'a' + 10; }
				if((*tc >= 'A') && (*tc <= 'F')) { pa <<= 4; pa += *tc - 'A' + 10; }
				tc += sizeof(TCHAR);
			}
		} else if(matchArgPart(cl, "-t")) {
			t = 0;
			tc = cl + (sizeof(TCHAR)<<1);
			while((inq || !(*tc == (TCHAR)' ')) && !(*tc == (TCHAR)'\0')) {
				switch(*tc) {
					case (TCHAR)'"': inq = !inq; break;
					case (TCHAR)'n': t |= 1; break;
					case (TCHAR)'b': t |= 2; break;
					case (TCHAR)'x': t |= 4; break;
				}
				tc += sizeof(TCHAR);
			}
		} else if(matchArg(cl, "-o")) {
			fprintf(stdout, "Output Template Types:\n");
			fprintf(stdout, "  n - Use normal output template (default)\n");
			fprintf(stdout, "  b - Use compressor benchmarking output template\n");
			fprintf(stdout, "  x - Use xml output template\n");
			return 1;
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
		printHelp();
		return 1;
	}
	/* Setup structures */
	GetStartupInfo(&si);
	si.cb = sizeof(STARTUPINFO);
	mc.cb = sizeof(PROCESS_MEMORY_COUNTERS);
	/* Create process */
	if(CreateProcess(NULL, cl, NULL, NULL, 0, cf, NULL, NULL, &si, &pi)) {
		/* Retrieve start time */
#ifdef STDPTIME
		bt = clock();
#endif
		/* Set process affinity if required */
		if(pa != -1) SetProcessAffinityMask(pi.hProcess, pa);
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
		if(t&1) {
			if(ce&1)fprintf(stderr, "Process ID       : %d\n", pi.dwProcessId);
			if(ce&2)fprintf(stderr, "Thread ID        : %d\n", pi.dwThreadId);
			if(ce&4)fprintf(stderr, "Process Exit Code: %d\n", pec);
			if(ce&8)fprintf(stderr, "Thread Exit Code : %d\n", tec);
			/* fprintf(stderr, "\n");
			fprintf(stderr, "Start Date: \n");
			fprintf(stderr, "End Date  : \n"); */
			if(ce&15)fprintf(stderr, "\n");
			if(ce&16)fprintf(stderr, "User Time        : %*lld.%03llds\n", (8+wdiffs[u])&al, utv/1000, utv%1000);
			if(ce&32)fprintf(stderr, "Kernel Time      : %*lld.%03llds\n", (8+wdiffs[u])&al, ktv/1000, ktv%1000);
			if(ce&64)fprintf(stderr, "Process Time     : %*lld.%03llds\n", (8+wdiffs[u])&al, (utv+ktv)/1000, (utv+ktv)%1000);
			if(ce&128)fprintf(stderr, "Clock Time       : %*lld.%03llds\n", (8+wdiffs[u])&al, (etv-ctv)/1000, (etv-ctv)%1000);
			if(ce&240)fprintf(stderr, "\n");
			if(ce&256)fprintf(stderr, "Working Set      : %*lld %s\n", (12+wdiffs[u])&al, (ULONGLONG)mc.PeakWorkingSetSize>>shifts[u], units[u]);
			if(ce&512)fprintf(stderr, "Paged Pool       : %*lld %s\n", (12+wdiffs[u])&al, (ULONGLONG)mc.QuotaPeakPagedPoolUsage>>shifts[u], units[u]);
			if(ce&1024)fprintf(stderr, "Nonpaged Pool    : %*lld %s\n", (12+wdiffs[u])&al, (ULONGLONG)mc.QuotaPeakNonPagedPoolUsage>>shifts[u], units[u]);
			if(ce&2048)fprintf(stderr, "Pagefile         : %*lld %s\n", (12+wdiffs[u])&al, (ULONGLONG)mc.PeakPagefileUsage>>shifts[u], units[u]);
			if(ce&4096)fprintf(stderr, "Page Fault Count : %d\n", mc.PageFaultCount);
			if(ce&7936)fprintf(stderr, "\n");
			if(ce&8192)fprintf(stderr, "IO Read          : %*lld %s (in %*lld reads%s)\n", (12+wdiffs[u])&al, ic.ReadTransferCount>>shifts[u], units[u], 15&al, ic.ReadOperationCount, al?" ":"");
			if(ce&16384)fprintf(stderr, "IO Write         : %*lld %s (in %*lld writes)\n", (12+wdiffs[u])&al, ic.WriteTransferCount>>shifts[u], units[u], 15&al, ic.WriteOperationCount);
			if(ce&32768)fprintf(stderr, "IO Other         : %*lld %s (in %*lld others)\n", (12+wdiffs[u])&al, ic.OtherTransferCount>>shifts[u], units[u], 15&al, ic.OtherOperationCount);
			if(ce&57344)fprintf(stderr, "\n");
		}
		if(t&2) {
			/*fprintf(stderr, "%lld -> %lld: %d.%02d%%. Cpu ", ic.ReadTransferCount, ic.WriteTransferCount, ic.ReadTransferCount?(ic.WriteTransferCount*100)/ic.ReadTransferCount:0, (ic.ReadTransferCount?(ic.WriteTransferCount*10000)/ic.ReadTransferCount:0)%100);*/
			printSVal(ic.ReadTransferCount, 0, TRUE);
			fprintf(stderr, " -> ");
			printSVal(ic.WriteTransferCount, 0, TRUE);
			if(ic.ReadTransferCount < ic.WriteTransferCount) {
				ic.ReadTransferCount ^= ic.WriteTransferCount;
				ic.WriteTransferCount ^= ic.ReadTransferCount;
				ic.ReadTransferCount ^= ic.WriteTransferCount;
			}
			fprintf(stderr, ": %d.%02d%%. Cpu ", (DWORD)(ic.ReadTransferCount?(ic.WriteTransferCount*100)/ic.ReadTransferCount:0), (DWORD)((ic.ReadTransferCount?(ic.WriteTransferCount*10000)/ic.ReadTransferCount:0)%100));
			printSpeed((utv+ktv)?(ic.ReadTransferCount*1000)/(utv+ktv):0, du);
			fprintf(stderr, " (%lld.%03lld sec), real ", (utv+ktv)/1000, (utv+ktv)%1000);
			printSpeed((etv-ctv)?(ic.ReadTransferCount*1000)/(etv-ctv):0, du);
			fprintf(stderr, " (%lld.%03lld sec) = %d%%\n", (etv-ctv)/1000, (etv-ctv)%1000, (etv-ctv)?((utv+ktv)*100)/(etv-ctv):0);
		}
		if(t&4) {
			fprintf(stderr, "<profile>\n");
			if(ce&15)fprintf(stderr, "\t<general>\n");
			if(ce&1)fprintf(stderr, "\t\t<processID>%d</processID>\n", pi.dwProcessId);
			if(ce&2)fprintf(stderr, "\t\t<threadID>%d</threadID>\n", pi.dwThreadId);
			if(ce&4)fprintf(stderr, "\t\t<processExitCode>%d</processExitCode>\n", pec);
			if(ce&8)fprintf(stderr, "\t\t<threadExitCode>%d</threadExitCode>\n", tec);
			/* fprintf(stderr, "\n");
			fprintf(stderr, "Start Date: \n");
			fprintf(stderr, "End Date  : \n"); */
			if(ce&15)fprintf(stderr, "\t</general>\n");
			if(ce&240)fprintf(stderr, "\t<timings>\n");
			if(ce&16)fprintf(stderr, "\t\t<user>%lld.%03lld</user>\n", utv/1000, utv%1000);
			if(ce&32)fprintf(stderr, "\t\t<kernel>%lld.%03lld</kernel>\n", ktv/1000, ktv%1000);
			if(ce&64)fprintf(stderr, "\t\t<process>%lld.%03lld</process>\n", (utv+ktv)/1000, (utv+ktv)%1000);
			if(ce&128)fprintf(stderr, "\t\t<clock>%lld.%03lld</clock>\n", (etv-ctv)/1000, (etv-ctv)%1000);
			if(ce&240)fprintf(stderr, "\t</timings>\n");
			if(ce&7936)fprintf(stderr, "\t<memory>\n");
			if(ce&256)fprintf(stderr, "\t\t<workingSet>%lld</workingSet>\n", (ULONGLONG)mc.PeakWorkingSetSize);
			if(ce&512)fprintf(stderr, "\t\t<pagedPool>%lld</pagedPool>\n", (ULONGLONG)mc.QuotaPeakPagedPoolUsage);
			if(ce&1024)fprintf(stderr, "\t\t<nonpagedPool>%lld</nonpagedPool>\n", (ULONGLONG)mc.QuotaPeakNonPagedPoolUsage);
			if(ce&2048)fprintf(stderr, "\t\t<pagefile>%lld</pagefile>\n", (ULONGLONG)mc.PeakPagefileUsage);
			if(ce&4096)fprintf(stderr, "\t\t<pageFaults>%d</pageFaults>\n", mc.PageFaultCount);
			if(ce&7936)fprintf(stderr, "\t</memory>\n");
			if(ce&57344)fprintf(stderr, "\t<io>\n");
			if(ce&8192) {
				fprintf(stderr, "\t\t<readData>%lld</readData>\n", ic.ReadTransferCount);
				fprintf(stderr, "\t\t<readCount>%lld</readCount>\n", ic.ReadOperationCount);
			}
			if(ce&16384) {
				fprintf(stderr, "\t\t<writeData>%lld</writeData>\n", ic.WriteTransferCount);
				fprintf(stderr, "\t\t<writeCount>%lld</writeCount>\n", ic.WriteOperationCount);
			}
			if(ce&32768) {
				fprintf(stderr, "\t\t<otherData>%lld</otherData>\n", ic.OtherTransferCount);
				fprintf(stderr, "\t\t<otherCount>%lld</otherCount>\n", ic.OtherOperationCount);
			}
			if(ce&57344)fprintf(stderr, "\t</io>\n");
			fprintf(stderr, "</profile>\n");
		}
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
