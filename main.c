#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int readFile(const char *path, char **source) {
	FILE *file = fopen(path, "rb");
	if (file == NULL) {
		fprintf(stderr, "Khong the mo file: %s\n", path);
		return 0;
	}
	if (fseek(file, 0, SEEK_END) !=0) {
		fclose(file);
		return 0;
	}
	long fileSize = ftell(file);

	if (fileSize < 0) {
		fclose(file);
		return 0;
	}
	rewind(file);
	*source = malloc((size_t)fileSize + 1);

	if (*source == NULL) {
		fclose(file);
		return 0;
	}
	size_t bytesRead = fread(*source, 1, (size_t)fileSize, file);
	fclose(file);
	if (byteRead != (size_t)fileSize) {
		free(*source);
		*source = NULL;
		return 0;
	}
	(*source)[bytesRead] = '\0';
	return 1;
}
const char *demoSource(void) {
	static const char source[] =
	    "var a = 10;\n"
	    "var b = 20;\n"
	    "print a + b * 2;\n"
	    "if (a < b) {\n"
	    "    print a;\n"
	    "}\n"
	    "while (a < 13) {\n"
	    "    a = a + 1;\n"
	    "    print a;\n"
	    "}\n";
	return source;
}
static void printUsage(const char* programName) {
	printf("Cach dung:\n");
	printf(" %s <file.lox>\n", programName);
	printf(" %s --tokens <file.lox>\n", programName);
	printf(" %s --demo\n", programName);
}