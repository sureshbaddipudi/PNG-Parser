
#include "PNGParser.h"

#define READ_BUFFER_SIZE	( 64 * 1024 )

int parsePNGImage( const char *FileName )
{
	int Result = TRUE;
	FILE *File = fopen( FileName, "rb" );
	if (File) {
		unsigned char *ReadBuffer = (unsigned char *) malloc( READ_BUFFER_SIZE );
		if (ReadBuffer)	{
			PNGData PNG;
			if (initPNGProcess(&PNG)) {
				while (!feof(File))	{
					size_t BytesRead = fread( ReadBuffer, 1, READ_BUFFER_SIZE, File );
					if ((BytesRead != READ_BUFFER_SIZE ) && !feof(File)) {
						printf( "Cannot read file: %s\n", FileName );
						Result = FALSE;
						break;
					}
					if (!processBuffer( &PNG, ReadBuffer, BytesRead)) {
						Result = FALSE;
						break;
					}
				}
				if ( Result )
					Result = processFinish( &PNG );
				processCleanup( &PNG );
			}
			else
				Result = FALSE;
			free( ReadBuffer );
		}
		else {
			printf( "Cannot allocate memory: %u bytes\n", (unsigned int) READ_BUFFER_SIZE );
			Result = FALSE;
		}
		fclose( File );
	}
	else {
		printf( "Cannot open file %s\n", FileName );
		Result = FALSE;
	}
	return Result;
}

int main( int argc, char *argv[] )
{
	if (argc < 2) {
		printf( "Usage: pngparser <file>\n" );
		return 0;
	}
	if (argc >= 3) {
		printf( "Too many arguments\n" );
		return -1;
	}
	if (!parsePNGImage(argv[1]))
		return -1;

	printf( "File parsed successfully\n" );
	return 0;
}
