#include "PNGParser.h"
#include "crc.h"

static const unsigned char PngHeader[] = { 137, 80, 78, 71, 13, 10, 26, 10 };

static int isChunkTypeValid( const unsigned char *ChunkType ) {
	size_t i = 0;
	for ( ; i < CHUNK_TYPE_LENGTH; i++ ) {
		unsigned int Character = ChunkType[i];
		int IsIso646Letter = ( ( Character >= 65 ) && ( Character <= 90 ) ) || ( ( Character >= 97 ) && ( Character <= 122 ) );
		if ( !IsIso646Letter )
			return FALSE;
	}
	return TRUE;
}

static int isValidCrc( const unsigned char *ChunkType, const unsigned char *ChunkData, size_t ChunkSize, uint32_t ChunkCrc ) {
	unsigned long Crc = update_crc( 0xffffffffL, ChunkType, CHUNK_TYPE_LENGTH );
	Crc = update_crc( Crc, ChunkData, (int) ChunkSize );
	Crc ^= 0xffffffffL;
	return ( Crc == ChunkCrc );
}

static int verifyAndProcessChunk( PNGData* PNG ) {
	Chunk chunk;
	const unsigned char *ChunkType = PNG->Header + 4;
	if ( !isValidCrc( ChunkType, PNG->ChunkData, PNG->ChunkSize, GetLittleEndianInteger( PNG->ChunkCrc ) ) ) {
		printf( "Data is corrupted\n" );
		return FALSE;
	}
	if ( !isChunkTypeValid( ChunkType ) ) {
		printf( "Incorrect chunk type\n" );
		return FALSE;
	}

	memcpy( chunk.chunkType, ChunkType, sizeof( chunk.chunkType ) );
	chunk.Data = PNG->ChunkData;
	chunk.dataSize = PNG->ChunkSize;
	return processChunk( &PNG->chunkInfo, &chunk );
}

static void freeChunkData( PNGData* PNG ) {
	free( PNG->ChunkData );
	PNG->ChunkData = NULL;
}

static void prepareToCopy( PNGData* PNG, unsigned char *TargetBuffer, size_t BytestoCopy ) {
	PNG->BytesToCopy = BytestoCopy;
	PNG->BytesCopied = 0;
	PNG->BufferData = TargetBuffer;
}

static int processCopiedData( PNGData* PNG ) {
	switch ( PNG->State ) {
	case PROCESS_PNG_HEADER:
		if (memcmp( PNG->Header, PngHeader, sizeof( PNG->Header )))	{
			printf( "File does not contain valid png signature\n" );
			return FALSE;
		}
		PNG->State = PROCESS_CHUNK_HEADER;
		prepareToCopy( PNG, PNG->Header, sizeof( PNG->Header ) );
		break;

	case PROCESS_CHUNK_HEADER:
		PNG->ChunkSize = GetLittleEndianInteger( PNG->Header );
		if ( PNG->ChunkSize) {
			/* chunk size should be maximum 2^31-1 by specification */
			if ( PNG->ChunkSize > ( 1u << 31 ) - 1)	{
				printf( "Chunk contains invalid length\n");
				return FALSE;
			}
			PNG->ChunkData = (unsigned char*) malloc( PNG->ChunkSize );
			if ( !PNG->ChunkData) {
				printf("Cannot allocate memory: %u bytes\n", (unsigned int)PNG->ChunkSize );
				return FALSE;
			}
			PNG->State = PROCESS_CHUNK_DATA;
			prepareToCopy( PNG, PNG->ChunkData, PNG->ChunkSize );
			break;
		}
		/* fall through - zero-length chunk */

	case PROCESS_CHUNK_DATA:
		PNG->State = PROCESS_CHUNK_CRC;
		prepareToCopy( PNG, PNG->ChunkCrc, sizeof( PNG->ChunkCrc));
		break;

	case PROCESS_CHUNK_CRC:
		if ( !verifyAndProcessChunk(PNG))
			return FALSE;
		freeChunkData( PNG );
		PNG->State = PROCESS_CHUNK_HEADER;
		prepareToCopy( PNG, PNG->Header, sizeof( PNG->Header ) );
		break;

	default:
		printf( "Internal Error\n" );
		return FALSE;
	}
	return TRUE;
}

