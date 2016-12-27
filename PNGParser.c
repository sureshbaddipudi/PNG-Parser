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
	/*open the file in read mode*/
	FILE *File = fopen(argv[1], "rb" );
	if (File) {
		/*Read the of fixed size into buffer*/
		unsigned char *readBuffer = (unsigned char *) malloc(READ_BUFFER_SIZE);
		if (readBuffer)	{
			PNGData PNG;
			/*Initialize the PNGData and process*/
			if (initPNGProcess(&PNG)) {
				while (!feof(File))	{
					size_t bytesRead = fread( readBuffer, 1, READ_BUFFER_SIZE, File );
					if ((bytesRead != READ_BUFFER_SIZE ) && !feof(File)) {
						printf( "\nCAN'T READ FILE: %s\n", argv[1]);
						break;
					}
					/*Process the buffer*/
					if (processBuffer( &PNG, readBuffer, bytesRead)) {
						parsed = TRUE;
					}
					else {
						parsed = FALSE;
						break;
					}
				}
				if (parsed) {
					/*Process the last chunks*/
					parsed = processFinish( &PNG );
				}
				/*delete the buffer*/
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
		printf( "PARSING COMPLETED\n" );

	return 0;
}

