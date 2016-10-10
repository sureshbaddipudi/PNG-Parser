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


#define CHUNK_TYPE_LENGTH	4
#define IHDR_DATA_LENGTH	13
#define TIME_DATA_LENGTH	7

#define CHRM_DATA_LENGTH	32
#define GAMA_DATA_LENGTH	4

#define TEXT_DATA_KEY_LENGTH_MAX	79
#define BKGD_DATA_LENGTH_TYPE_0_4	2
#define BKGD_DATA_LENGTH_TYPE_2_6	6
#define BKGD_DATA_LENGTH_TYPE_3		1
#define PHY_DATA_LENGTH		9

#define PLTE_DATA_LENGTH	256
#define SRGB_DATA_LENGTH	1

#define SBIT_DATA_LENGTH_TYPE_0		1
#define SBIT_DATA_LENGTH_TYPE_2_3	3
#define SBIT_DATA_LENGTH_TYPE_4		2
#define SBIT_DATA_LENGTH_TYPE_6		4

#define PROCESS_PNG_HEADER 11
#define	PROCESS_CHUNK_HEADER 12
#define	PROCESS_CHUNK_DATA 13
#define	PROCESS_CHUNK_CRC 14

#define PNG_MAX_VALUE ((1u << 31) - 1)

struct chunk {
	unsigned char		chunkType[CHUNK_TYPE_LENGTH];
	size_t				dataSize;
	const unsigned char *Data;
};

typedef struct chunk Chunk;

struct chunkInfo {
	int IHDR;
	int IDAT;
	int PLTE;
	int ICCP;
	int SRGB;
	int CHRM;
	int GAMA;
	int SBIT;
	int BKGD;
	int HIST;
	int TRNS;
	int PHYS;
	int TIME;
	int IEND;
	int lastChunkIEND;
	int lastChunkIDAT;

	unsigned int colorType;
};

typedef struct chunkInfo ChunkInfo;



int isChunkType(const unsigned char *ChunkType, const char *ChunkString);

int initChunkProcess( ChunkInfo *Context );
int processChunk( ChunkInfo *Context, const Chunk *chunk );
int processLastChunk( ChunkInfo *Context );



struct pngData {
	int	State;

	/* all chunk/headers contains exactly 8 bytes */
	unsigned char	Header[ 8 ];
	unsigned char	ChunkCrc[ 4 ];
	size_t			ChunkSize;
	unsigned char	*ChunkData;

	size_t			BytesToCopy;
	size_t			BytesCopied;
	unsigned char	*BufferData;

	ChunkInfo		chunkInfo;
};

typedef struct pngData PNGData;

int initPNGProcess( PNGData* Context );
int processBuffer( PNGData* Context, const unsigned char *Data, size_t DataLength );
int processFinish( PNGData* Context );
void processGenericChunk(const Chunk *chunk);
int isValidChunkOrder(ChunkInfo *Context, const Chunk *chunk);

#endif /* PNGPARSER_H_ */