int initPNGProcess( PNGData* PNG ) {
	PNG->State = PROCESS_PNG_HEADER;
	memset( PNG->Header, 0, sizeof( PNG->Header ) );
	memset( PNG->ChunkCrc, 0, sizeof( PNG->ChunkCrc ) );
	PNG->ChunkSize = 0;
	PNG->ChunkData = NULL;
	prepareToCopy( PNG, PNG->Header, sizeof( PNG->Header ) );
	return initChunkProcess( &PNG->chunkInfo);
}

int processBuffer( PNGData* PNG, const unsigned char *Data, size_t DataLength ) {
	size_t i = 0;
	while (i < DataLength) {
		/* copy as much data as possible to buffer */
		size_t BytesAvailable = DataLength - i;
		size_t BytesRequired = PNG->BytesToCopy - PNG->BytesCopied;
		size_t BytesToCopy = ( ( BytesAvailable < BytesRequired ) ? BytesAvailable : BytesRequired );
		memcpy( PNG->BufferData + PNG->BytesCopied, Data + i, BytesToCopy );

		/* update position */
		PNG->BytesCopied += BytesToCopy;
		i += BytesToCopy;

		/* check if buffer is complete */
		if ( PNG->BytesCopied == PNG->BytesToCopy )
			if ( !processCopiedData( PNG ) )
				return FALSE;
	}
	return TRUE;
}

int processFinish( PNGData* PNG ) {
	/* parser must ends exactly in state waiting for chunk */
	if ( ( PNG->State != PROCESS_CHUNK_HEADER ) ||	PNG->BytesCopied) {
		printf( "Mising chunk/header data\n" );
		return FALSE;
	}

	/* notify processor about last chunk */
	return processLastChunk( &PNG->chunkInfo);
}

void processCleanup( PNGData* PNG ) {
	freeChunkData( PNG );
}


int isChunkType(const unsigned char *ChunkType, const char *ChunkString) {
	if (strlen(ChunkString) != CHUNK_TYPE_LENGTH)
		return FALSE;
	return !memcmp(ChunkType, ChunkString, CHUNK_TYPE_LENGTH);
}

int initChunkProcess( ChunkInfo *cInfo ) {
	cInfo->IHDR = FALSE;
	cInfo->IDAT = FALSE;
	cInfo->PLTE = FALSE;
	cInfo->ICCP = FALSE;
	cInfo->SRGB = FALSE;
	cInfo->CHRM = FALSE;
	cInfo->GAMA = FALSE;
	cInfo->SBIT = FALSE;
	cInfo->BKGD = FALSE;
	cInfo->HIST = FALSE;
	cInfo->TRNS = FALSE;
	cInfo->PHYS = FALSE;
	cInfo->TIME = FALSE;
	cInfo->IEND = FALSE;
	cInfo->lastChunkIEND = FALSE;
	cInfo->lastChunkIDAT = FALSE;
	cInfo->colorType = 0;
	return TRUE;
}

int processChunk( ChunkInfo *cInfo, const Chunk *chunk ) {
	if ( !isValidChunkOrder( cInfo, chunk )) {
		printf( "Chunk order is not valid\n" );
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
		printf("Unknown critical chunk encountered\n");
		return FALSE;
	}
	else
		processGenericChunk(chunk);

	return TRUE;
}

int processLastChunk( ChunkInfo *cInfo ) {
	/* last chunk shall always be IEND */
	if ( !cInfo->lastChunkIEND ) {
		printf( "IEND chunk shall be the last one\n" );
		return FALSE;
	}
	/* PLTE chunk is required for color type 3 */
	if ((cInfo->colorType == 3) && !cInfo->PLTE) {
		printf("PLTE chunk shall be present for color type 3\n");
		return FALSE;
	}
	/* PLTE chunk must not be present for color type 0 or 4 */
	if (((cInfo->colorType == 0) || (cInfo->colorType == 4)) && cInfo->PLTE) {
		printf("PLTE chunk shall not be present for color type 0 or 4\n");
		return FALSE;
	}
	return TRUE;
}


