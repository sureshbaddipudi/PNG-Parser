#include "PNGParser.h"


int main( int argc, char *argv[] )
{
	int parsed = FALSE;
	if (argc < 2) {
		printf( "Usage: PNGParser <file_name>\n" );
		return 0;
	}
	if (argc >= 3) {
		printf( "Too Many Arguments - Only 2 Arguments Are Allowed\n" );
		return -1;
	}

	FILE *File = fopen(argv[1], "rb" );
	if (File) {
		unsigned char *readBuffer = (unsigned char *) malloc(READ_BUFFER_SIZE);
		if (readBuffer)	{
			PNGData PNG;
			if (initPNGProcess(&PNG)) {
				while (!feof(File))	{
					size_t bytesRead = fread( readBuffer, 1, READ_BUFFER_SIZE, File );
					if ((bytesRead != READ_BUFFER_SIZE ) && !feof(File)) {
						printf( "\nCAN'T READ FILE: %s\n", argv[1]);
						break;
					}
					if (processBuffer( &PNG, readBuffer, bytesRead)) {
						parsed = TRUE;
					}
				}
				if (parsed) {

					parsed = processFinish( &PNG );
				}
				freeChunkData(&PNG);
			}

			free(readBuffer);
		}
		else {
			printf( "\nCAN'T ALLOCATE MEMORY: %u bytes\n", (unsigned int) READ_BUFFER_SIZE );
		}
		fclose( File );
	}
	else {
		printf( "Cannot open file %s\n", argv[1]);
	}

	if(parsed)
		printf( "File parsed successfully\n" );

	return 0;
}

