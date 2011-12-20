/***************************************************************
 *
 * Copyright (C) 2010 Red Hat, Inc.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#include <iostream>

#include <map>
#include <string>
#include <string.h>
#include <stdlib.h>

#include <unistd.h>
#include <getopt.h>

#include <syslog.h>

#include <errno.h>

#include <qpid/messaging/Connection.h>
#include <qpid/messaging/Message.h>
#include <qpid/messaging/Sender.h>
#include <qpid/messaging/Session.h>

#include "ClassAdLogReader.h"
#include "JobQueuePublisherClassAdLogConsumer.h"

#include "Globals.h"

#include "Config.h"

using namespace qpid::messaging;

void Dump();

void PublishJob(const string &key, Sender &sender);

void
usage(char *argv[])
{
	syslog(LOG_ERR,
		   "usage: %s "
		   "--file <job_queue.log> "
		   "[--daemon] "
		   "[--trigger <attribute name>]* "
		   "[--broker <broker url: amqp:tcp:127.0.0.1:5672>] "
		   "[--address <queue or topic>] "
		   "[--interval <poll interval: 15>]\n",
		   argv[0]);
	exit(1);
}

void
parse_args(int argc, char *argv[], Config &config)
{
	static struct option options[] = {
		{"file", 1, NULL, 'f'},
		{"daemon", 0, NULL, 'd'},
		{"trigger", 1, NULL, 't'},
		{"broker", 1, NULL, 'b'},
		{"address", 1, NULL, 'a'},
		{"interval", 1, NULL, 'i'},
		{"dump", 0, NULL, 'u'},
		{NULL, NULL, NULL, NULL}
	};

	int c;
	while (1) {
		c = getopt_long(argc, argv, ":fd:t:b:a:i:u", options, NULL);
		if (-1 == c) break;

		switch (c) {
		case 'f':
			config.file = optarg;
			break;
		case 'd':
			config.daemon = true;
			break;
		case 't':
			config.triggers.insert(optarg);
			break;
		case 'b':
			config.broker = optarg;
			break;
		case 'a':
			config.address = optarg;
			break;
		case 'i':
			config.interval = (int) strtol(optarg, NULL, 10);
			if (0 > config.interval) {
				syslog(LOG_ERR, "invalid interval, must be >=0: %s", optarg);
				usage(argv);
			}
			break;
		case 'u':
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
	Sender sender;
	Connection connection;
	Session session;

	config.broker = "amqp:tcp:127.0.0.1:5672";
	config.interval = 15;
	config.daemon = false;

	openlog("job_queue_publisher", LOG_PID|LOG_PERROR, LOG_DAEMON);

	parse_args(argc, argv, config);

	stringstream triggers;
	for (Config::TriggersType::const_iterator i = config.triggers.begin();
		 config.triggers.end() != i;
		 i++) {
		triggers << (*i) << " ";
	}
	if (triggers.str().empty()) {
		triggers << "*";
	}
	syslog(LOG_INFO,
		   "config -- file = %s; daemon = %s; broker = %s; address: %s; triggers: %s\n",
		   config.file.c_str(),
		   config.daemon ? "yes" : "no",
		   config.broker.c_str(),
		   config.address.c_str(),
		   triggers.str().c_str());

	if (config.daemon && daemon(0, 0)) {
		syslog(LOG_ERR, "daemon(0,0) failed: errno=%d (%m)", errno);
		exit(1);
	}

	if (!config.address.empty()) {
		connection.open(config.broker);
		session = connection.newSession();
		sender = session.createSender(config.address);
	}

	JobQueuePublisherClassAdLogConsumer *consumer =
		new JobQueuePublisherClassAdLogConsumer();
	ClassAdLogReader *reader = new ClassAdLogReader(consumer);

	reader->SetClassAdLogFileName(config.file.c_str());

	while (1) {
		switch (reader->Poll()) {
		case POLL_FAIL:
			syslog(LOG_ERR, "Polling failed, ignoring, will try again");
			break;
		case POLL_ERROR:
			syslog(LOG_ERR, "Polling error, exiting");
			exit(1);
		}

		for (JobSetType::const_iterator i = g_dirty_jobs.begin();
			 g_dirty_jobs.end() != i;
			 i++) {
			PublishJob((*i), sender);
		}
		g_dirty_jobs.clear();

			// XXX TODO XXX
			// This is a hack to delete child jobs before parent
			// jobs. Fix properly with a shared_ptr.
		for (JobSetType::const_reverse_iterator i = g_delete_jobs.rbegin();
			 g_delete_jobs.rend() != i;
			 i++) {
			PublishJob((*i), sender);
			g_jobs.erase((*i));
		}
		g_delete_jobs.clear();

		if (config.dump) {
			Dump();
			if (!config.address.empty()) {
				Message message;
				message.setSubject("DONE");
				sender.send(message);
			}
			break;
		}

		sleep(config.interval);
	}

	consumer->Reset();
	delete reader;

	if (!config.address.empty()) {
		session.sync();
		connection.close();
	}

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
	for (JobCollectionType::const_iterator i = g_jobs.begin();
		 g_jobs.end() != i;
		 i++) {
		stringstream line;
		for (Job::AttributeMapType::const_iterator j = (*i).second.GetAttributes().begin();
			 (*i).second.GetAttributes().end() != j;
			 j++) {
			line.str("");
			line << (*i).first << " " << (*j).first << " " << (*j).second;
			syslog(LOG_DEBUG, "-> %s", line.str().c_str());
		}

		Job::AttributeMapType attrs;
		attrs = (*i).second.GetParentAttributes();
		for (Job::AttributeMapType::const_iterator attr =
				 (*i).second.GetAttributes().begin();
			 (*i).second.GetAttributes().end() != attr;
			 attr++) {
			attrs[(*attr).first] = (*attr).second;
		}
		for (Job::AttributeMapType::const_iterator j = attrs.begin();
			 attrs.end() != j;
			 j++) {
			line.str("");
			line << (*i).first << " " << (*j).first << " " << (*j).second;
			syslog(LOG_DEBUG, "=> %s", line.str().c_str());
		}
	}
	syslog(LOG_DEBUG, "***END DUMP***");
}

void
PublishJob(const string &key, Sender &sender)
{
	if (!config.address.empty()) {
		Message message;
		MapContent content(message);

		JobCollectionType::const_iterator job = g_jobs.find(key);
		assert(g_jobs.end() != job);

		message.setSubject(key);

		for (Job::AttributeMapType::const_iterator attr =
				 (*job).second.GetParentAttributes().begin();
			 (*job).second.GetParentAttributes().end() != attr;
			 attr++) {
			content[(*attr).first] = (*attr).second;
		}

		for (Job::AttributeMapType::const_iterator attr =
				 (*job).second.GetAttributes().begin();
			 (*job).second.GetAttributes().end() != attr;
			 attr++) {
			content[(*attr).first] = (*attr).second;
		}

		content.encode();
		sender.send(message);
	}
}