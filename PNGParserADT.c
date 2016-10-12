#include "PNGParser.h"
#include "crc.h"

/*
 * Functon to check whether the given chunk contains valid characters or not
 */
int isChunkTypeValid( const unsigned char *ChunkType ) {
	size_t i = 0;
	int validCharacters = FALSE;
	for ( ; i < CHUNK_TYPE_LENGTH; i++ ) {
		unsigned int Character = ChunkType[i];
		validCharacters = ((Character >= 65) && (Character <= 90)) || ((Character >= 97) && (Character <= 122));
		if (!validCharacters)
			break;
	}
	return validCharacters;
}

/*
 * Function to check CRC of the chunk
 */
int isValidCrc( const unsigned char *ChunkType, const unsigned char *ChunkData, size_t ChunkSize, uint32_t ChunkCrc ) {
	unsigned long crc = update_crc( 0xffffffffL, ChunkType, CHUNK_TYPE_LENGTH );
	crc = update_crc( crc, ChunkData, (int) ChunkSize );
	crc ^= 0xffffffffL;
	return (crc == ChunkCrc);
}

/*
 * Function to verify the type of chunk and CRC,
 *  If chunk type is valid and CRC is valid then it processes the chunk
 */

int verifyAndProcessChunk( PNGData* PNG ) {
	Chunk chunk;
	int processed = FALSE;
	const unsigned char *ChunkType = PNG->chunkHeader + 4;
	if ( !isValidCrc( ChunkType, PNG->chunkData, PNG->chunkSize, getLastByte( PNG->chunkCRC ) ) ) {
		printf( "DATA CORRUPTED\n" );
		return processed;
	}
	if ( !isChunkTypeValid( ChunkType ) ) {
		printf( "INVALID CHUNK TYPE\n" );
		return processed;
	}
	memcpy( chunk.chunkType, ChunkType, sizeof( chunk.chunkType ) );
	chunk.Data = PNG->chunkData;
	chunk.dataSize = PNG->chunkSize;
	processed = processChunk( &PNG->chunkInfo, &chunk );
	return processed;
}

/*
 * chunkData from the PNGData structure will be deleted/NULL
 */
void freeChunkData( PNGData* PNG ) {
	free( PNG->chunkData );
	PNG->chunkData = NULL;
}

/*
 * Function to process the components of chunk layout
 */
int processCopiedData( PNGData* PNG ) {
	switch ( PNG->State ) {
	/*Verifying whether it is PNG file or not*/
	case PROCESS_PNG_HEADER:
		if (memcmp( PNG->chunkHeader, pngHeader, sizeof( PNG->chunkHeader )))	{
			printf( "INVALID PNG SIGNATURE\n" );
			return FALSE;
		}
		PNG->State = PROCESS_CHUNK_HEADER;

		PNG->bytesToCopy = sizeof( PNG->chunkHeader );
		PNG->bytesCopied = 0;
		PNG->bufferData = PNG->chunkHeader;
		break;

		/* verifying chunk header*/
	case PROCESS_CHUNK_HEADER:
		PNG->chunkSize = getLastByte( PNG->chunkHeader );
		if ( PNG->chunkSize) {

			if ( PNG->chunkSize > ( 1u << 31 ) - 1)	{
				printf( "INVALID CHUNK LENGTH\n");
				return FALSE;
			}
			PNG->chunkData = (unsigned char*) malloc( PNG->chunkSize );
			if ( !PNG->chunkData) {
				printf("CAN'T ALLOCATE MEMORY: %u bytes\n", (unsigned int)PNG->chunkSize );
				return FALSE;
			}
			PNG->State = PROCESS_CHUNK_DATA;

			PNG->bytesToCopy = PNG->chunkSize;
			PNG->bytesCopied = 0;
			PNG->bufferData = PNG->chunkData;
			break;
		}
		/*Prepare to process chunk data to validate CRC*/
	case PROCESS_CHUNK_DATA:
		PNG->State = PROCESS_CHUNK_CRC;

		PNG->bytesToCopy = sizeof(PNG->chunkCRC);
		PNG->bytesCopied = 0;
		PNG->bufferData = PNG->chunkCRC;
		break;
		/*Validate and Process chunk*/
	case PROCESS_CHUNK_CRC:
		if ( !verifyAndProcessChunk(PNG))
			return FALSE;
		freeChunkData( PNG );
		PNG->State = PROCESS_CHUNK_HEADER;

		PNG->bytesToCopy = sizeof(PNG->chunkHeader);
		PNG->bytesCopied = 0;
		PNG->bufferData = PNG->chunkHeader;
		break;

	default:
		printf( "INTERNAL ERROR\n" );
		return FALSE;
	}
	return TRUE;
}

