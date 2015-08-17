/*
 * Parse NMP Player uploaded log files and generate XML instances for UI charting and reporting.
 * 
 * Usage:
 * logfile version 0.3
 * 
 *     -b -B    --buffer     the size of the buffer in bytes, or in K where specified (e.g. 64K)
 * 
 *     -c -C    --chart      [ daily- | hourly- ] profile-access | system-resources | temperature (not implemented)
 * 
 *     -d -D    --dir        path to logfile directory
 * 
 *     -f -F    --format     [ text | xml ]
 * 
 *     -h -H -? --help       display this help
 * 
 *     -k -K    --skip       skip this many records before starting a page
 * 
 *     -l -L    --limit      limit each page size to this many records
 * 
 *     -n -N    --name       specify logfile name for testing
 * 
 *                           Important Notice: log entries in httpd error_log
 *                           without a timestamp - it is possible - are skipped
 * 
 *     -p -P    --pattern    simple regular expression:
 * 
 *                           c Matches any literal character c
 *                           . (period) Matches any single character
 *                           ^ Matches the beginning of the input string
 *                           $ Matches the end of the input string
 *                           * Matches zero or more occurrences of the previous character
 * 
 *     -r -R    --rank       rank this many top items (not implemented)
 * 
 *     -s -S    --start      start processing at this time
 * 
 *     -t -T    --stop       stop processing at this time (inclusive)
 * 
 *     -u -U    --until      process until this time (exclusive)
 * 
 *     -v       --version    display the version
 * 
 *     -z -Z    --tzoffset   time zone offset
 * 
 */

#include <stdio.h>
#include <sys/times.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <zlib.h> // http://www.zlib.net/manual.html

// Charts
#define PROFILEACCESSCHART 0x1
#define SYSTEMRESOURCESCHART 0x2
#define TEMPERATURECHART 0x4

// Formats
#define TEXTFORMAT 0x1
#define XMLFORMAT 0x2

// http://programmingpraxis.com/2009/09/11/beautiful-code/
// MODIFIED TO CHECK FOR TEXT TERMINATOR '\n' INSTEAD OF '\0'

int match(const char *re, const char *text);
int matchhere(const char *re, const char *text);
int matchstar(int c, const char *re, const char *text);

/*
 * Character Meaning:
 *	 c Matches any literal character c.
 *	 . (period) Matches any single character.
 *	 ^ Matches the beginning of the input string.
 *	 $ Matches the end of the input string.
 *	 * Matches zero or more occurrences of the previous character.
 */

/* match: search for re anywhere in text */
int match(const char *re, const char *text)
{
   if (re[0] == '^')
	  return matchhere(re+1, text);
   do { /* must look at empty string */
	  if (matchhere(re, text))
		 return 1;
   } while (*text++ != '\n');
   return 0;
}

/* matchhere: search for re at beginning of text */
int matchhere(const char *re, const char *text)
{
   if (re[0] == '\0')
	  return 1;
   if (re[1] == '*')
	  return matchstar(re[0], re+2, text);
   if (re[0] == '$' && re[1] == '\0')
	  return *text == '\n';
   if (*text != '\n' && (re[0] == '.' || re[0] == *text))
	  return matchhere(re+1, text+1);
   return 0;
}

/* matchstar: search for c*re at beginning of text */
int matchstar(int c, const char *re, const char *text)
{
   do { /* a * matches zero or more instances */
	  if (matchhere(re, text))
		 return 1;
   } while (*text != '\n' && (*text++ == c || c == '.'));
   return 0;
}

#define BUFLEN 1024
#define VERSION "0.3"

typedef struct Statistics_ {
	long totalBytes;
	long totalLength;
	long totalSearchBytes;
	long totalSearchLength;
	char elapsed[64];
	char user_time[64];
	char system_time[64];
	clock_t start_time;
	clock_t stop_time;
	struct tms start_cpu;
	struct tms stop_cpu;
} Statistics;

typedef struct Record_ {
	long count;
} Record;

typedef struct Command_ {
	// Application Arguments
	long buffer_size;
	long chart;
	char dir[BUFLEN+1];
	long format;
	long limit;
	char name[BUFLEN+1];
	char name_format[BUFLEN+1];
	char pattern[BUFLEN+1];
	long rank;
	long skip;
	time_t start_time;
	time_t stop_time;
	char tzoffset[6+1];
	
	// Application Variables
	int count, index;
	char *buffer;
// 	FILE *idx_file;
// 	FILE *log_file;
	long marker;
	size_t offset;
	size_t offset_size;
	time_t time;
	struct tm timestamp;
	long timezoneoffset;
	long value;
	
	// Ragel Variables
	char buf[BUFLEN+1];
	int buflen;
	int cs;
} Command;

void print_begin_xml() {
	printf("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<instance>\n");
}

void print_begin_files_xml() {
	printf("\t<properties key=\"files\">\n");
}

