/*
 * Copyright (c) 2005-2010 Hypertriton, Inc. <http://hypertriton.com/>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <errno.h>
#ifdef HAVE_SIGNAL
#include <signal.h>
#endif

#include <core/core.h>

#ifdef WIN32
#include <windows.h>
#endif


AG_ProcessID
AG_Execute(const char *file, char **argv)
{
	if(!file) {
		AG_SetError("No file provided for execution.");
		return (-1);
	}
#if defined(_WIN32)
	STARTUPINFOA si;
	PROCESS_INFORMATION pi;
	char argstr[AG_ARG_MAX];
	int  i = 0;

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));

	if(file && strncmp(file, argv[0], strlen(file))) {
		strcpy(argstr, file);
		strcat(argstr, " ");
	} else {
		strcpy(argstr, argv[0]);
		strcat(argstr, " ");
		i++;
	}

	// Add the command-line parameters
	while(argv[i] != NULL) {
		if( (AG_ARG_MAX - strlen(argstr) < strlen(argv[i]) + 1) ) {
			AG_SetError(_("%s: Supplied command arguments exceed AG_ARG_MAX (%d)"), 
				file, AG_ARG_MAX);
			return (-1);
		}
		strcat(argstr, argv[i]);
		strcat(argstr, " ");
		i++;
	}

	if(CreateProcessA(NULL, argstr, NULL, NULL, FALSE, 
						0, NULL, NULL, &si, &pi) == 0) {
		AG_SetError(_("Failed to execute (%s)"), AG_Strerror(GetLastError()));
		return (-1);
	}
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);

	return (pi.dwProcessId);
#elif defined(HAVE_EXECVP)
	AG_ProcessID pid;

	if((pid = fork()) == -1) {
		AG_SetError(_("Fork failed (%s)"), AG_Strerror(errno));
		return (-1);
	} else if(pid == 0) {
		execvp(file, argv);

		// If we get here an error occurred
		_exit(EXIT_FAILURE);
	} else {
		return (pid);
	}
#endif
	AG_SetError("AG_Execute() is not supported on this platform");
	return (-1);
}

AG_ProcessID
AG_WaitOnProcess(AG_ProcessID pid, enum ag_exec_wait_type wait_t)
{
#if defined(_WIN32)
	int time = 0;
	int res;
	DWORD status;
	HANDLE psHandle;

	if(wait_t == AG_EXEC_WAIT_INFINITE) {
		time = INFINITE;
	}

	if((psHandle = OpenProcess(SYNCHRONIZE |
	                           PROCESS_QUERY_INFORMATION,
	                           FALSE, pid)) == NULL) {
		AG_SetError(_("Unable to obtain process handle (%s)"), AG_Strerror(GetLastError()));
		return -1;
	}

	res = WaitForSingleObject(psHandle, time);

	if(res) {
		if(res == WAIT_TIMEOUT) {
			return 0;
		} else if(res == WAIT_FAILED) {
			AG_SetError(_("Wait on process failed (%s)"), AG_Strerror(GetLastError()));
			return -1;
		}
	}

	if(GetExitCodeProcess(psHandle, &status) == 0) {
		AG_SetError(_("Failed to obtain process exit code (%s)"), AG_Strerror(GetLastError()));
		return -1;
	} else if(status) {
		AG_SetError(_("Process exited with status (%d)"), status);
		return -1;
	}

	CloseHandle(psHandle);

	return (pid);
#elif defined(HAVE_EXECVP)
	int res;
	int status;
	int options = 0;

	if(wait_t == AG_EXEC_WAIT_IMMEDIATE) {
		options = WNOHANG;
	}

	res = waitpid(pid, &status, options);

	if(res == -1) {
		AG_SetError(_("waitpid() failed with error (%s)"), AG_Strerror(errno));
		return (-1);
	} else if(res > 0 && status) {
		if(WIFEXITED(status)) {
			AG_SetError(_("Process exited with status (%d)"), WEXITSTATUS(status));
		} else if(WIFSIGNALED(status)) {
			AG_SetError(_("Process terminated by signal (%d)"), WTERMSIG(status));
		} else {
			AG_SetError("Process exited for unknown reason");
		}
		return (-1);
	}
	return (res);
#endif
	AG_SetError("AG_WaitOnProcess() is not supported on this platform");
	return (-1);
}

int
AG_Kill(AG_ProcessID pid)
{
	if(pid <= 0) {
		AG_SetError("Invalid process id");
		return (-1);
	}

#if defined(_WIN32)
	HANDLE psHandle;

	if((psHandle = OpenProcess(SYNCHRONIZE |
	                           PROCESS_TERMINATE |
	                           PROCESS_QUERY_INFORMATION,
	                           FALSE, pid)) == NULL) {
		AG_SetError(_("Unable to obtain process handle (%s)"), AG_Strerror(GetLastError()));
		return -1;
	}

	if(TerminateProcess(psHandle, -1) == 0) {
		AG_SetError(_("Unable to kill process (%s)"), AG_Strerror(GetLastError()));
		return -1;
	}

	CloseHandle(psHandle);

	return (0);
#elif defined(HAVE_EXECVP)
	if(kill(pid, SIGKILL) == -1) {
		AG_SetError(_("Failed to kill process (%s)"), AG_Strerror(errno));
		return (-1);
	}
	return (0);
#endif
	AG_SetError("AG_Kill() is not supported on this platform");
	return (-1);
}