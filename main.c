#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <blkid/blkid.h>

int backup = 1;
int stdoutput = 0;

void usage(char* filename) {
	printf("Simple fstab generator\n");
	printf("Usage: %s -h/-n/-s\n", filename);
	printf("	-h		Display help message and exit\n");
	printf("	-n		Do not backup fstab\n");
	printf("	-s		Use stdout instead of fstab.\n\n");
}

void makeacopy(char* copyfrom, char* copyto) {
	FILE* src, *dest;
	if((src = fopen(copyfrom, "r")) == NULL) return; // Nothing to backup?
	if((dest = fopen(copyto, "w")) == NULL) {
		printf("Failed to backup %s\n", copyfrom);
		exit(4);
	}
	// https://stackoverflow.com/a/2180157
	struct stat fileinfo = {0};
	fstat(src->_fileno, &fileinfo);
	ssize_t copied = sendfile(dest->_fileno, src->_fileno, NULL, fileinfo.st_size);
	fclose(src);
	fclose(dest);
	if(copied != fileinfo.st_size) {
		printf("Failed to backup %s\n", copyfrom);
		exit(5);
	}
}

// https://stackoverflow.com/a/6811983
int getuuid(char* disk, char* dest) {
	blkid_probe probe;
	const char* uuid;

	probe = blkid_new_probe_from_filename(disk);
	if(!probe) {
		if(!stdoutput) printf("Failed to get UUID for disk %s. It's name will be used instead\n", disk);
		strcpy(dest, disk);
		return 0;
	}

	blkid_do_probe(probe);
	blkid_probe_lookup_value(probe, "UUID", &uuid, NULL);
	strcpy(dest, uuid);
	blkid_free_probe(probe);
	return 1;
}

void stripwrite(FILE* src, FILE* dest) {
	char buf[256];
	char disk[12];
	char uuid[37]; // 36 and 0 byte

	// Not essential but who cares
	memset(buf, 0, sizeof(buf));
	memset(disk, 0, sizeof(disk));
	memset(uuid, 0, sizeof(uuid));
	while(!feof(src)) {
		fgets(buf, 255, src);
		if(buf[0] != '/') continue;

		//while(*buf && *buf != ' ')
			//*disk++ = *buf++;
		
		int i;
		for(i = 0; buf[i] && buf[i] != ' '; i++)
			disk[i] = buf[i];

		// From now on, we have a disk name in disk, offset in i
		// We should now get UUID from libblkid and write UUID=something
		// Then we write the rest of the line

		if(getuuid(disk, uuid)) fputs("UUID=", dest);
		fputs(uuid, dest);
		fputs(buf+i, dest);

		memset(disk, 0, sizeof(disk));
	}

	fputs("tmpfs /tmp tmpfs defaults,nosuid,nodev 0 0\n", dest);
}

int main(int argc, char** argv) {
	for(int i = 1; i < argc; i++) {
		if(!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
			usage(argv[0]);
			return 0;
		}
		else if(!strcmp(argv[i], "-n")) backup = 0;
		else if(!strcmp(argv[i], "-s")) stdoutput = 1;
		else {
			printf("Unknown option: %s\n", argv[i]);
			return 1;
		}
	}

	FILE* mounts, *fstab;
	if((mounts = fopen("/proc/mounts", "r")) == NULL) {
		printf("Failed to open /proc/mounts\n");
		return 2;
	}

	if(stdoutput) fstab = stdout;
	else {
		if(backup) makeacopy("/etc/fstab", "/etc/fstab.bak");
		if((fstab = fopen("/etc/fstab", "w")) == NULL) {
			printf("Failed to open /etc/fstab\n");
			fclose(mounts);
			return 3;
		}
	}

	stripwrite(mounts, fstab);

	fclose(mounts);
	if(!stdoutput) fclose(fstab);
	return 0;
}