/*
 * Build:
 * gcc -O2 -lz test.c -o test.bin
 * 
 * ./test.bin
 * 
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include "zlib.h" // http://www.zlib.net/manual.html

#define BUFSIZE 4096
char buf[BUFSIZE];

int main (int argc, char *argv[])
{
	gzFile *file;
	char *string;
	time_t start, stop;
	struct tm *time, time_stamp;
	
	struct dirent **directory, **directory2;
	int nentries, entry, nentries2, entry2;
	char *dir = "/var/opt/nmp2/player/log";
	char path[1024], type[1024];
	
	if (chdir(dir) == 0) {
		nentries = scandir(".", &directory, 0, alphasort);
		if (nentries >= 0) {
			for (entry = 0; entry < nentries; entry++) {
				//printf("%s\n", directory[entry]->d_name);
				if (strcmp(directory[entry]->d_name, ".") == 0) {
					continue;
				}
				if (strcmp(directory[entry]->d_name, "..") == 0) {
					continue;
				}
				printf("mkdir -p /var/opt/nmp2/player/%s/nmpb-client\n", directory[entry]->d_name);
				printf("mkdir -p /var/opt/nmp2/player/%s/nmpb-installer\n", directory[entry]->d_name);
				printf("mkdir -p /var/opt/nmp2/player/%s/nmpb-player\n", directory[entry]->d_name);
				nentries2 = scandir(directory[entry]->d_name, &directory2, 0, alphasort);
				if (nentries2 >= 0) {
					for (entry2 = 0; entry2 < nentries2; entry2++) {
						//printf("%s\n", directory2[entry2]->d_name);
						if (strcmp(directory2[entry2]->d_name, ".") == 0) {
							continue;
						}
						if (strcmp(directory2[entry2]->d_name, "..") == 0) {
							continue;
						}
						strcpy(path, directory[entry]->d_name);
						strcat(path, "/");
						strcat(path, directory2[entry2]->d_name);
						strcpy(type, directory2[entry2]->d_name);
						string = strchr(type, '.');
						if (string != NULL) {
							*string = '\0';
							//printf("%s\n", path);
							file = gzopen(path, "rb");
							if (file != Z_NULL) {
								start = -1;
								while (1) {
									string = gzgets(file, buf, BUFSIZE);
									if (string == Z_NULL) {
										break;
									}
									if (sscanf(string, "%4d-%2d-%2d%2d:%2d:%2d", 
										&time_stamp.tm_year, &time_stamp.tm_mon, &time_stamp.tm_mday, 
										&time_stamp.tm_hour, &time_stamp.tm_min, &time_stamp.tm_sec) == 6) {
										time_stamp.tm_year -= 1900;
										time_stamp.tm_mon -= 1;
										time_stamp.tm_isdst = 0;
										time_stamp.tm_isdst = 0;
										//time_stamp.tm_usec = 0;
										time_stamp.tm_gmtoff = 0;
										stop = timegm(&time_stamp);
										if (start == -1) {
											start = stop;
										}
									}
								};
								gzclose(file);
								time = gmtime(&start);
								printf("cp %s/%s /var/opt/nmp2/player/%s/%s/", dir, path, directory[entry]->d_name, type);
								printf("%4d%02d%02d%02d%02d%02d-", time->tm_year + 1900, time->tm_mon + 1, 
									time->tm_mday, time->tm_hour, time->tm_min, time->tm_sec);
								time = gmtime(&stop);
								printf("%4d%02d%02d%02d%02d%02d.%s.log.gz\n", time->tm_year + 1900, time->tm_mon + 1, 
									time->tm_mday, time->tm_hour, time->tm_min, time->tm_sec, type);
							}
						}
					}
					free(directory2);
				}
				else {
					fprintf(stderr, "logfile: could not scan directory (%s)\n", directory[entry]->d_name);
				}
			}
			free(directory);
		}
		else {
			fprintf(stderr, "logfile: could not scan directory (%s)\n", dir);
		}
	}
	else {
		fprintf(stderr, "logfile: could not change directory (%s)\n", dir);
	}
	
	return 0;
}