/*
 * Initialize all the values of PNG Structure before reading the file
 */
int initPNGProcess( PNGData* PNG ) {
	PNG->State = PROCESS_PNG_HEADER;
	memset( PNG->chunkHeader, 0, sizeof( PNG->chunkHeader ) );
	memset( PNG->chunkCRC, 0, sizeof( PNG->chunkCRC ) );
	PNG->chunkSize = 0;
	PNG->chunkData = NULL;
	PNG->bytesToCopy = sizeof(PNG->chunkHeader);
	PNG->bytesCopied = 0;
	PNG->bufferData = PNG->chunkHeader;

	PNG->chunkInfo.IHDR = FALSE;
	PNG->chunkInfo.IDAT = FALSE;
	PNG->chunkInfo.PLTE = FALSE;
	PNG->chunkInfo.ICCP = FALSE;
	PNG->chunkInfo.SRGB = FALSE;
	PNG->chunkInfo.CHRM = FALSE;
	PNG->chunkInfo.GAMA = FALSE;
	PNG->chunkInfo.SBIT = FALSE;
	PNG->chunkInfo.BKGD = FALSE;
	PNG->chunkInfo.HIST = FALSE;
	PNG->chunkInfo.TRNS = FALSE;
	PNG->chunkInfo.PHYS = FALSE;
	PNG->chunkInfo.TIME = FALSE;
	PNG->chunkInfo.IEND = FALSE;
	PNG->chunkInfo.lastChunkIEND = FALSE;
	PNG->chunkInfo.lastChunkIDAT = FALSE;
	PNG->chunkInfo.colorType = 0;
	return TRUE;
}

/*
 * read the Buffer into PNGData and then process
 */
int processBuffer( PNGData* PNG, const unsigned char *Data, size_t DataLength ) {
	size_t i = 0;
	int processed = FALSE;
	while (i < DataLength) {
		size_t BytesAvailable = DataLength - i;
		size_t BytesRequired = PNG->bytesToCopy - PNG->bytesCopied;
		size_t BytesToCopy = ((BytesAvailable < BytesRequired ) ? BytesAvailable : BytesRequired);
		memcpy( PNG->bufferData + PNG->bytesCopied, Data + i, BytesToCopy );
		PNG->bytesCopied += BytesToCopy;
		i += BytesToCopy;
		if ( PNG->bytesCopied == PNG->bytesToCopy) {
			processed = processCopiedData(PNG);
			if(!processed)
				return processed;
		}
	}
	return processed;
}

/*
 * Finish the Processing of the File
 */
int processFinish( PNGData* PNG ) {
	/*Process state should be waitingfor another chunk*/
	if ( ( PNG->State != PROCESS_CHUNK_HEADER ) ||	PNG->bytesCopied) {
		printf( "MISSING CHUNK HEADER\n" );
		return FALSE;
	}
	/*Process Last chunk*/
	return processLastChunk( &PNG->chunkInfo);
}

/*
 * Verifies the chunkType and its length
 */
int isChunkType(const unsigned char *ChunkType, const char *ChunkString) {
	if (strlen(ChunkString) != CHUNK_TYPE_LENGTH)
		return FALSE;
	return !memcmp(ChunkType, ChunkString, CHUNK_TYPE_LENGTH);
}

/*
 * Process the chunks
 */
