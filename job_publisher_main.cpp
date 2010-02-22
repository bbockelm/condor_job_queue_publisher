#include <iostream>

#include <map>
#include <string>
#include <string.h>
#include <stdlib.h>

#include <unistd.h>
#include <getopt.h>

#include <syslog.h>

#include "JobLogReader.h"
#include "JobPublisherJobLogConsumer.h"

#include "Globals.h"

#include "Config.h"

void Dump();

bool dump = false;

void
usage(char *argv[])
{
	syslog(LOG_ERR,
		   "usage: %s "
		   "--file <job_queue.log> "
		   "[--host <broker host>] "
		   "[--port <broker port>]\n",
		   argv[0]);
	exit(1);
}

void
parse_args(int argc, char *argv[], Config &config)
{
	static struct option options[] = {
		{"host", 1, NULL, 'h'},
		{"port", 1, NULL, 'p'},
		{"file", 1, NULL, 'f'},
		{"dump", 0, NULL, 'd'},
		{NULL, NULL, NULL, NULL}
	};

	int c;
	while (1) {
		c = getopt_long(argc, argv, ":h:p:f:d", options, NULL);
		if (-1 == c) break;

		switch (c) {
		case 'h':
			config.host = optarg;
			break;
		case 'p':
			config.port = (int) strtol(optarg, NULL, 10);
			if (0 == config.port) {
				syslog(LOG_ERR, "invalid port: %s\n", optarg);
				usage(argv);
			}
			break;
		case 'f':
			config.file = optarg;
			break;
		case 'd':
			config.dump = true;
			break;
		case ':':
			syslog(LOG_ERR, "%s requires an argument\n", argv[optind - 1]);
			usage(argv);
			break;
		case '?':
			syslog(LOG_ERR, "unknown argument: %s\n", argv[optind - 1]);
			usage(argv);
			break;
		default:
			usage(argv);
			break;
		}
	}

	if (config.file.empty()) {
		syslog(LOG_ERR, "--file required\n");
		usage(argv);
	}

//	if (optind < argc) {
//		syslog(LOG_ERR, "unknown arguments: ");
//		while (optind < argc) {
//			syslog(LOG_ERR, "%s ", argv[optind++]);
//		}
//		usage(argv);
//	}
}

int main(int argc, char *argv[])
{
	Config config;
	config.host = "localhost";
	config.port = 5672;

	openlog("job_publisher", LOG_PID|LOG_PERROR, LOG_DAEMON);

	parse_args(argc, argv, config);

	syslog(LOG_INFO, "config -- host = %s; port: %d; file: %s\n",
		   config.host.c_str(), config.port, config.file.c_str());

//	closelog();
//	openlog("job_publisher", LOG_PID, LOG_DAEMON);

	JobPublisherJobLogConsumer *consumer = new JobPublisherJobLogConsumer();
	JobLogReader *reader = new JobLogReader(consumer);

	reader->SetJobLogFileName(config.file.c_str());

	while (1) {
		reader->Poll();

		if (dump) {
			Dump();
			consumer->Reset();
			delete reader;
			return 0;
		}

		cout << "Dirty: ";
		for (JobSetType::iterator i = g_dirty_jobs.begin();
			 g_dirty_jobs.end() != i;
			 i++) {
			std::cout << (*i) << " ";
		}
		cout << endl;

		cout << "Delete: ";
		for (JobSetType::iterator i = g_delete_jobs.begin();
			 g_delete_jobs.end() != i;
			 i++) {
			std::cout << (*i) << " ";
		}
		cout << endl;

		sleep(5);
	}

	delete reader;

	return 0;
}

void
Dump()
{
	syslog(LOG_DEBUG, "***BEGIN DUMP***");
	syslog(LOG_DEBUG, "Total number of jobs: %d", g_jobs.size());
/*
	string jobs = "Jobs: ";
	for (JobCollectionType::const_iterator i = g_jobs.begin();
		 g_jobs.end() != i;
		 i++) {
		jobs += (*i).first;
		jobs += " ";
	}
	syslog(LOG_DEBUG, "%s", jobs.c_str());
*/
	string line;
	for (JobCollectionType::const_iterator i = g_jobs.begin();
		 g_jobs.end() != i;
		 i++) {
		for (Job::AttributeMapType::const_iterator j = (*i).second.GetAttributes().begin();
			 (*i).second.GetAttributes().end() != j;
			 j++) {
			line = (*i).first;
			line += " ";
			line += (*j).first;
			line += " ";
			line += (*j).second.GetValue();
			syslog(LOG_DEBUG, "-> %s", line.c_str());
		}
	}
	syslog(LOG_DEBUG, "***END DUMP***");
}
