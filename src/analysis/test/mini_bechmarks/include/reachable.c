#include "reach.h"

int MakeChoice()
{
	INCLUDE_ASM(MAKECHOICE_MAGIC);
	return 0;
}

void reachable() {
	INCLUDE_ASM(REACHABLE_MAGIC);
}

FILE* fopen(const char *path, const char* mode)
{
    INCLUDE_ASM(FOPEN_MAGIC);
    return 0;
}

int fclose(FILE* fp)
{
    INCLUDE_ASM(FCLOSE_MAGIC);
    return 0;
}

my_size_t fread(void* ptr, my_size_t size, my_size_t nmemb, FILE* stream)
{
    INCLUDE_ASM(FREAD_MAGIC);
    return 0;
}

my_size_t fwrite(const void* ptr, my_size_t size, my_size_t nmemb, FILE* stream)
{
    INCLUDE_ASM(FWRITE_MAGIC);
    return 0;
}

long ftell(FILE* stream)
{
    INCLUDE_ASM(FTELL_MAGIC);
    return 0;
}

int fseek(FILE* stream, long offset, int whence)
{
    INCLUDE_ASM(FSEEK_MAGIC);
    return 0;
}