int processChunk( ChunkInfo *cInfo, const Chunk *chunk ) {
	if ( !isValidChunkOrder( cInfo, chunk )) {
		printf( "INVALID CHUNK ORDER\n" );
		return FALSE;
	}

	if (isChunkType(chunk->chunkType, "IHDR")) {
		if (!processIHDRChunk(chunk, &cInfo->colorType))
			return FALSE;
	}
	else if (isChunkType(chunk->chunkType, "IDAT"))	{
		processGenericChunk(chunk);
	}
	else if (isChunkType(chunk->chunkType, "IEND"))	{
		if (!processIENDChunk(chunk))
			return FALSE;
	}
	else if (isChunkType(chunk->chunkType, "tIME"))	{
		if (!processTIMEChunk(chunk))
			return FALSE;
	}
	else if (isChunkType(chunk->chunkType, "cHRM"))	{
		if (!processCHRMChunk(chunk))
			return FALSE;
	}
	else if (isChunkType(chunk->chunkType, "gAMA"))	{
		if (!processGAMAChunk(chunk))
			return FALSE;
	}
	else if (isChunkType(chunk->chunkType, "tEXt"))	{
		if (!processTEXTChunk(chunk))
			return FALSE;
	}
	else if (isChunkType(chunk->chunkType, "bKGD"))	{
		if (!processBKGDChunk(chunk, cInfo->colorType))
			return FALSE;
	}
	else if (isChunkType(chunk->chunkType, "pHYs")) {
		if (!processPHYSChunk(chunk))
			return FALSE;
	}
	else if (isChunkType(chunk->chunkType, "PLTE"))	{
		if (!processPLTEChunk(chunk))
			return FALSE;
	}
	else if (isChunkType(chunk->chunkType, "iCCP")) {
		if (!processICCPChunk(chunk))
			return FALSE;
	}
	else if (isChunkType(chunk->chunkType, "sRGB"))	{
		if (!processSRGBChunk(chunk))
			return FALSE;
	}
	else if (isChunkType(chunk->chunkType, "sBIT")) {
		if (!processSBITChunk(chunk, cInfo->colorType))
			return FALSE;
	}
	else if (isChunkType(chunk->chunkType, "zTXt"))	{
		if (!processZTXTChunk(chunk))
			return FALSE;
	}
	else if (!(chunk->chunkType[0] & (1u << 5)))
	{
		/* unknown critical chunk */
		printf("UNKNOWN CRITICAL CHUNK\n");
		return FALSE;
	}
	else
		processGenericChunk(chunk);

	return TRUE;
}

/*
 * Process the Last Chunk
 */
int processLastChunk( ChunkInfo *cInfo ) {
	int processed = FALSE;
	/* last chunk should be IEND */
	if ( !cInfo->lastChunkIEND ) {
		printf( "IEND CHUNK SHOULD BE THE LAST CHUNK\n" );
		return processed;
	}
	/* colorType 3 required for PLTE chunk*/
	if ((cInfo->colorType == 3) && !cInfo->PLTE) {
		printf("PLTE CHUNK SHOULD HAVE COLOR TYPE 3\n");
		return processed;
	}
	/*ColorType 0 or 4 should not be there for PLTE chunk*/
	if (((cInfo->colorType == 0) || (cInfo->colorType == 4)) && cInfo->PLTE) {
		printf("PLTE CHUNK SHOULDN'T HAVE FOR COLOR TYPE 0 AND 4\n");
		return processed;
	}
	processed = TRUE;

	return processed;
}

/*
 * Verify the order of chunks
 */
