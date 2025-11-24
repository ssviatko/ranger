#include <stdio.h>

#include "carith.h"

int main(int argc, char **argv)
{
	printf("carith build %s release %s\nbuilt on %s\n", BUILD_NUMBER, RELEASE_NUMBER, BUILD_DATE);
	return 0;
}