void print_end_xml() {
	printf("</instance>\n");
}

void print_end_files_xml() {
	printf("\t</properties>\n");
}

void print_file_xml(const char *name, long marker) {
	printf("\t\t<instance>\n");
	printf("\t\t\t<property key=\"name\">\n\t\t\t\t<string>%s</string>\n\t\t\t</property>\n", name);
	printf("\t\t\t<property key=\"marker\">\n\t\t\t\t<integer>%ld</integer>\n\t\t\t</property>\n", marker);
	printf("\t\t</instance>\n");
}

void print_record_xml(Record *rec, Command *cmd) {
	struct tm *timestamp = gmtime(&cmd->time);
	printf("\t\t<instance>\n\t\t\t<property key=\"date\">\n\t\t\t\t<time>%4d-%02d-%02d %02d:%02d:%02d%s%s</time>\n\t\t\t</property>\n", 
		timestamp->tm_year + 1900, timestamp->tm_mon + 1, timestamp->tm_mday, 
		timestamp->tm_hour, timestamp->tm_min, timestamp->tm_sec, 
		cmd->timezoneoffset ? " " : "", cmd->tzoffset);
	printf("\t\t\t<property key=\"count\">\n\t\t\t\t<integer>%ld</integer>\n\t\t\t</property>\n", rec->count);
	printf("\t\t</instance>\n");
}

void print_records_xml(Record *rec, Command *cmd) {
	printf("\t<properties key=\"records\">\n");
	for (cmd->index = 0; cmd->index < cmd->count; cmd->index++) {
		cmd->time = ((((cmd->start_time + cmd->timezoneoffset) / cmd->value) + cmd->index) * cmd->value);
		print_record_xml(&rec[cmd->index], cmd);
	}
	printf("\t</properties>\n");
}

void print_statistics_xml(Statistics *stat) {
	printf("\t<property key=\"statistics\">\n\t\t<instance>\n");
	printf("\t\t\t<property key=\"totalBytes\">\n\t\t\t\t<integer>%ld</integer>\n\t\t\t</property>\n", stat->totalBytes);
	printf("\t\t\t<property key=\"totalLength\">\n\t\t\t\t<integer>%ld</integer>\n\t\t\t</property>\n", stat->totalLength);
	printf("\t\t\t<property key=\"totalSearchBytes\">\n\t\t\t\t<integer>%ld</integer>\n\t\t\t</property>\n", stat->totalSearchBytes);
	printf("\t\t\t<property key=\"totalSearchLength\">\n\t\t\t\t<integer>%ld</integer>\n\t\t\t</property>\n", stat->totalSearchLength);
	printf("\t\t\t<property key=\"elapsed\">\n\t\t\t\t<string>%s</string>\n\t\t\t</property>\n", stat->elapsed);
	printf("\t\t\t<property key=\"systemTime\">\n\t\t\t\t<string>%s</string>\n\t\t\t</property>\n", stat->system_time);
	printf("\t\t\t<property key=\"userTime\">\n\t\t\t\t<string>%s</string>\n\t\t\t</property>\n", stat->user_time);
	printf("\t\t</instance>\n\t</property>\n");
}

void reset_record(Record *rec)
{
	rec->count = 0;
}

void start_clock(Statistics *stat)
{
	stat->start_time = times(&stat->start_cpu);
}

void stop_clock(Statistics *stat)
{
	long clock_ticks, clock_ticks_per_second = sysconf(_SC_CLK_TCK);
	
	stat->stop_time = times(&stat->stop_cpu);
	clock_ticks = stat->stop_time - stat->start_time;
	clock_ticks = (clock_ticks || stat->totalSearchLength == 0) ? clock_ticks : 1; // no 0.00s display
	sprintf(stat->elapsed, "%ld.%02lds", clock_ticks / clock_ticks_per_second, clock_ticks % clock_ticks_per_second);
	clock_ticks = stat->stop_cpu.tms_utime - stat->start_cpu.tms_utime;
	clock_ticks = (clock_ticks || stat->totalSearchLength == 0) ? clock_ticks : 1; // no 0.00s display
	sprintf(stat->user_time, "%ld.%02lds", clock_ticks / clock_ticks_per_second, clock_ticks % clock_ticks_per_second);
	clock_ticks = stat->stop_cpu.tms_stime - stat->start_cpu.tms_stime;
	clock_ticks = (clock_ticks || stat->totalSearchLength == 0) ? clock_ticks : 1; // no 0.00s display
	sprintf(stat->system_time, "%ld.%02lds", clock_ticks / clock_ticks_per_second, clock_ticks % clock_ticks_per_second);
}

#include "params.c" // ragel params.rl
#include "parser.c" // ragel parser.rl

int main(int argc, char **argv)
{
	Command cmd;
	int result;
	
	result = params(&cmd, argc, argv);
	if (result == 0) {
		result = parser(&cmd);
	}
	params_cleanup(&cmd);
	return result;
}