int isValidChunkOrder(ChunkInfo *cInfo, const Chunk *chunk) {
	if (cInfo->lastChunkIEND)
		return FALSE;

	if (cInfo->IDAT == TRUE && cInfo->lastChunkIDAT == FALSE && isChunkType(chunk->chunkType, "IDAT"))
		return FALSE;
	cInfo->lastChunkIDAT = FALSE;

	if (isChunkType(chunk->chunkType, "IHDR")) {
		if (cInfo->IHDR == FALSE) {
			cInfo->IHDR = TRUE;
		}
		else {
			return FALSE;
		}
	}
	else if (isChunkType(chunk->chunkType, "PLTE"))	{
		if (cInfo->PLTE == FALSE && cInfo->IHDR == TRUE &&
				cInfo->IDAT == FALSE && cInfo->BKGD == FALSE &&
				cInfo->HIST == FALSE && cInfo->TRNS == FALSE) {
			cInfo->PLTE = TRUE;
		}
		else {
			return FALSE;
		}
	}
	else if (isChunkType(chunk->chunkType, "IDAT"))	{
		if (cInfo->IHDR == FALSE) {
			return FALSE;
		}
		if (cInfo->IDAT == FALSE)
			cInfo->IDAT = TRUE;
		cInfo->lastChunkIDAT = TRUE;
	}
	else if (isChunkType(chunk->chunkType, "IEND"))	{
		if (cInfo->IHDR == TRUE && cInfo->IDAT == TRUE && cInfo->IEND == FALSE)	{
			cInfo->IEND = TRUE;
			cInfo->lastChunkIEND = TRUE;
		}
		else {
			return FALSE;
		}
	}
	else if (isChunkType(chunk->chunkType, "cHRM"))	{
		if (cInfo->IHDR == TRUE && cInfo->CHRM == FALSE &&
				cInfo->PLTE == FALSE && cInfo->IDAT == FALSE) {
			cInfo->CHRM = TRUE;
		}
		else {
			return FALSE;
		}
	}
	else if (isChunkType(chunk->chunkType, "gAMA"))	{
		if (cInfo->IHDR == TRUE && cInfo->GAMA == FALSE &&
				cInfo->PLTE == FALSE && cInfo->IDAT == FALSE) {
			cInfo->GAMA = TRUE;
		}
		else {
			return FALSE;
		}
	}
	else if (isChunkType(chunk->chunkType, "iCCP"))	{
		if (cInfo->IHDR == TRUE && cInfo->ICCP == FALSE &&
				cInfo->SRGB == FALSE && cInfo->PLTE == FALSE &&
				cInfo->IDAT == FALSE) {
			cInfo->ICCP = TRUE;
		}
		else {
			return FALSE;
		}
	}
	else if (isChunkType(chunk->chunkType, "sBIT"))	{
		if (cInfo->IHDR == TRUE && cInfo->SBIT == FALSE &&
				cInfo->PLTE == FALSE && cInfo->IDAT == FALSE) {
			cInfo->SBIT = TRUE;
		}
		else {
			return FALSE;
		}
	}
	else if (isChunkType(chunk->chunkType, "sRGB"))	{
		if (cInfo->IHDR == TRUE && cInfo->ICCP == FALSE &&
				cInfo->SRGB == FALSE && cInfo->PLTE == FALSE &&
				cInfo->IDAT == FALSE) {
			cInfo->SRGB = TRUE;
		}
		else {
			return FALSE;
		}
	}
	else if (isChunkType(chunk->chunkType, "bKGD"))	{
		if (cInfo->IHDR == TRUE && cInfo->BKGD == FALSE &&
				cInfo->IDAT == FALSE) {
			cInfo->BKGD = TRUE;
		}
		else {
			return FALSE;
		}
	}
	else if (isChunkType(chunk->chunkType, "hIST"))	{
		if (cInfo->IHDR == TRUE && cInfo->HIST == FALSE && cInfo->PLTE == TRUE &&
				cInfo->IDAT == FALSE) {
			cInfo->HIST = TRUE;
		}
		else {
			return FALSE;
		}
	}
	else if (isChunkType(chunk->chunkType, "tRNS"))	{
		if (cInfo->IHDR == TRUE && cInfo->TRNS == FALSE &&
				cInfo->IDAT == FALSE) {
			cInfo->TRNS = TRUE;
		}
		else {
			return FALSE;
		}
	}
	else if (isChunkType(chunk->chunkType, "pHYs"))	{
		if (cInfo->IHDR == TRUE && cInfo->PHYS == FALSE && cInfo->IDAT == FALSE) {
			cInfo->PHYS = TRUE;
		}
		else {
			return FALSE;
		}
	}
	else if (isChunkType(chunk->chunkType, "tIME"))	{
		if (cInfo->IHDR == TRUE && cInfo->TIME == FALSE) {
			cInfo->TIME = TRUE;
		}
		else {
			return FALSE;
		}
	}
	else {
		if (cInfo->IHDR == FALSE)
			return FALSE;
	}
	return TRUE;
}

int processIENDChunk(const Chunk *chunk) {
	if (chunk->dataSize)
	{
		fputs("IEND CHUNK LENGTH SHOULD BE 0.\n", stderr);
		return FALSE;
	}
	return TRUE;
}

/*
 * Process the generic chunk type and print
 */
void processGenericChunk(const Chunk *chunk) {
	const size_t limitSize = 20;
	int IsPrintLimit = chunk->dataSize > limitSize;
	size_t PrintBytes = (IsPrintLimit ? limitSize : chunk->dataSize);
	size_t i = 0;

	printf("RAW DATA ");
	for (; i < CHUNK_TYPE_LENGTH; i++)
		printf("%c", chunk->chunkType[i]);
	if (chunk->dataSize)
		printf(":");
	for (i = 0; i < PrintBytes; i++)
		printf(" %.2x", chunk->Data[i]);
	if (IsPrintLimit)
		printf(" ...");
	printf("\n");
}

/*
 * process chunk type IHDR
 */
