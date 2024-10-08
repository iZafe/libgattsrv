// Copyright 2017-2019 Paul Nettle
//
// This file is part of Gobbledegook.
//
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file in the root of the source tree.

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// >>
// >>>  INSIDE THIS FILE
// >>
//
// This is an example single-file stand-alone application that runs a Gobbledegook server.
//
// >>
// >>>  DISCUSSION
// >>
//
// Very little is required ("MUST") by a stand-alone application to instantiate a valid Gobbledegook server. There are also some
// things that are reocommended ("SHOULD").
//
// * A stand-alone application MUST:
//
//     * Start the server via a call to `ggkStart()`.
//
//         Once started the server will run on its own thread.
//
//         Two of the parameters to `ggkStart()` are delegates responsible for providing data accessors for the server, a
//         `GGKServerDataGetter` delegate and a 'GGKServerDataSetter' delegate. The getter method simply receives a string name (for
//         example, "battery/level") and returns a void pointer to that data (for example: `(void *)&batteryLevel`). The setter does
//         the same only in reverse.
//
//         While the server is running, you will likely need to update the data being served. This is done by calling
//         `ggkNofifyUpdatedCharacteristic()` or `ggkNofifyUpdatedDescriptor()` with the full path to the characteristic or delegate
//         whose data has been updated. This will trigger your server's `onUpdatedValue()` method, which can perform whatever
//         actions are needed such as sending out a change notification (or in BlueZ parlance, a "PropertiesChanged" signal.)
//
// * A stand-alone application SHOULD:
//
//     * Shutdown the server before termination
//
//         Triggering the server to begin shutting down is done via a call to `ggkTriggerShutdown()`. This is a non-blocking method
//         that begins the asynchronous shutdown process.
//
//         Before your application terminates, it should wait for the server to be completely stopped. This is done via a call to
//         `ggkWait()`. If the server has not yet reached the `EStopped` state when `ggkWait()` is called, it will block until the
//         server has done so.
//
//         To avoid the blocking behavior of `ggkWait()`, ensure that the server has stopped before calling it. This can be done
//         by ensuring `ggkGetServerRunState() == EStopped`. Even if the server has stopped, it is recommended to call `ggkWait()`
//         to ensure the server has cleaned up all threads and other internals.
//
//         If you want to keep things simple, there is a method `ggkShutdownAndWait()` which will trigger the shutdown and then
//         block until the server has stopped.
//
//     * Implement signal handling to provide a clean shut-down
//
//         This is done by calling `ggkTriggerShutdown()` from any signal received that can terminate your application. For an
//         example of this, search for all occurrences of the string "signalHandler" in the code below.
//
//     * Register a custom logging mechanism with the server
//
//         This is done by calling each of the log registeration methods:
//
//             `ggkLogRegisterDebug()`
//             `ggkLogRegisterInfo()`
//             `ggkLogRegisterStatus()`
//             `ggkLogRegisterWarn()`
//             `ggkLogRegisterError()`
//             `ggkLogRegisterFatal()`
//             `ggkLogRegisterAlways()`
//             `ggkLogRegisterTrace()`
//
//         Each registration method manages a different log level. For a full description of these levels, see the header comment
//         in Logger.cpp.
//
//         The code below includes a simple logging mechanism that logs to stdout and filters logs based on a few command-line
//         options to specify the level of verbosity.
//
// >>
// >>>  Building with GOBBLEDEGOOK
// >>
//
// The Gobbledegook distribution includes this file as part of the Gobbledegook files with everything compiling to a single, stand-
// alone binary. It is built this way because Gobbledegook is not intended to be a generic library. You will need to make your
// custom modifications to it. Don't worry, a lot of work went into Gobbledegook to make it almost trivial to customize
// (see Server.cpp).
//
// If it is important to you or your build process that Gobbledegook exist as a library, you are welcome to do so. Just configure
// your build process to build the Gobbledegook files (minus this file) as a library and link against that instead. All that is
// required by applications linking to a Gobbledegook library is to include `include/Gobbledegook.h`.
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#include <signal.h>
#include <iostream>
#include <thread>
#include <sstream>

#include "../include/Gobbledegook.h"


//
// Logging
//

enum LogLevel
{
	Debug,
	Verbose,
	Normal,
	ErrorsOnly
};

// Our log level - defaulted to 'Normal' but can be modified via command-line options
LogLevel logLevel = Normal;

