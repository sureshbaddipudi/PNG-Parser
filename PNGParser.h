
/*
 * PNGParser.h
 *
 *  Created on: 08-Oct-2016
 *      Author: suresh
 */

#ifndef PNGPARSER_H_
#define PNGPARSER_H_

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <stdint.h>
#include <limits.h>
#include <string.h>

#define TRUE 1
#define FALSE 0

#define PNG_MAX_VALUE ((1u << 31) - 1)

#define READ_BUFFER_SIZE	( 64 * 1024 )
static const unsigned char pngHeader[] = { 137, 80, 78, 71, 13, 10, 26, 10 };

#define PROCESS_PNG_HEADER 11
#define	PROCESS_CHUNK_HEADER 12
#define	PROCESS_CHUNK_DATA 13
#define	PROCESS_CHUNK_CRC 14

#define CHUNK_TYPE_LENGTH	4
#define IHDR_DATA_LENGTH	13
#define TIME_DATA_LENGTH	7

#define CHRM_DATA_LENGTH	32
#define GAMA_DATA_LENGTH	4

#define TEXT_DATA_KEY_LENGTH_MAX	79
#define BKGD_TYPE_0_AND_4_DATA_LENGTH	2
#define BKGD_TYPE_2_AND_6_DATA_LENGTH	6
#define BKGD_TYPE_3_DATA_LENGTH		1
#define PHY_DATA_LENGTH		9

#define PLTE_DATA_LENGTH	256
#define SRGB_DATA_LENGTH	1

#define SBIT_TYPE_0_DATA_LENGTH		1
#define SBIT_TYPE_2_AND_3_DATA_LENGTH	3
#define SBIT_TYPE_4_DATA_LENGTH		2
#define SBIT_TYPE_6_DATA_LENGTH		4


/*
 * Structure represents chunk type and its data
 */
struct chunk {
	unsigned char		chunkType[CHUNK_TYPE_LENGTH]; //store chunk w.r.t type
	size_t				dataSize; //chunk size
	const unsigned char *Data; // chunk data
};

typedef struct chunk Chunk;

/*
 * Structure to store available types of chunks and its color type
 */
struct chunkInfo {
	/*Critical Chunk Types*/
	int IHDR; //Image Header
	int IDAT; //Image Data
	int PLTE; // Palltte
	int IEND; //Image Trailer

	/*Ancillary Chunks*/
	int TRNS; /* Transparency Info */
	/*Color Space Information*/
	int ICCP; //Embedded ICC Profile
	int CHRM; //Primary Chromaticities and white point
	int GAMA; //image gamma
	int SRGB; // standard RGB color Space
	int SBIT; //significant Bits
	/*Textual Information*/
	int tEXt; //Textual Data
	int zTXt; // Compressed Textual Data
	/*Miscellanious Information*/
	int BKGD; //background color
	int HIST; //image histogram
	int PHYS; // Physical pixel dimensions
	int SPLT; // suggested paletter
	/*Timestamp Information*/
	int TIME;

	int lastChunkIEND; // last IEND
	int lastChunkIDAT; //last IDAT
	unsigned int colorType; //defined color types
};

typedef struct chunkInfo ChunkInfo;



int isChunkType(const unsigned char*, const char*);
int initChunkProcess( ChunkInfo*);
int isChunkTypeValid( const unsigned char*);
int processChunk( ChunkInfo*, const Chunk*);
int processLastChunk(ChunkInfo*);

/*
 * Structure to store the all fields of PNG File
 */


struct pngData {
	int	State; //State of the processing of the file
	unsigned char	chunkHeader[8]; //to store chunk header
	unsigned char	chunkCRC[4]; //to store chunk CRC
	size_t			chunkSize; //to store chunksize
	unsigned char	*chunkData; //to store chunk data
	size_t			bytesToCopy; // bytes to be copied to PNGData from file
	size_t			bytesCopied; // bytes copied to PNGData from File
	unsigned char	*bufferData; //buffer read from file
	ChunkInfo		chunkInfo;  // everything about chunk
};

typedef struct pngData PNGData;

int initPNGProcess(PNGData*);
int verifyAndProcessChunk(PNGData*);
int processBuffer(PNGData* , const unsigned char*, size_t);
int processCopiedData(PNGData*);
int processFinish(PNGData*);
void processGenericChunk(const Chunk*);
int isValidChunkOrder(ChunkInfo*, const Chunk*);
int isValidCrc( const unsigned char*, const unsigned char*, size_t, uint32_t);

int processIHDRChunk(const Chunk*, unsigned int*);
int processIENDChunk(const Chunk*);
int processTIMEChunk(const Chunk*);
int processCHRMChunk(const Chunk*);
int processGAMAChunk(const Chunk*);
int processTEXTChunk(const Chunk*);
int processBKGDChunk(const Chunk*, unsigned int);
int processPHYSChunk(const Chunk*);

int processPLTEChunk(const Chunk*);
int processICCPChunk(const Chunk*);
int processSRGBChunk(const Chunk*);
int processSBITChunk(const Chunk*, unsigned int);
int processZTXTChunk(const Chunk*);
void freeChunkData(PNGData*);
uint32_t getLastByte( const unsigned char*);
uint16_t getLastWord(const unsigned char*);

#endif /* PNGPARSER_H_ */