int processIHDRChunk(const Chunk *chunk, unsigned int *ColorTypePtr ) {
	unsigned char bitDepth;
	unsigned char colorType;
	unsigned char compressionMethod;
	unsigned char filterMethod;
	unsigned char interlaceMethod;
	unsigned int width;
	unsigned int height;
	const char* imgType;
	if (chunk->dataSize != IHDR_DATA_LENGTH) {
		fputs("IMAGE HEADER DISTORTED.\n", stderr);
		return FALSE;
	}

	width = getLastByte(chunk->Data);
	height = getLastByte(chunk->Data + 4);
	if (width == 0 || height == 0 || width > PNG_MAX_VALUE || height > PNG_MAX_VALUE) {
		fputs("IMAGE RESOLUTION DISTORTED.\n", stderr);
		return FALSE;
	}
	bitDepth = chunk->Data[8];
	colorType = chunk->Data[9];
	compressionMethod = chunk->Data[10];
	filterMethod = chunk->Data[11];
	interlaceMethod = chunk->Data[12];


	if (bitDepth != 0x01 && bitDepth != 0x02
			&& bitDepth != 0x04 && bitDepth != 0x08
			&& bitDepth != 0x10) {
		fputs("BIT DEPTH INVALID.\n", stderr);
		return FALSE;
	}


	if (colorType != 0x00 && colorType != 0x02
			&& colorType != 0x03 && colorType != 0x04
			&& colorType != 0x06) {
		fputs("COLOR TYPE INVALID.\n", stderr);
		return FALSE;
	}


	if (colorType == 0x02 || colorType == 0x04
			|| colorType == 0x06) {
		if (bitDepth != 0x08 && bitDepth != 0x10) {
			fputs("BIT DEPTH INVALID FOR THIS COLOR TYPE.\n", stderr);
			return FALSE;
		}
	}

	if (colorType == 0x03) {
		if (bitDepth == 0x10){
			fputs("BIT DEPTH INVALID FOR THIS COLOR TYPE.\n", stderr);
			return FALSE;
		}
	}




	if (compressionMethod != 0x00) {
		fputs("UNKNOWN COMPRESSION METHOD, ONLY 0 ALLOWED.\n", stderr);
		return FALSE;
	}


	if (filterMethod != 0x00) {
		fputs("UNKNOWN FILTER METHOD, ONLY 0 ALLOWED.\n", stderr);
		return FALSE;
	}


	if (interlaceMethod != 0x00 && interlaceMethod != 0x01) {
		fputs("UNKNOWN INTERLACE METHOD, ONLY 0 AND 1 ARE ALLOWED.\n", stderr);
		return FALSE;
	}

	printf("SIZE OF IMAGE IS %u x %u PIXELS.\n", width, height);

	switch (colorType) {
	case 0x00:
		imgType = "GREY SCALE";
		break;
	case 0x02:
		imgType = "TRUE COLOR";
		break;
	case 0x03:
		imgType = "INDEXED COLOR";
		break;
	case 0x04:
		imgType = "GREY SCALE WITH ALPHA";
		break;
	case 0x06:
		imgType = "TRUE COLOR WITH ALPHA";
		break;
	default:
		return FALSE;
	}
	printf("COLOR TYPE : %s\n", imgType);
	*ColorTypePtr = colorType;
	return TRUE;
}
/*
 * process chunk type tIME
 */
int processTIMEChunk(const Chunk *chunk) {
	unsigned int year;
	unsigned int month;
	unsigned int day;
	unsigned int hour;
	unsigned int minute;
	unsigned int second;

	if (chunk->dataSize != TIME_DATA_LENGTH) {
		fputs("tIME CHUNK LENGTH INVALID.\n", stderr);
		return FALSE;
	}

	year = getLastWord(chunk->Data);
	month = chunk->Data[2];
	day = chunk->Data[3];
	hour = chunk->Data[4];
	minute = chunk->Data[5];
	second = chunk->Data[6];
	if ((month < 1 || month > 12) ||
			(day < 1 || day > 31) ||
			(hour > 23) ||
			(minute > 59) ||
			(second > 60)) {
		fputs("INVALID TIME.\n", stderr);
		return FALSE;
	}

	printf("LAST MODIFIED TIME: %u.%u.%u %02u:%02u:%02u\n", day, month, year, hour, minute, second);
	return TRUE;
}
/*
 * process chunk type cHRM
 */
int processCHRMChunk(const Chunk *chunk) {

	unsigned int white_x;
	unsigned int white_y;
	unsigned int red_x;
	unsigned int red_y;
	unsigned int green_x;
	unsigned int green_y;
	unsigned int blue_x;
	unsigned int blue_y;
	const double scale = 100000.0;

	if(chunk->dataSize != CHRM_DATA_LENGTH)	{
		fputs("cHRM CHUNK LENGTH INVALID.\n",stderr);
		return FALSE;
	}

	white_x=getLastByte(chunk->Data);
	white_y=getLastByte(chunk->Data+4);
	red_x=getLastByte(chunk->Data+8);
	red_y=getLastByte(chunk->Data+12);
	green_x=getLastByte(chunk->Data+16);
	green_y=getLastByte(chunk->Data+20);
	blue_x=getLastByte(chunk->Data+24);
	blue_y=getLastByte(chunk->Data+28);

	printf("PRIMARY CHROMATICITIES:\n");
	printf("\tWhite x is %.2lf White y is %.2lf\n", white_x / scale, white_y / scale);
	printf("\tRed x is %.2lf Red y is %.2lf\n", red_x / scale, red_y / scale);
	printf("\tGreen x is %.2lf Green y is %.2lf\n", green_x / scale, green_y / scale);
	printf("\tBlue x is %.2lf Blue y is %.2lf\n", blue_x / scale, blue_y / scale);
	return TRUE;
}
/*
 * process chunk type gAMA
 */