// Our full set of logging methods (we just log to stdout)
//
// NOTE: Some methods will only log if the appropriate `logLevel` is set
void LogDebug(const char *pText) { if (logLevel <= Debug) { std::cout << "  DEBUG: " << pText << std::endl; } }
void LogInfo(const char *pText) { if (logLevel <= Verbose) { std::cout << "   INFO: " << pText << std::endl; } }
void LogStatus(const char *pText) { if (logLevel <= Normal) { std::cout << " STATUS: " << pText << std::endl; } }
void LogWarn(const char *pText) { std::cout << "WARNING: " << pText << std::endl; }
void LogError(const char *pText) { std::cout << "!!ERROR: " << pText << std::endl; }
void LogFatal(const char *pText) { std::cout << "**FATAL: " << pText << std::endl; }
void LogAlways(const char *pText) { std::cout << "..Log..: " << pText << std::endl; }
void LogAlways8(uint8_t value) { std::cout << "..Log.. <uint32_t>: " << value << std::endl; }
void LogAlways16(uint16_t value) { std::cout << "..Log.. <uint64_t>: " << value << std::endl; }
void LogAlways32(uint32_t value) { std::cout << "..Log.. <uint32_t>: " << value << std::endl; }
void LogAlways64(uint64_t value) { std::cout << "..Log.. <uint64_t>: " << value << std::endl; }
void LogTrace(const char *pText) { std::cout << "-Trace-: " << pText << std::endl; }

//
// Signal handling
//

// We setup a couple Unix signals to perform graceful shutdown in the case of SIGTERM or get an SIGING (CTRL-C)
void signalHandler(int signum)
{
	switch (signum)
	{
		case SIGINT:
			LogStatus("SIGINT recieved, shutting down");
			ggkTriggerShutdown();
			break;
		case SIGTERM:
			LogStatus("SIGTERM recieved, shutting down");
			ggkTriggerShutdown();
			break;
	}
}


//
// Constants
//

// Maximum time to wait for any single async process to timeout during initialization
static const int kMaxAsyncInitTimeoutMS = 30 * 1000;

//
// Server data values
//

// The battery level ("battery/level") reported by the server (see Server.cpp)
static uint8_t serverDataBatteryLevel = 78;

// The text string ("text/string") used by our custom text string service (see Server.cpp)
static std::string serverDataTextString = "Hello, world! Maybe it's to loong for us";
static std::string careToken = "0";// "GAKuZPRcL1";
static std::string careTokenSetter = "";// "GAKuZPRcL1";//
static std::string strAUTH = "1894573873000031214000";
static std::string firstname = "Piotrek";
static std::string lastName = "Kundu";
static unsigned char strAUTHArray[] = {
    0x18, 0x94, 0x57, 0x38, 0x73, 
    0x00, 0x00, 0x31, 0x21, 0x40, 0x00
};

static uint64_t status = 0x1A; //(3<<3 for full battery);
static uint64_t currentTime = 0x1a1b2c3d4e5f600f;
static uint32_t birthday = 267462000; // Represents 12:00 PM UTC on August 27, 2024
static uint32_t dispense_lastdate = 1724740800; // Represents 12:00 PM UTC on August 27, 2024
static uint32_t dispense_nextttime = 1724827200; // +1 day (Represents 12:00 PM UTC on August 28, 2024)
static std::string strFirstDispense = "2024-08-20T20:00:20Z"; //[YYYY-MM-DDTHH:MM:DD.00Z]
static uint32_t control = 0;
static uint8_t dispense_daysbeforelastdispensealert= 0;
static uint8_t dispense_daysbeforelastdispensenotification = 0;
static uint16_t uncollected_minutesbefore = 60;

//
// Server data management
//

// Called by the server when it wants to retrieve a named value
//
// This method conforms to `GGKServerDataGetter` and is passed to the server via our call to `ggkStart()`.
//
// The server calls this method from its own thread, so we must ensure our implementation is thread-safe. In our case, we're simply
// sending over stored values, so we don't need to take any additional steps to ensure thread-safety.
const void *dataGetter(const char *pName)
{
	LogAlways("####     pName: ");
	LogAlways(pName);

	if (nullptr == pName)
	{
		LogError("NULL name sent to server data getter");
		return nullptr;
	}

	std::string strName = pName;
	if (strName == "status")
	{
		LogAlways64(status);
		return &status;
	}
	else if (strName == "current/time")
	{
		LogAlways64(currentTime);
		return &currentTime;
	}
	else if (strName == "birthday")
	{
		LogAlways32(birthday);
		return &birthday;
	}
	else if (strName == "dispense/nexttime")
	{
		LogAlways32(dispense_nextttime);
		return &dispense_nextttime;
	}
	else if (strName == "dispense/lastdate")
	{
		LogAlways32(dispense_lastdate);
		return &dispense_lastdate;
	}
	else if (strName == "authentication/id")
	{
		// std::string strAuthenticationID = "89457387300003121400";
		// std::string hex_str = strAuthenticationID.size() == 19 ? "0" : "1";
		// hex_str += strAuthenticationID;
		// hex_str += strAuthenticationID.size() == 19 ? "" : "0";   // Make sure there are n*2 hex digits
		return &strAUTHArray;
	}
	else if (strName == "caregiver/token")
	{
		std::string local = "GAKuZPRcL1";
		if (careTokenSetter  == local)
		{
			careToken = "1";
			LogAlways("authenticated YES");
		} 
		else
		{
			LogAlways("authenticated NO !!!!!!!!!!!!!!!!!!!!!!!!1");
		}
		LogAlways(careToken.c_str());
		return careToken.c_str();
	}
	else if (strName == "dispense/first")
	{
		LogAlways(strFirstDispense.c_str());
		return strFirstDispense.c_str();
	}
	else if (strName == "name/first")
	{
		LogAlways(firstname.c_str());
		return firstname.c_str();
	}
	else if (strName == "name/last")
	{
		LogAlways(lastName.c_str());
		return lastName.c_str();
	}
	else if (strName == "control")
	{
		LogAlways64(control);
		return &control;
	}
	else if (strName == "dispense/daysbeforelastdispensealert")
	{
		LogAlways8(dispense_daysbeforelastdispensealert);
		return &dispense_daysbeforelastdispensealert;
	}
	else if (strName == "dispense/daysbeforelastdispensenotification")
	{
		LogAlways8(dispense_daysbeforelastdispensenotification);
		return &dispense_daysbeforelastdispensenotification;
	}
	else if (strName == "uncollected/minutesbefore")
	{
		LogAlways16(uncollected_minutesbefore);
		return &uncollected_minutesbefore;
	}
	LogWarn((std::string("Unknown name for server data getter request: '") + pName + "'").c_str());
	return nullptr;
}