int isValidChunkOrder(ChunkInfo *cInfo, const Chunk *chunk) {
	/* no chunk shall appear after IEND */
	if (cInfo->lastChunkIEND)
		return FALSE;
	/* Idat chunks shall be consecutive */
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
		/* other chunks shall appear after IHDR */
		if (cInfo->IHDR == FALSE)
			return FALSE;
	}
	return TRUE;
}

int processIENDChunk(const Chunk *chunk) {
	if (chunk->dataSize)
	{
		fputs("IEND chunk length has to be zero.\n", stderr);
		return FALSE;
	}
	return TRUE;
}

void processGenericChunk(const Chunk *chunk) {
	const size_t limitSize = 17;
	int IsPrintLimit = chunk->dataSize > limitSize;
	size_t PrintBytes = (IsPrintLimit ? limitSize : chunk->dataSize);
	size_t i = 0;
	printf("Raw data of chunk ");
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
		fputs("Image header is distorted.\n", stderr);
		return FALSE;
	}

	width = GetLittleEndianInteger(chunk->Data);
	height = GetLittleEndianInteger(chunk->Data + 4);
	if (width == 0 || height == 0 || width > PNG_MAX_VALUE || height > PNG_MAX_VALUE) {
		fputs("Image resolution attributes are distorted.\n", stderr);
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
		fputs("Bit depth not allowed.\n", stderr);
		return FALSE;
	}

	
	if (colorType != 0x00 && colorType != 0x02
			&& colorType != 0x03 && colorType != 0x04
			&& colorType != 0x06) {
		fputs("Color type not allowed.\n", stderr);
		return FALSE;
	}

	
	if (colorType == 0x02 || colorType == 0x04
			|| colorType == 0x06) {
		if (bitDepth != 0x08 && bitDepth != 0x10) {
			fputs("Bit depth not allowed for this color type.\n", stderr);
			return FALSE;
		}
	}

	if (colorType == 0x03) {
		if (bitDepth == 0x10){
			fputs("Bit depth not allowed for this color type.\n", stderr);
			return FALSE;
		}
	}

	

	
	if (compressionMethod != 0x00) {
		fputs("Unknown compression method, only 0 allowed.\n", stderr);
		return FALSE;
	}

	
	if (filterMethod != 0x00) {
		fputs("Unknown filter method, only 0 allowed.\n", stderr);
		return FALSE;
	}

	
	if (interlaceMethod != 0x00 && interlaceMethod != 0x01) {
		fputs("Unknown interlace method, only 0 and 1 allowed.\n", stderr);
		return FALSE;
	}

	printf("Size of this image is %u x %u pixels.\n", width, height);

	switch (colorType) {
	case 0x00:
		imgType = "Greyscale";
		break;
	case 0x02:
		imgType = "TRUEcolor";
		break;
	case 0x03:
		imgType = "Indexed-color";
		break;
	case 0x04:
		imgType = "Greyscale with alpha";
		break;
	case 0x06:
		imgType = "TRUEcolor with alpha";
		break;
	default:
		return FALSE;
	}
	printf("Color type is : %s\n", imgType);
	*ColorTypePtr = colorType;
	return TRUE;
}