int processGAMAChunk(const Chunk *chunk) {
	unsigned int gama;
	const double scale = 100000.0;
	if(chunk->dataSize != GAMA_DATA_LENGTH)	{
		fputs("gAMA CHUNK LENGTH INVALID.\n",stderr);
		return FALSE;
	}
	gama = getLastByte(chunk->Data);
	if (gama == 0 || gama > PNG_MAX_VALUE)	{
		fputs("gAMA CHUNK VALUE INVALID.\n", stderr);
		return FALSE;
	}
	printf("gAMA: \n\t%.5lf\n", gama / scale);
	return TRUE;
}
/*
 * process chunk type tEXt
 */
int processTEXTChunk(const Chunk *chunk) {

	unsigned int keyword_length;
	const unsigned int null_length = 1;
	unsigned int text_length;
	unsigned int index;

	const unsigned char *NullBytePtr = memchr(chunk->Data, 0x00, chunk->dataSize);
	if (!NullBytePtr) {
		fputs("tEXt CHUNK INVALID.\n", stderr);
		return FALSE;
	}
	keyword_length = NullBytePtr - chunk->Data;
	text_length = chunk->dataSize - (keyword_length + null_length);
	if (memchr(NullBytePtr + null_length, 0x00, text_length)) {
		fputs("tEXt CHUNK INVALID.\n", stderr);
		return FALSE;
	}

	if(keyword_length > TEXT_DATA_KEY_LENGTH_MAX || keyword_length < 1) {
		fputs("tEXt CHUNK LENGTH INVALID.\n",stderr);
		return FALSE;
	}

	printf("%s: ",chunk->Data);
	for (index = null_length + keyword_length; index < chunk->dataSize; index++)
		printf("%c",chunk->Data[index]);
	printf("\n");
	return TRUE;
}
/*
 * process chunk type bKGD
 */
int processBKGDChunk(const Chunk *chunk, unsigned int ColorType) {

	if (((ColorType == 0 || ColorType == 4) && (chunk->dataSize != BKGD_TYPE_0_AND_4_DATA_LENGTH)) ||
			((ColorType == 2 || ColorType == 6) && (chunk->dataSize != BKGD_TYPE_2_AND_6_DATA_LENGTH)) ||
			((ColorType == 3) && (chunk->dataSize != BKGD_TYPE_3_DATA_LENGTH)))	{
		fputs("bKGD CHUNK LENGTH INVALID.\n", stderr);
		return FALSE;
	}

	if (chunk->dataSize == BKGD_TYPE_0_AND_4_DATA_LENGTH) {
		unsigned int greyScale = getLastWord(chunk->Data);
		printf("BACKGROUND:\n\tGrey Scale:%u\n",greyScale);
	}
	else if (chunk->dataSize == BKGD_TYPE_2_AND_6_DATA_LENGTH) {
		unsigned int Red = getLastWord(chunk->Data);
		unsigned int Green = getLastWord(chunk->Data + 2);
		unsigned int Blue = getLastWord(chunk->Data + 4);
		printf("BACKGROUND:\n\tRed:%u\n\tGreen:%u\n\tBlue:%u\n",Red,Green,Blue);
	}
	else if (chunk->dataSize == BKGD_TYPE_3_DATA_LENGTH) {
		unsigned int Palette_index = chunk->Data[0];
		printf("BACKGROUND:\n\tPalette index:%u\n",Palette_index);
	}
	return TRUE;

}
/*
 * process chunk type pHYs
 */
int processPHYSChunk(const Chunk *chunk) {
	unsigned int x;
	unsigned int y;
	unsigned int unit;

	if (chunk->dataSize != PHY_DATA_LENGTH)	{
		fputs("pHYs CHUNK LENGTH INVALID.\n", stderr);
		return FALSE;
	}

	x = getLastByte(chunk->Data);
	y = getLastByte(chunk->Data+4);
	if (x > PNG_MAX_VALUE || y > PNG_MAX_VALUE) {
		fputs("pHYs CHUNK DATA INVALID.\n", stderr);
		return FALSE;
	}

	unit = chunk->Data[8];
	if ((unit != 1) && (unit != 0))	{
		fputs("pHYs CHUNK DATA INVALID.\n", stderr);
		return FALSE;
	}

	if (unit == 1)
		printf("PHYSIC:\n\tPixels per units in x axis %u\n\tPixels per units in y axis %u\n\tUnit value is the metre\n",x,y);
	else
		printf("Physic:\n\tPixels per units in x axis %u\n\tPixels per units in y axis %u\n\tUnit value unknown\n",x,y);

	return TRUE;
}
/*
 * process chunk type PLTE
 */
