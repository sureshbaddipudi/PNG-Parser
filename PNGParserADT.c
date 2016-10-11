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