// Called by the server when it wants to update a named value
//
// This method conforms to `GGKServerDataSetter` and is passed to the server via our call to `ggkStart()`.
//
// The server calls this method from its own thread, so we must ensure our implementation is thread-safe. In our case, we're simply
// sending over stored values, so we don't need to take any additional steps to ensure thread-safety.
int dataSetter(const char *pName, const void *pData)
{
	if (nullptr == pName)
	{
		LogError("NULL name sent to server data setter");
		return 0;
	}
	if (nullptr == pData)
	{
		LogError("NULL pData sent to server data setter");
		return 0;
	}

	std::string strName = pName;

	if (strName == "status")
	{
		serverDataBatteryLevel = *static_cast<const uint8_t *>(pData);
		LogDebug((std::string("Server data: battery level set to ") + std::to_string(serverDataBatteryLevel)).c_str());
		return 1;
	}
	else if (strName == "text/string")
	{
		serverDataTextString = static_cast<const char *>(pData);
		LogDebug((std::string("Server data: text string set to '") + serverDataTextString + "'").c_str());
		return 1;
	}
	else if (strName == "caregiver/token")
	{
		careTokenSetter = static_cast<const char *>(pData);
		LogDebug((std::string("careTokenSetter data: text string set to '") + careTokenSetter + "'").c_str());
		return 1;
	}

	LogWarn((std::string("Unknown name for server data setter request: '") + pName + "'").c_str());

	return 0;
}

//
// Entry point
//

int main(int argc, char **ppArgv)
{
	// A basic command-line parser
	for (int i = 1; i < argc; ++i)
	{
		std::string arg = ppArgv[i];
		if (arg == "-q")
		{
			logLevel = ErrorsOnly;
		}
		else if (arg == "-v")
		{
			logLevel = Verbose;
		}
		else if  (arg == "-d")
		{
			logLevel = Debug;
		}
		else
		{
			LogFatal((std::string("Unknown parameter: '") + arg + "'").c_str());
			LogFatal("");
			LogFatal("Usage: standalone [-q | -v | -d]");
			return -1;
		}
	}

	// Setup our signal handlers
	signal(SIGINT, signalHandler);
	signal(SIGTERM, signalHandler);

	// Register our loggers
	ggkLogRegisterDebug(LogDebug);
	ggkLogRegisterInfo(LogInfo);
	ggkLogRegisterStatus(LogStatus);
	ggkLogRegisterWarn(LogWarn);
	ggkLogRegisterError(LogError);
	ggkLogRegisterFatal(LogFatal);
	ggkLogRegisterAlways(LogAlways);
	ggkLogRegisterTrace(LogTrace);

	// Start the server's ascync processing
	//
	// This starts the server on a thread and begins the initialization process
	//
	// !!!IMPORTANT!!!
	//
	//     This first parameter (the service name) must match tha name configured in the D-Bus permissions. See the Readme.md file
	//     for more information.
	//
	// first params must match the name in /etc/dbus-1/system.d/com.dosell.v3.conf and may not include dot eg. dosell.v3 is NOT valid
	if (!ggkStart("dosell", "Dosell", "Dosell", dataGetter, dataSetter, kMaxAsyncInitTimeoutMS))
	{
		return -1;
	}

	// Wait for the server to start the shutdown process
	//
	// While we wait, every 15 ticks, drop the battery level by one percent until we reach 0
	while (ggkGetServerRunState() < EStopping)
	{
		std::this_thread::sleep_for(std::chrono::seconds(15));

		serverDataBatteryLevel = std::max(serverDataBatteryLevel - 1, 0);
		//ggkNofifyUpdatedCharacteristic("/com/dosell/service/1/status");
	}

	// Wait for the server to come to a complete stop (CTRL-C from the command line)
	if (!ggkWait())
	{
		return -1;
	}

	// Return the final server health status as a success (0) or error (-1)
  	return ggkGetServerHealth() == EOk ? 0 : 1;
}