int processPLTEChunk(const Chunk *chunk) {
	unsigned int index;
	unsigned int size = chunk->dataSize / 3;

	if ((chunk->dataSize == 0 ) || ((chunk->dataSize % 3) != 0) || ((chunk->dataSize / 3) > PLTE_DATA_LENGTH)) {
		fputs("PLTE CHUNK LENGTH INVALID.\n", stderr);
		return FALSE;
	}
	printf("PLTE data:\n");
	for(index = 0; index < size; index++){
		unsigned int r = chunk->Data[0 + index * 3];
		unsigned int g = chunk->Data[1 + index * 3];
		unsigned int b = chunk->Data[2 + index * 3];
		printf("PALETTE INDEX %u:\tR:\t%u\tG:\t%u\tB:\t%u\n",(unsigned int)index,r,g,b);
	}
	return TRUE;
}
/*
 * process chunk type iCCP
 */
int processICCPChunk(const Chunk *chunk) {
	unsigned int profile_name_length;
	unsigned int index;
	unsigned int compressed_length;
	const unsigned char * compressed_data;
	unsigned int compression_method;
	const unsigned char *NullBytePtr = memchr(chunk->Data, 0x00, chunk->dataSize);
	if (!NullBytePtr) {
		fputs("iCCP CHUNK INVALID.\n", stderr);
		return FALSE;
	}
	profile_name_length = NullBytePtr - chunk->Data;
	if ((profile_name_length < 1) || (profile_name_length > 79)) {
		fputs("iCCP PROFILE NAME LENGTH INVALID.\n", stderr);
		return FALSE;
	}

	for (index = 0; index < profile_name_length; index++) {
		if (((32 <= chunk->Data[index]) && (chunk->Data[index] <= 126)) || ((161 <= chunk->Data[index]))) {}
		else{
			fputs("iCCP PROFILE TEXT INVALID.\n", stderr);
			return FALSE;
		}
	}


	if ((chunk->Data[0] == ' ') || (chunk->Data[profile_name_length - 1] == ' ')) {
		fputs("iCCP PROFILE TEXT INVALID.\n", stderr);
		return FALSE;
	}


	for (index = 0; index < profile_name_length - 1; index++) {
		if ((chunk->Data[index] == ' ') && (chunk->Data[index+1] == ' ')) {
			fputs("iCCP PROFILE TEXT INVALID.\n", stderr);
			return FALSE;
		}
	}

	compressed_data = chunk->Data + profile_name_length + 1;
	compressed_length = chunk->dataSize - (profile_name_length + 1);
	if (compressed_length < 1) {
		fputs("iCCP DATA LENGTH INVALID.\n", stderr);
		return FALSE;
	}
	compression_method = chunk->Data[profile_name_length + 1];
	if (compression_method != 0) {
		fputs("iCCP DATA COMPRESSION METHOD INVALID.\n", stderr);
		return FALSE;
	}

	printf("iCCP DATA:\n");

	printf("\tPROFILE NAME:%s\n", chunk->Data);

	printf("\tCOMPRESSION METHOD (0=zlib):%u\n",compression_method);

	printf("\tCOMPRESSED DATA:\n\t\t\t");
	for(index = 1; index < compressed_length; index++) {
		printf("%.2x",compressed_data[index] );
		if(index != 0 && index % 15 == 0)
			printf("\n\t\t\t");
		else
			printf(" ");
	}
	printf("\n");
	return TRUE;
}
/*
 * process chunk type sRGB
 */
int processSRGBChunk(const Chunk *chunk) {
	unsigned int intent;

	if (chunk->dataSize != SRGB_DATA_LENGTH) {
		fputs("sRGB CHUNK LENGTH INVALID.\n", stderr);
		return FALSE;
	}
	intent = chunk->Data[0];
	switch(intent) {
	case 0:
		printf("sRGB: Perceptual\n");
		break;
	case 1:
		printf("sRGB: Relative colorimetric\n");
		break;
	case 2:
		printf("sRGB: Saturation\n");
		break;
	case 3:
		printf("sRGB: Absolute colorimetric\n");
		break;
	default:
		fputs("sRGB VALUE INVALID.\n", stderr);
		return FALSE;
		break;
	}
	return TRUE;
}
/*
 * process chunk type sBIT
 */
