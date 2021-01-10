// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id$
//
// Copyright (C) 2006-2020 by The Odamex Team.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
//
// DESCRIPTION:
//   POSIX Crash handling.
//
//-----------------------------------------------------------------------------

#if defined UNIX && !defined GEKKO

#define CRASH_DIR_LEN 1024

#include "i_crash.h"

#include <csignal>
#include <cstdio>
#include <cstring>

#include <execinfo.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#include "i_system.h"

/**
 * @brief An array containing the directory where crashes are written to.
 */
static char gCrashDir[CRASH_DIR_LEN];

// Write a backtrace to a file.
//
// This is not a "safe" signal handler, but this is used in a process
// that is already crashing, and is meant to provide as much
// information as reasonably possible in the potential absence of a
// core dump.
static void WriteBacktrace(int sig, siginfo_t* si)
{
	// Generate a timestamp.
	time_t now = time(NULL);
	struct tm* local = localtime(&now);

	char timebuf[1024];
	strftime(timebuf, ARRAY_LENGTH(timebuf), "%Y%m%d_%H%M%S", local);

	// Find the spot to write our backtrace.
	int len = 0;
	char filename[CRASH_DIR_LEN];
	len = snprintf(filename, sizeof(filename), "%s/odamex_%s_dump.txt", ::gCrashDir,
	               timebuf);
	if (len < 0)
	{
		fprintf(stderr, "%s: File path too long.\n", __FUNCTION__);
		return;
	}

	// Create a file.
	int fd = creat(filename, 0644);
	if (fd == -1)
	{
		fprintf(stderr, "%s: File could not be created.\n", __FUNCTION__);
		return;
	}

	// Stamp out the header.
	char buf[1024];
	len = snprintf(buf, sizeof(buf),
	               "Signal number: %d\nErrno: %d\nSignal code: %d\nFault Address: %p\n",
	               si->si_signo, si->si_errno, si->si_code, si->si_addr);
	if (len < 0)
	{
		fprintf(stderr, "%s: Header too long for buffer.\n", __FUNCTION__);
		return;
	}

	// Write the header.
	ssize_t writelen = write(fd, buf, len);
	if (writelen < 1)
	{
		fprintf(stderr, "%s: Failed to write to fd.\n", __FUNCTION__);
		return;
	}

	// Get backtrace data.
	void* bt[50];
	size_t size = backtrace(bt, ARRAY_LENGTH(bt));

	// Write stack frames to file.
	backtrace_symbols_fd(bt, size, fd);
	close(fd);

	// Tell the user about it.
	fprintf(stderr, "Wrote \"%s\".\n", filename);
}

static void SigActionCallback(int sig, siginfo_t* si, void* ctx)
{
	fprintf(stderr, "Caught Signal %d (%s), dumping crash info...\n", si->si_signo,
	        strsignal(si->si_signo));

	// Change our signal handler back to default.
	struct sigaction act;
	std::memset(&act, 0, sizeof(act));
	act.sa_handler = SIG_DFL;

	sigaction(SIGQUIT, &act, NULL);
	sigaction(SIGILL, &act, NULL);
	sigaction(SIGABRT, &act, NULL);
	sigaction(SIGFPE, &act, NULL);
	sigaction(SIGSEGV, &act, NULL);
	sigaction(SIGBUS, &act, NULL);

	// Write out the backtrace
	WriteBacktrace(sig, si);

	// Once we're done, re-raise the signal.
	kill(getpid(), sig);
}

void I_SetCrashCallbacks()
{
	struct sigaction act;
	std::memset(&act, 0, sizeof(act));

	act.sa_sigaction = &SigActionCallback;
	act.sa_flags = SA_SIGINFO;

	sigaction(SIGQUIT, &act, NULL);
	sigaction(SIGILL, &act, NULL);
	sigaction(SIGABRT, &act, NULL);
	sigaction(SIGFPE, &act, NULL);
	sigaction(SIGSEGV, &act, NULL);
	sigaction(SIGBUS, &act, NULL);
}

void I_SetCrashDir(const char* crashdir)
{
	std::string homedir;
	char testfile[CRASH_DIR_LEN];

	// Check to see if our crash dir is too big.
	size_t len = strlen(crashdir);
	if (len > CRASH_DIR_LEN)
	{
		I_FatalError(
		    "Crash directory is too long.\nPlease pass a correct -crashout param.");
		std::abort();
	}

	// Check to see if we can write to our crash directory.
	snprintf(testfile, ARRAY_LENGTH(testfile), "%s/crashXXXXXX", crashdir);
	int res = mkstemp(testfile);
	if (res == -1)
	{
		I_FatalError("Crash directory is not writable.\nPlease point -crashout to "
		             "a directory with write permissions.");
		std::abort();
	}

	// We don't need the temporary file anymore.
	remove(testfile);

	// Copy the crash directory.
	memcpy(::gCrashDir, crashdir, len);
}

#endif