int processTIMEChunk(const Chunk *chunk) {
	unsigned int year;
	unsigned int month;
	unsigned int day;
	unsigned int hour;
	unsigned int minute;
	unsigned int second;

	if (chunk->dataSize != TIME_DATA_LENGTH) {
		fputs("tIME chunk length is invalid.\n", stderr);
		return FALSE;
	}

	year = GetLittleEndianWord(chunk->Data);
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
		fputs("Invalid time is specified.\n", stderr);
		return FALSE;
	}

	printf("Time of last modification: %u.%u.%u %02u:%02u:%02u\n", day, month, year, hour, minute, second);
	return TRUE;
}
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
		fputs("cHRM chunk length is invalid.\n",stderr);
		return FALSE;
	}

	white_x=GetLittleEndianInteger(chunk->Data);
	white_y=GetLittleEndianInteger(chunk->Data+4);
	red_x=GetLittleEndianInteger(chunk->Data+8);
	red_y=GetLittleEndianInteger(chunk->Data+12);
	green_x=GetLittleEndianInteger(chunk->Data+16);
	green_y=GetLittleEndianInteger(chunk->Data+20);
	blue_x=GetLittleEndianInteger(chunk->Data+24);
	blue_y=GetLittleEndianInteger(chunk->Data+28);

	printf("Primary chromaticities:\n");
	printf("\tWhite x is %.2lf White y is %.2lf\n", white_x / scale, white_y / scale);
	printf("\tRed x is %.2lf Red y is %.2lf\n", red_x / scale, red_y / scale);
	printf("\tGreen x is %.2lf Green y is %.2lf\n", green_x / scale, green_y / scale);
	printf("\tBlue x is %.2lf Blue y is %.2lf\n", blue_x / scale, blue_y / scale);
	return TRUE;
}
int processGAMAChunk(const Chunk *chunk) {
	unsigned int gamma;
	const double scale = 100000.0;
	if(chunk->dataSize != GAMA_DATA_LENGTH)	{
		fputs("gAMA chunk length is invalid.\n",stderr);
		return FALSE;
	}
	gamma = GetLittleEndianInteger(chunk->Data);
	if (gamma == 0 || gamma > PNG_MAX_VALUE)	{
		fputs("gAMA chunk value is invalid.\n", stderr);
		return FALSE;
	}
	printf("gamma: \n\t%.5lf\n", gamma / scale);
	return TRUE;
}

int processTEXTChunk(const Chunk *chunk) {

	unsigned int keyword_length;
	const unsigned int null_length = 1;
	unsigned int text_length;
	unsigned int index;

	const unsigned char *NullBytePtr = memchr(chunk->Data, 0x00, chunk->dataSize);
	if (!NullBytePtr) {
		fputs("tEXt chunk is invalid.\n", stderr);
		return FALSE;
	}
	keyword_length = NullBytePtr - chunk->Data;
	text_length = chunk->dataSize - (keyword_length + null_length);
	if (memchr(NullBytePtr + null_length, 0x00, text_length)) {
		fputs("tEXt chunk is invalid.\n", stderr);
		return FALSE;
	}

	if(keyword_length > TEXT_DATA_KEY_LENGTH_MAX || keyword_length < 1) {
		fputs("tEXt chunk length is invalid.\n",stderr);
		return FALSE;
	}

	printf("%s: ",chunk->Data);
	for (index = null_length + keyword_length; index < chunk->dataSize; index++)
		printf("%c",chunk->Data[index]);
	printf("\n");
	return TRUE;
}
int processBKGDChunk(const Chunk *chunk, unsigned int ColorType) {
	if (((ColorType == 0 || ColorType == 4) && (chunk->dataSize != BKGD_DATA_LENGTH_TYPE_0_4)) ||
			((ColorType == 2 || ColorType == 6) && (chunk->dataSize != BKGD_DATA_LENGTH_TYPE_2_6)) ||
			((ColorType == 3) && (chunk->dataSize != BKGD_DATA_LENGTH_TYPE_3)))	{
		fputs("bKGD chunk length is invalid.\n", stderr);
		return FALSE;
	}

	if (chunk->dataSize == BKGD_DATA_LENGTH_TYPE_0_4) {
		unsigned int Greyscale = GetLittleEndianWord(chunk->Data);
		printf("Background:\n\tGreyscale:%u\n",Greyscale);
	}
	else if (chunk->dataSize == BKGD_DATA_LENGTH_TYPE_2_6) {
		unsigned int Red = GetLittleEndianWord(chunk->Data);
		unsigned int Green = GetLittleEndianWord(chunk->Data + 2);
		unsigned int Blue = GetLittleEndianWord(chunk->Data + 4);
		printf("Background:\n\tRed:%u\n\tGreen:%u\n\tBlue:%u\n",Red,Green,Blue);
	}
	else if (chunk->dataSize == BKGD_DATA_LENGTH_TYPE_3) {
		unsigned int Palette_index = chunk->Data[0];
		printf("Background:\n\tPalette index:%u\n",Palette_index);
	}
	return TRUE;

}