int processSBITChunk(const Chunk *chunk, unsigned int ColorType) {
	unsigned int greyscale;
	unsigned int r;
	unsigned int g;
	unsigned int b;
	unsigned int alpha;
	if (((ColorType == 0 ) && (chunk->dataSize != SBIT_TYPE_0_DATA_LENGTH)) ||
			((ColorType == 2 || ColorType == 3) && (chunk->dataSize != SBIT_TYPE_2_AND_3_DATA_LENGTH)) ||
			((ColorType == 4) && (chunk->dataSize != SBIT_TYPE_4_DATA_LENGTH)) ||
			((ColorType == 6) && (chunk->dataSize != SBIT_TYPE_6_DATA_LENGTH)))	{
		fputs("sBIT CHUNK LENGTH INVALID.\n", stderr);
		return FALSE;
	}

	switch(chunk->dataSize){
	case SBIT_TYPE_0_DATA_LENGTH:
		greyscale = chunk->Data[0];
		printf("sBIT:\n\tGREY SCALE %u\n",greyscale);
		break;
	case SBIT_TYPE_2_AND_3_DATA_LENGTH:
		r = chunk->Data[0];
		g = chunk->Data[1];
		b = chunk->Data[2];
		printf("sBIT:\n\tR: %u\tG: %u\tB: %u\n",r,g,b);
		break;
	case SBIT_TYPE_4_DATA_LENGTH:
		greyscale = chunk->Data[0];
		alpha = chunk->Data[1];
		printf("sBIT:\n\tGREY SCALE: %u\tAlpha: %u\n",greyscale,alpha);
		break;
	case SBIT_TYPE_6_DATA_LENGTH:
		r = chunk->Data[0];
		g = chunk->Data[1];
		b = chunk->Data[2];
		alpha = chunk->Data[3];
		printf("sBIT:\n\tR: %u\tG: %u\tB: %u\tAlpha: %u\n",r,g,b,alpha);
		break;
	default:
		return FALSE;
	}
	return TRUE;
}
/*
 * process chunk type zTXt
 */
int processZTXTChunk(const Chunk *chunk) {

	unsigned int keyword_length;
	unsigned int compressed_length;
	const unsigned char *compressed_data;
	unsigned int index;
	unsigned int compression_method;
	const unsigned char *NullBytePtr = memchr(chunk->Data, 0x00, chunk->dataSize);
	if (!NullBytePtr) {
		fputs("zTXt CHUNK INVALID.\n", stderr);
		return FALSE;
	}
	keyword_length = NullBytePtr - chunk->Data;
	if ((keyword_length < 1) || (keyword_length > 79)) {
		fputs("zTXt KEY LENGTH INVALID.\n", stderr);
		return FALSE;
	}
	compressed_data = chunk->Data + keyword_length + 1;
	compressed_length = chunk->dataSize - (keyword_length + 1);
	if (compressed_length < 1) {
		fputs("zTXt DATA LENGTH INVALID.\n", stderr);
		return FALSE;
	}
	compression_method = chunk->Data[keyword_length + 1];
	if (compression_method != 0) {
		fputs("zTXt DATA COMPRESSED METHOD INVALID.\n", stderr);
		return FALSE;
	}
	printf("zTXt DATA:\n");

	printf("\tKEYWORD:%s\n", chunk->Data);

	printf("\tCOMPRESSED METHOD (0=zlib):%u\n",compression_method);

	printf("\tCOMPRESSED DATA:\n\t\t\t");
	for(index = 1; index < compressed_length; index++) {
		printf("%.2x",compressed_data[index] );
		if(index != 0 && index % 15 == 0)
			printf("\n\t\t\t");
		else
			printf(" ");
	}
	printf("\n");
	return TRUE;
}
/*
 * To get the last byte of the Integer
 */

uint32_t getLastByte( const unsigned char *Data ) {
	uint32_t Result = Data[0];
	Result <<= CHAR_BIT;
	Result |= Data[1];
	Result <<= CHAR_BIT;
	Result |= Data[2];
	Result <<= CHAR_BIT;
	Result |= Data[3];
	return Result;
}
/*
 * To get the last 2 bytes of the Integer
 */
uint16_t getLastWord(const unsigned char *Data) {
	uint16_t Result = Data[0];
	Result <<= CHAR_BIT;
	Result |= Data[1];
	return Result;
}