int processPHYSChunk(const Chunk *chunk) {
	unsigned int x;
	unsigned int y;
	unsigned int unit;

	if (chunk->dataSize != PHY_DATA_LENGTH)	{
		fputs("pHYs chunk length is invalid.\n", stderr);
		return FALSE;
	}

	x = GetLittleEndianInteger(chunk->Data);
	y = GetLittleEndianInteger(chunk->Data+4);
	if (x > PNG_MAX_VALUE || y > PNG_MAX_VALUE) {
		fputs("pHYs chunk data is invalid.\n", stderr);
		return FALSE;
	}

	unit = chunk->Data[8];
	if ((unit != 1) && (unit != 0))	{
		fputs("pHYs chunk data is invalid.\n", stderr);
		return FALSE;
	}

	if (unit == 1)
		printf("Physic:\n\tPixel per units in x axis %u\n\tPixel per units in y axis %u\n\tUnit value is the metre\n",x,y);
	else
		printf("Physic:\n\tPixel per units in x axis %u\n\tPixel per units in y axis %u\n\tUnit value unknown\n",x,y);

	return TRUE;
}

int processPLTEChunk(const Chunk *chunk) {
	unsigned int index;
	unsigned int size = chunk->dataSize / 3;
	
	if ((chunk->dataSize == 0 ) || ((chunk->dataSize % 3) != 0) || ((chunk->dataSize / 3) > PLTE_DATA_LENGTH)) {
		fputs("PLTE chunk length is invalid.\n", stderr);
		return FALSE;
	}
	printf("PLTE data:\n");
	for(index = 0; index < size; index++){
		unsigned int r = chunk->Data[0 + index * 3];
		unsigned int g = chunk->Data[1 + index * 3];
		unsigned int b = chunk->Data[2 + index * 3];
		printf("Palette index %u:\tR:\t%u\tG:\t%u\tB:\t%u\n",(unsigned int)index,r,g,b);
	}
	return TRUE;
}
int processICCPChunk(const Chunk *chunk) {
	unsigned int profile_name_length;
	unsigned int index;
	unsigned int compressed_length;
	const unsigned char * compressed_data;
	unsigned int compression_method;
	const unsigned char *NullBytePtr = memchr(chunk->Data, 0x00, chunk->dataSize);
	if (!NullBytePtr) {
		fputs("iCCP chunk is invalid.\n", stderr);
		return FALSE;
	}
	profile_name_length = NullBytePtr - chunk->Data;
	if ((profile_name_length < 1) || (profile_name_length > 79)) {
		fputs("iCCP Profile name length is invalid.\n", stderr);
		return FALSE;
	}
	
	for (index = 0; index < profile_name_length; index++) {
		if (((32 <= chunk->Data[index]) && (chunk->Data[index] <= 126)) || ((161 <= chunk->Data[index]))) {}
		else{
			fputs("iCCP Profile text is invalid.\n", stderr);
			return FALSE;
		}
	}

	
	if ((chunk->Data[0] == ' ') || (chunk->Data[profile_name_length - 1] == ' ')) {
		fputs("iCCP Profile text is invalid.\n", stderr);
		return FALSE;
	}

	
	for (index = 0; index < profile_name_length - 1; index++) {
		if ((chunk->Data[index] == ' ') && (chunk->Data[index+1] == ' ')) {
			fputs("iCCP Profile text is invalid.\n", stderr);
			return FALSE;
		}
	}

	compressed_data = chunk->Data + profile_name_length + 1;
	compressed_length = chunk->dataSize - (profile_name_length + 1);
	if (compressed_length < 1) {
		fputs("iCCP Data length is invalid.\n", stderr);
		return FALSE;
	}
	compression_method = chunk->Data[profile_name_length + 1];
	if (compression_method != 0) {
		fputs("iCCP Data compression method is invalid.\n", stderr);
		return FALSE;
	}

	printf("iCCP data:\n");
	
	printf("\tProfile name:%s\n", chunk->Data);
	
	printf("\tCompression method (0=zlib):%u\n",compression_method);
	
	printf("\tCompressed data:\n\t\t\t");
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

int processSRGBChunk(const Chunk *chunk) {
	unsigned int intent;

	if (chunk->dataSize != SRGB_DATA_LENGTH) {
		fputs("sRGB chunk length is invalid.\n", stderr);
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
		fputs("sRGB value is invalid.\n", stderr);
		return FALSE;
		break;
	}
	return TRUE;
}
int processSBITChunk(const Chunk *chunk, unsigned int ColorType) {
	unsigned int greyscale;
	unsigned int r;
	unsigned int g;
	unsigned int b;
	unsigned int alpha;
	if (((ColorType == 0 ) && (chunk->dataSize != SBIT_DATA_LENGTH_TYPE_0)) ||
			((ColorType == 2 || ColorType == 3) && (chunk->dataSize != SBIT_DATA_LENGTH_TYPE_2_3)) ||
			((ColorType == 4) && (chunk->dataSize != SBIT_DATA_LENGTH_TYPE_4)) ||
			((ColorType == 6) && (chunk->dataSize != SBIT_DATA_LENGTH_TYPE_6)))	{
		fputs("sBIT chunk length is invalid.\n", stderr);
		return FALSE;
	}

	switch(chunk->dataSize){
	case SBIT_DATA_LENGTH_TYPE_0:
		greyscale = chunk->Data[0];
		printf("sBIT:\n\tGreyscale %u\n",greyscale);
		break;
	case SBIT_DATA_LENGTH_TYPE_2_3:
		r = chunk->Data[0];
		g = chunk->Data[1];
		b = chunk->Data[2];
		printf("sBIT:\n\tR: %u\tG: %u\tB: %u\n",r,g,b);
		break;
	case SBIT_DATA_LENGTH_TYPE_4:
		greyscale = chunk->Data[0];
		alpha = chunk->Data[1];
		printf("sBIT:\n\tGreyscale: %u\tAlpha: %u\n",greyscale,alpha);
		break;
	case SBIT_DATA_LENGTH_TYPE_6:
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
int processZTXTChunk(const Chunk *chunk) {
	
	unsigned int keyword_length;
	unsigned int compressed_length;
	const unsigned char *compressed_data;
	unsigned int index;
	unsigned int compression_method;
	const unsigned char *NullBytePtr = memchr(chunk->Data, 0x00, chunk->dataSize);
	if (!NullBytePtr) {
		fputs("zTXt chunk is invalid.\n", stderr);
		return FALSE;
	}
	keyword_length = NullBytePtr - chunk->Data;
	if ((keyword_length < 1) || (keyword_length > 79)) {
		fputs("zTXt Keyword length is invalid.\n", stderr);
		return FALSE;
	}
	compressed_data = chunk->Data + keyword_length + 1;
	compressed_length = chunk->dataSize - (keyword_length + 1);
	if (compressed_length < 1) {
		fputs("zTXt Data length is invalid.\n", stderr);
		return FALSE;
	}
	compression_method = chunk->Data[keyword_length + 1];
	if (compression_method != 0) {
		fputs("zTXt Data compression method is invalid.\n", stderr);
		return FALSE;
	}
	printf("zTXt data:\n");

	printf("\tKeyword:%s\n", chunk->Data);
	
	printf("\tCompression method (0=zlib):%u\n",compression_method);
	
	printf("\tCompressed data:\n\t\t\t");
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