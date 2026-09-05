// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h> // chdir()
#include <sys/stat.h>
#endif
#include "ft2_header.h"
#include "ft2_gui.h"
#include "ft2_sample_ed.h"
#include "ft2_diskop.h"
#include "ft2_exs_manifest.h"
#include "ft2_mouse.h"
#include "ft2_structs.h"

typedef struct wavHeader_t
{
	uint32_t chunkID, chunkSize, format, subchunk1ID, subchunk1Size;
	uint16_t audioFormat, numChannels;
	uint32_t sampleRate, byteRate;
	uint16_t blockAlign, bitsPerSample;
	uint32_t subchunk2ID, subchunk2Size;
} wavHeader_t;

typedef struct sampleLoop_t
{
	uint32_t dwIdentifier, dwType, dwStart, dwEnd, dwFraction, dwPlayCount;
} sampleLoop_t;

typedef struct samplerChunk_t
{
	uint32_t chunkID, chunkSize, dwManufacturer, dwProduct, dwSamplePeriod;
	uint32_t dwMIDIUnityNote, dwMIDIPitchFraction, dwSMPTEFormat;
	uint32_t dwSMPTEOffset, cSampleLoops, cbSamplerData;
	sampleLoop_t loop;
} samplerChunk_t;

typedef struct mptExtraChunk_t
{
	uint32_t chunkID, chunkSize, flags;
	uint16_t defaultPan, defaultVolume, globalVolume, reserved;
	uint8_t vibratoType, vibratoSweep, vibratoDepth, vibratoRate;
} mptExtraChunk_t;

static const char *rangedDataStr = "Ranged data from FT2";

// thread data
static bool saveRangeFlag;
static SDL_Thread *thread;

// restores modified interpolation tap samples after loopEnd (for .RAW/.IFF/.WAV samples after save)
static void fileRestoreFixedSampleData(UNICHAR *filenameU, uint32_t sampleDataOffset, sample_t *s)
{
	if (!s->isFixed)
		return; // nothing to restore

	int32_t sampleFixPos = s->fixedPos;
	int32_t sampleFixOffset = 0;
	int32_t samplesToWrite = MAX_RIGHT_TAPS;

	if (saveRangeFlag)
	{
		const int32_t markStart = getSampleRangeStart();
		const int32_t markEnd = getSampleRangeEnd();

		if (markStart > sampleFixPos+MAX_RIGHT_TAPS || markEnd < sampleFixPos)
			return; // nothing to do here

		if (markStart > sampleFixPos)
		{
			sampleFixOffset += markStart-sampleFixPos;
			samplesToWrite -= markStart-sampleFixPos;
		}

		sampleFixPos -= markStart;

		if (sampleFixPos + samplesToWrite > markEnd)
			samplesToWrite = markEnd - sampleFixPos;

		if (samplesToWrite < 0 || samplesToWrite > MAX_RIGHT_TAPS || sampleFixPos < 0 || sampleFixOffset < 0 || sampleFixOffset >= MAX_RIGHT_TAPS)
			return;
	}

	FILE* f = UNICHAR_FOPEN(filenameU, "r+"); // open in read+update mode
	if (f == NULL)
		return;

	bool sample16Bit = !!(s->flags & SAMPLE_16BIT);
	if (sample16Bit)
		fseek(f, sampleDataOffset + (sampleFixPos * 2), SEEK_SET);
	else
		fseek(f, sampleDataOffset + sampleFixPos, SEEK_SET);

	if (sample16Bit)
	{
		fwrite(&s->fixedSmp[sampleFixOffset], sizeof (int16_t), samplesToWrite, f);
	}
	else
	{
		for (int32_t i = 0; i < samplesToWrite; i++)
		{
			int8_t fixedSmp = (int8_t)s->fixedSmp[sampleFixOffset+i];
			if (editor.sampleSaveMode == SMP_SAVE_MODE_WAV) // on 8-bit WAVs the sample data is unsigned
				fixedSmp ^= 0x80; // signed -> unsigned

			fwrite(&fixedSmp, sizeof (int8_t), 1, f);
		}
	}

	fclose(f);
}

static bool saveRawSample(UNICHAR *filenameU, bool saveRangedData)
{
	int8_t *samplePtr;
	uint32_t sampleLen;

	instr_t *ins = instr[editor.curInstr];
	if (ins == NULL || ins->smp[editor.curSmp].dataPtr == NULL || ins->smp[editor.curSmp].length == 0)
	{
		okBoxThreadSafe(0, "System message", "The sample is empty!", NULL);
		return false;
	}

	sample_t *smp = &instr[editor.curInstr]->smp[editor.curSmp];
	bool sample16Bit = !!(smp->flags & SAMPLE_16BIT);

	if (saveRangedData)
	{
		samplePtr = &smp->dataPtr[getSampleRangeStart() << sample16Bit];
		sampleLen = getSampleRangeLength();
	}
	else
	{
		sampleLen = smp->length;
		samplePtr = smp->dataPtr;
	}

	FILE *f = UNICHAR_FOPEN(filenameU, "wb");
	if (f == NULL)
	{
		okBoxThreadSafe(0, "System message", "General I/O error during saving! Is the file in use?", NULL);
		return false;
	}

	if (fwrite(samplePtr, sampleLen, 1, f) != 1)
	{
		fclose(f);
		okBoxThreadSafe(0, "System message", "Error saving sample: General I/O error!", NULL);
		return false;
	}

	fclose(f);

	// restore modified interpolation tap samples after loopEnd
	bool loopEnabled = GET_LOOPTYPE(smp->flags) != LOOP_DISABLED;
	if (loopEnabled && smp->length > smp->loopStart+smp->loopLength)
		fileRestoreFixedSampleData(filenameU, 0, smp);

	editor.diskOpReadDir = true; // force diskop re-read

	setMouseBusy(false);
	return true;
}

static void iffWriteChunkHeader(FILE *f, char *chunkName, uint32_t chunkLen)
{
	fwrite(chunkName, sizeof (int32_t), 1, f);
	chunkLen = SWAP32(chunkLen);
	fwrite(&chunkLen, sizeof (int32_t), 1, f);
}

static void iffWriteUint32(FILE *f, uint32_t value)
{
	value = SWAP32(value);
	fwrite(&value, sizeof (int32_t), 1, f);
}

static void iffWriteUint16(FILE *f, uint16_t value)
{
	value = SWAP16(value);
	fwrite(&value, sizeof (int16_t), 1, f);
}

static void iffWriteUint8(FILE *f, const uint8_t value)
{
	fwrite(&value, sizeof (int8_t), 1, f);
}

static void iffWriteChunkData(FILE *f, const void *data, size_t length)
{
	fwrite(data, sizeof (int8_t), length, f);
	if (length & 1) fputc(0, f); // write pad byte if chunk size is uneven
}

static bool saveIFFSample(UNICHAR *filenameU, bool saveRangedData)
{
	char *smpNamePtr;
	int8_t *samplePtr;
	uint32_t sampleLen, smpNameLen, chunkLen;

	instr_t *ins = instr[editor.curInstr];
	if (ins == NULL || ins->smp[editor.curSmp].dataPtr == NULL || ins->smp[editor.curSmp].length == 0)
	{
		okBoxThreadSafe(0, "System message", "The sample is empty!", NULL);
		return false;
	}

	sample_t *smp = &instr[editor.curInstr]->smp[editor.curSmp];

	FILE *f = UNICHAR_FOPEN(filenameU, "wb");
	if (f == NULL)
	{
		okBoxThreadSafe(0, "System message", "General I/O error during saving! Is the file in use?", NULL);
		return false;
	}

	bool sample16Bit = !!(smp->flags & SAMPLE_16BIT);

	if (saveRangedData)
	{
		samplePtr = &smp->dataPtr[getSampleRangeStart() << sample16Bit];
		sampleLen = getSampleRangeLength();
	}
	else
	{
		sampleLen = smp->length;
		samplePtr = smp->dataPtr;
	}

	// "FORM" chunk
	iffWriteChunkHeader(f, "FORM", 0); // "FORM" chunk size is overwritten later
	iffWriteUint32(f, sample16Bit ? 0x31365356 : 0x38535658); // bitdepth - "16SV" (16-bit) or "8SVX" (8-bit)

	// "VHDR" chunk
	iffWriteChunkHeader(f, "VHDR", 20);

	if (!saveRangedData && GET_LOOPTYPE(smp->flags) != LOOP_DISABLED)
	{
		iffWriteUint32(f, smp->loopStart << sample16Bit); // oneShotHiSamples
		iffWriteUint32(f, smp->loopLength << sample16Bit); // repeatHiSamples
	}
	else
	{
		iffWriteUint32(f, 0); // oneShotHiSamples
		iffWriteUint32(f, 0); // repeatHiSamples
	}

	iffWriteUint32(f, 0); // samplesPerHiCycle

	// samplesPerSec
	uint32_t tmp32 = getSampleC4Hz(smp);
	if (tmp32 == 0 || tmp32 > 65535) tmp32 = 16726;
	iffWriteUint16(f, (uint16_t)tmp32);

	iffWriteUint8(f, 1); // ctOctave (number of samples)
	iffWriteUint8(f, 0); // sCompression
	iffWriteUint32(f, smp->volume * 1024); // volume (max: 65536/0x10000)

	// "NAME" chunk

	if (saveRangedData)
	{
		smpNamePtr = (char *)rangedDataStr;
		smpNameLen = (uint32_t)strlen(rangedDataStr);
	}
	else
	{
		smpNamePtr = smp->name;

		smpNameLen = 0;
		while (smpNameLen < 22)
		{
			if (smpNamePtr[smpNameLen] == '\0')
				break;

			smpNameLen++;
		}
	}

	// "NAME" chunk
	chunkLen = smpNameLen;
	if (chunkLen > 0)
	{
		iffWriteChunkHeader(f, "NAME", chunkLen);
		iffWriteChunkData(f, smpNamePtr, chunkLen);
	}

	// "ANNO" chunk (we put the program name here)
	chunkLen = sizeof (PROG_NAME_STR) - 1;
	iffWriteChunkHeader(f, "ANNO", chunkLen);
	iffWriteChunkData(f, PROG_NAME_STR, chunkLen);

	// "BODY" chunk
	chunkLen = sampleLen << sample16Bit;
	iffWriteChunkHeader(f, "BODY", chunkLen);
	const uint32_t sampleDataPos = ftell(f);
	iffWriteChunkData(f, samplePtr, chunkLen);

	// go back and fill in "FORM" chunk size
	chunkLen = ftell(f) - 8;
	fseek(f, 4, SEEK_SET);
	iffWriteUint32(f, chunkLen);

	fclose(f);

	// restore modified interpolation tap samples after loopEnd
	bool loopEnabled = GET_LOOPTYPE(smp->flags) != LOOP_DISABLED;
	if (loopEnabled && smp->length > smp->loopStart+smp->loopLength)
		fileRestoreFixedSampleData(filenameU, sampleDataPos, smp);

	editor.diskOpReadDir = true; // force diskop re-read

	setMouseBusy(false);
	return true;
}

/*
** Low-level WAV writer.
**
** Unlike saveWAVSample(), this helper does not depend on the currently
** selected instrument or sample. This makes it reusable by operations
** such as Export Sample Set, which must walk arbitrary sample slots
** without changing the editor selection.
*/
static bool saveWAVSampleFromPointers(UNICHAR *filenameU, instr_t *ins,
	sample_t *smp, bool saveRangedData)
{
	char *smpNamePtr;
	int8_t *samplePtr;
	uint32_t i, sampleLen, riffChunkSize, smpNameLen, tmpLen;
	wavHeader_t wavHeader;
	samplerChunk_t samplerChunk;
	mptExtraChunk_t mptExtraChunk;

	if (ins == NULL || smp == NULL || smp->dataPtr == NULL || smp->length == 0)
	{
		okBoxThreadSafe(0, "System message", "The sample is empty!", NULL);
		return false;
	}

	FILE *f = UNICHAR_FOPEN(filenameU, "wb");
	if (f == NULL)
	{
		okBoxThreadSafe(0, "System message", "General I/O error during saving! Is the file in use?", NULL);
		return false;
	}

	bool sample16Bit = !!(smp->flags & SAMPLE_16BIT);

	if (saveRangedData)
	{
		samplePtr = &smp->dataPtr[getSampleRangeStart() << sample16Bit];
		sampleLen = getSampleRangeLength();
	}
	else
	{
		sampleLen = smp->length;
		samplePtr = smp->dataPtr;
	}

	const uint8_t sampleBitDepth = sample16Bit ? 16 : 8;

	wavHeader.chunkID = 0x46464952; // "RIFF"
	wavHeader.chunkSize = 0; // is filled later
	wavHeader.format = 0x45564157; // "WAVE"
	wavHeader.subchunk1ID = 0x20746D66; // "fmt "
	wavHeader.subchunk1Size = 16;
	wavHeader.audioFormat = 1;
	wavHeader.numChannels = 1;
	wavHeader.sampleRate = getSampleC4Hz(smp);
	wavHeader.byteRate = (wavHeader.sampleRate * wavHeader.numChannels * sampleBitDepth) / 8;
	wavHeader.blockAlign = (wavHeader.numChannels * sampleBitDepth) / 8;
	wavHeader.bitsPerSample = sampleBitDepth;
	wavHeader.subchunk2ID = 0x61746164; // "data"
	wavHeader.subchunk2Size = sampleLen << sample16Bit;

	// write main header
	fwrite(&wavHeader, sizeof (wavHeader_t), 1, f);

	// write sample data
	const uint32_t sampleDataPos = ftell(f);
	if (sampleBitDepth == 16)
	{
		fwrite((int16_t *)samplePtr, sizeof (int16_t), sampleLen, f);
	}
	else
	{
		for (i = 0; i < sampleLen; i++)
			fputc(samplePtr[i] ^ 0x80, f); // write as unsigned 8-bit data
	}

	if (wavHeader.subchunk2Size & 1)
		fputc(0, f); // write pad byte if chunk size is uneven

	// write "smpl" chunk if loop is enabled
	if (!saveRangedData && GET_LOOPTYPE(smp->flags) != LOOP_DISABLED)
	{
		memset(&samplerChunk, 0, sizeof (samplerChunk));

		samplerChunk.chunkID = 0x6C706D73; // "smpl"
		samplerChunk.chunkSize = sizeof (samplerChunk) - 4 - 4;
		samplerChunk.dwSamplePeriod = 1000000000 / wavHeader.sampleRate;
		samplerChunk.dwMIDIUnityNote = 60; // 60 = MIDI middle-C
		samplerChunk.cSampleLoops = 1;
		samplerChunk.loop.dwType = (smp->flags & SAMPLE_REVERSE_LOOP) != 0
			? 2 : GET_LOOPTYPE(smp->flags)-1; // 0 forward, 1 ping-pong, 2 backward
		samplerChunk.loop.dwStart = smp->loopStart;
		samplerChunk.loop.dwEnd = (smp->loopStart + smp->loopLength) - 1;

		fwrite(&samplerChunk, sizeof (samplerChunk), 1, f);
		if (samplerChunk.chunkSize & 1)
			fputc(0, f); // write pad byte if chunk size is uneven
	}

	// write modplug tracker "xtra" chunk
	if (!saveRangedData)
	{
		memset(&mptExtraChunk, 0, sizeof (mptExtraChunk));

		mptExtraChunk.chunkID = 0x61727478; // "xtra"
		mptExtraChunk.chunkSize = sizeof (mptExtraChunk) - 4 - 4;
		mptExtraChunk.flags = 0x20; // set pan flag
		mptExtraChunk.defaultPan = smp->panning; // 0..255
		mptExtraChunk.defaultVolume = smp->volume * 4; // 0..256
		mptExtraChunk.globalVolume = 64; // 0..64
		mptExtraChunk.vibratoType = ins->autoVibType; // 0..3    0 = sine, 1 = square, 2 = ramp up, 3 = ramp down
		mptExtraChunk.vibratoSweep = ins->autoVibSweep; // 0..255
		mptExtraChunk.vibratoDepth = ins->autoVibDepth; // 0..15
		mptExtraChunk.vibratoRate = ins->autoVibRate; // 0..63

		fwrite(&mptExtraChunk, sizeof (mptExtraChunk), 1, f);
		if (mptExtraChunk.chunkSize & 1)
			fputc(0, f); // write pad byte if chunk size is uneven
	}

	// write LIST->INFO->INAM chunk

	if (saveRangedData)
	{
		smpNamePtr = (char *)rangedDataStr;
		smpNameLen = (uint32_t)strlen(smpNamePtr);
	}
	else
	{
		smpNamePtr = smp->name;

		smpNameLen = 0;
		while (smpNameLen < 22)
		{
			if (smpNamePtr[smpNameLen] == '\0')
				break;

			smpNameLen++;
		}
	}

	const uint32_t progNameLen = sizeof (PROG_NAME_STR) - 1;

	tmpLen = 4 + (4 + 4) + (progNameLen + 1 + ((progNameLen + 1) & 1));
	if (smpNameLen > 0)
		tmpLen += ((4 + 4) + (progNameLen + 1 + ((progNameLen + 1) & 1)));

	fwrite("LIST", sizeof (int32_t), 1, f);
	fwrite(&tmpLen, sizeof (int32_t), 1, f);
	fwrite("INFO", sizeof (int32_t), 1, f);

	if (smpNameLen > 0)
	{
		tmpLen = smpNameLen + 1;
		fwrite("INAM", sizeof (int32_t), 1, f);
		fwrite(&tmpLen, sizeof (int32_t), 1, f);
		fwrite(smpNamePtr, 1, smpNameLen, f);
		fputc(0, f); // string termination
		if (tmpLen & 1)
			fputc(0, f); // pad byte
	}

	tmpLen = progNameLen + 1;
	fwrite("ISFT", sizeof (int32_t), 1, f);
	fwrite(&tmpLen, sizeof (int32_t), 1, f);
	fwrite(PROG_NAME_STR, 1, progNameLen, f);
	fputc(0, f); // string termination
	if (tmpLen & 1)
		fputc(0, f); // pad byte

	// go back and fill in "RIFF" chunk size
	riffChunkSize = ftell(f) - 8;
	fseek(f, 4, SEEK_SET);
	fwrite(&riffChunkSize, sizeof (int32_t), 1, f);

	const bool writeFailed = ferror(f) || fclose(f) != 0;
	if (writeFailed)
	{
		UNICHAR_REMOVE(filenameU);
		setMouseBusy(false);
		okBoxThreadSafe(0, "System message", "General I/O error during saving!", NULL);
		return false;
	}

	// restore modified interpolation tap samples after loopEnd
	bool loopEnabled = GET_LOOPTYPE(smp->flags) != LOOP_DISABLED;
	if (loopEnabled && smp->length > smp->loopStart+smp->loopLength)
		fileRestoreFixedSampleData(filenameU, sampleDataPos, smp);

	editor.diskOpReadDir = true; // force diskop re-read

	setMouseBusy(false);
	return true;
}

bool saveWAVSampleDirect(const UNICHAR *filenameU, instr_t *instrument,
	sample_t *sample)
{
	if (filenameU == NULL)
		return false;
	return saveWAVSampleFromPointers((UNICHAR *)filenameU, instrument, sample,
		false);
}

/*
** Original editor-facing WAV saver.
**
** Keep the existing behavior for Save Sample while forwarding the actual
** writing work to the pointer-based helper above.
*/
static bool saveWAVSample(UNICHAR *filenameU, bool saveRangedData)
{
	instr_t *ins = instr[editor.curInstr];
	if (ins == NULL)
	{
		okBoxThreadSafe(0, "System message", "The sample is empty!", NULL);
		return false;
	}

	sample_t *smp = &ins->smp[editor.curSmp];
	return saveWAVSampleFromPointers(filenameU, ins, smp, saveRangedData);
}

static bool exsUsedOnly = true;

void setEXSExportUsedOnly(bool usedOnly)
{
	exsUsedOnly = usedOnly;
}

static void exsSafeName(const char *src, char *dst, size_t dstSize, const char *fallback)
{
	size_t j = 0;
	while (*src == ' ')
		src++;

	for (size_t i = 0; src[i] != '\0' && j+1 < dstSize && j < 48; i++)
	{
		const uint8_t c = (uint8_t)src[i];
		if (c < 32 || strchr("\\/:*?\"<>|", c) != NULL)
			dst[j++] = '_';
		else
			dst[j++] = (char)c;
	}

	while (j > 0 && (dst[j-1] == ' ' || dst[j-1] == '.'))
		j--;
	dst[j] = '\0';

	if (j == 0)
	{
		strncpy(dst, fallback, dstSize-1);
		dst[dstSize-1] = '\0';
	}
}

static void exsManifestValue(const char *src, char *dst, size_t dstSize)
{
	size_t j = 0;
	for (size_t i = 0; src[i] != '\0' && j+1 < dstSize; i++)
	{
		const uint8_t c = (uint8_t)src[i];
		dst[j++] = c < 32 || c == 127 ? ' ' : (char)c;
	}
	dst[j] = '\0';
}

static bool exsJoinPath(UNICHAR *dst, size_t dstCount, const UNICHAR *root,
	const char *child)
{
#ifdef _WIN32
	UNICHAR *childU = cp850ToUnichar((char *)child);
	if (childU == NULL)
	{
		dst[0] = '\0';
		return false;
	}
	const size_t rootLen = wcslen(root);
	const size_t childLen = wcslen(childU);
	if (rootLen + 1 + childLen >= dstCount)
	{
		free(childU);
		dst[0] = '\0';
		return false;
	}
	wmemcpy(dst, root, rootLen);
	dst[rootLen] = L'\\';
	wmemcpy(&dst[rootLen+1], childU, childLen+1);
	free(childU);
#else
	const size_t rootLen = strlen(root);
	const size_t childLen = strlen(child);
	if (rootLen + 1 + childLen >= dstCount)
	{
		dst[0] = '\0';
		return false;
	}
	memcpy(dst, root, rootLen);
	dst[rootLen] = '/';
	memcpy(&dst[rootLen+1], child, childLen+1);
#endif
	return true;
}

static bool exsMakeDirectory(const UNICHAR *path)
{
#ifdef _WIN32
	return _wmkdir(path) == 0;
#else
	return mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == 0;
#endif
}

static void exsRemoveDirectory(const UNICHAR *path)
{
#ifdef _WIN32
	_wrmdir(path);
#else
	rmdir(path);
#endif
}

static void exsFindUsedInstruments(bool used[MAX_INST+1])
{
	memset(used, 0, (MAX_INST+1) * sizeof (bool));
	const int32_t orderCount = CLAMP(song.songLength, 0, MAX_ORDERS);
	const int32_t channelCount = CLAMP(song.numChannels, 0, MAX_CHANNELS);

	for (int32_t order = 0; order < orderCount; order++)
	{
		const uint8_t pattNum = song.orders[order];
		if (pattern[pattNum] == NULL)
			continue;

		const int32_t rows = CLAMP(patternNumRows[pattNum], 0, MAX_PATT_LEN);
		for (int32_t row = 0; row < rows; row++)
		{
			const note_t *p = &pattern[pattNum][row * MAX_CHANNELS];
			for (int32_t ch = 0; ch < channelCount; ch++)
			{
				if (p[ch].instr >= 1 && p[ch].instr <= MAX_INST)
					used[p[ch].instr] = true;
			}
		}
	}
}

static const char *exsLoopName(const sample_t *smp)
{
	switch (GET_LOOPTYPE(smp->flags))
	{
		case LOOP_FORWARD: return "Forward";
		case LOOP_PINGPONG: return "PingPong";
		default: return "None";
	}
}

static void exsInstrumentDirectoryName(int32_t instrNum, char *dst, size_t dstSize)
{
	char name[64];
	exsSafeName(song.instrName[instrNum], name, sizeof (name), "Unnamed");
	snprintf(dst, dstSize, "instrument_%02d_%s", instrNum, name);
}

static void exsSampleFilename(int32_t instrNum, int32_t sampleNum,
	const sample_t *smp, char *dst, size_t dstSize)
{
	char name[64];
	exsSafeName(smp->name, name, sizeof (name), "Unnamed");
	exsStripKnownSampleExtension(name);
	if (name[0] == '\0')
		strcpy(name, "Unnamed");
	snprintf(dst, dstSize, "I%02d_S%02d_%s.wav", instrNum, sampleNum, name);
}

static int32_t exsInstrumentSampleCount(int32_t instrNum)
{
	if (instrNum < 1 || instrNum > MAX_INST || instr[instrNum] == NULL)
		return 0;

	int32_t count = 0;
	for (int32_t sampleNum = 0; sampleNum < MAX_SMP_PER_INST; sampleNum++)
	{
		const sample_t *smp = &instr[instrNum]->smp[sampleNum];
		if (smp->dataPtr != NULL && smp->length > 0)
			count++;
	}

	return count;
}

static void exsSampleRelativePath(int32_t instrNum, int32_t sampleNum,
	const sample_t *smp, int32_t instrumentSampleCount, char *dst, size_t dstSize)
{
	char sampleFile[128];
	exsSampleFilename(instrNum, sampleNum, smp, sampleFile, sizeof (sampleFile));
	if (instrumentSampleCount == 1)
	{
		strncpy(dst, sampleFile, dstSize-1);
		dst[dstSize-1] = '\0';
	}
	else
	{
		char instrDir[128];
		exsInstrumentDirectoryName(instrNum, instrDir, sizeof (instrDir));
		snprintf(dst, dstSize, "%s/%s", instrDir, sampleFile);
	}
}

static void exsCleanup(const UNICHAR *root, const bool selected[MAX_INST+1])
{
	char instrDir[128], relative[300];
	UNICHAR path[PATH_MAX+1];

	for (int32_t instrNum = 1; instrNum <= MAX_INST; instrNum++)
	{
		if (!selected[instrNum] || instr[instrNum] == NULL)
			continue;

		const int32_t instrumentSampleCount = exsInstrumentSampleCount(instrNum);
		for (int32_t sampleNum = 0; sampleNum < MAX_SMP_PER_INST; sampleNum++)
		{
			sample_t *smp = &instr[instrNum]->smp[sampleNum];
			if (smp->dataPtr == NULL || smp->length <= 0)
				continue;
			exsSampleRelativePath(instrNum, sampleNum, smp, instrumentSampleCount,
				relative, sizeof (relative));
			if (exsJoinPath(path, PATH_MAX+1, root, relative))
				UNICHAR_REMOVE(path);
		}

		if (instrumentSampleCount > 1)
		{
			exsInstrumentDirectoryName(instrNum, instrDir, sizeof (instrDir));
			if (exsJoinPath(path, PATH_MAX+1, root, instrDir))
				exsRemoveDirectory(path);
		}
	}

	if (exsJoinPath(path, PATH_MAX+1, root, "Processed"))
		exsRemoveDirectory(path);
	if (exsJoinPath(path, PATH_MAX+1, root, "EXS_manifest.ini"))
		UNICHAR_REMOVE(path);
	exsRemoveDirectory(root);
}

bool exportSampleSet(const UNICHAR *directoryU, bool usedOnly)
{
	bool selected[MAX_INST+1];
	if (usedOnly)
		exsFindUsedInstruments(selected);
	else
	{
		memset(selected, 0, sizeof (selected));
		for (int32_t i = 1; i <= MAX_INST; i++)
			selected[i] = true;
	}

	int32_t instrumentCount = 0, sampleCount = 0;
	for (int32_t i = 1; i <= MAX_INST; i++)
	{
		if (!selected[i] || instr[i] == NULL)
			continue;

		const int32_t instrumentSamples = exsInstrumentSampleCount(i);

		if (instrumentSamples > 0)
		{
			instrumentCount++;
			sampleCount += instrumentSamples;
		}
	}

	if (sampleCount == 0)
	{
		okBoxThreadSafe(0, "EXS - Export XM Samples",
			usedOnly ? "No used instruments contain populated samples." :
			"The module contains no populated samples.", NULL);
		return false;
	}

	UNICHAR root[PATH_MAX+1], path[PATH_MAX+1];
	UNICHAR_STRNCPY(root, directoryU, PATH_MAX);
	root[PATH_MAX] = '\0';
	bool rootCreated = exsMakeDirectory(root);
	for (int32_t suffix = 2; !rootCreated && errno == EEXIST && suffix <= 999; suffix++)
	{
#ifdef _WIN32
		swprintf(root, PATH_MAX+1, L"%ls_%02d", directoryU, suffix);
#else
		snprintf(root, PATH_MAX+1, "%s_%02d", directoryU, suffix);
#endif
		rootCreated = exsMakeDirectory(root);
	}

	if (!rootCreated)
	{
		okBoxThreadSafe(0, "EXS - Export XM Samples", "Couldn't create the export directory.", NULL);
		return false;
	}
	if (!exsJoinPath(path, PATH_MAX+1, root, "Processed") ||
		!exsMakeDirectory(path))
	{
		exsRemoveDirectory(root);
		okBoxThreadSafe(0, "EXS - Export XM Samples",
			"Couldn't create the Processed output directory.", NULL);
		return false;
	}

	const bool oldSaveRangeFlag = saveRangeFlag;
	saveRangeFlag = false;
	setMouseBusy(true);

	bool success = true;
	char instrDir[128], relative[300];
	for (int32_t i = 1; i <= MAX_INST && success; i++)
	{
		if (!selected[i] || instr[i] == NULL)
			continue;

		const int32_t instrumentSampleCount = exsInstrumentSampleCount(i);
		if (instrumentSampleCount == 0)
			continue;

		if (instrumentSampleCount > 1)
		{
			exsInstrumentDirectoryName(i, instrDir, sizeof (instrDir));
			if (!exsJoinPath(path, PATH_MAX+1, root, instrDir) || !exsMakeDirectory(path))
			{
				success = false;
				break;
			}
		}

		for (int32_t s = 0; s < MAX_SMP_PER_INST; s++)
		{
			sample_t *smp = &instr[i]->smp[s];
			if (smp->dataPtr == NULL || smp->length <= 0)
				continue;

			exsSampleRelativePath(i, s, smp, instrumentSampleCount,
				relative, sizeof (relative));
			if (!exsJoinPath(path, PATH_MAX+1, root, relative) ||
				!saveWAVSampleFromPointers(path, instr[i], smp, false))
			{
				success = false;
				break;
			}
		}
	}

	FILE *manifest = NULL;
	if (success)
	{
		if (exsJoinPath(path, PATH_MAX+1, root, "EXS_manifest.ini"))
			manifest = UNICHAR_FOPEN(path, "wb");
		if (manifest == NULL)
			success = false;
	}

	if (success)
	{
		char sourceName[64];
		exsManifestValue(song.name[0] != '\0' ? song.name : "Untitled", sourceName, sizeof (sourceName));
		fprintf(manifest, "[EXS]\nFormatVersion=1\nSourceModule=%s.xm\nExportMode=%s\nInstrumentCount=%d\nSampleCount=%d\n\n",
			sourceName, usedOnly ? "UsedInstruments" : "AllInstruments", instrumentCount, sampleCount);

		for (int32_t i = 1; i <= MAX_INST; i++)
		{
			if (!selected[i] || instr[i] == NULL)
				continue;
			const int32_t instrumentSampleCount = exsInstrumentSampleCount(i);
			for (int32_t s = 0; s < MAX_SMP_PER_INST; s++)
			{
				sample_t *smp = &instr[i]->smp[s];
				if (smp->dataPtr == NULL || smp->length <= 0)
					continue;

				char insName[64], smpName[64];
				exsManifestValue(song.instrName[i], insName, sizeof (insName));
				exsManifestValue(smp->name, smpName, sizeof (smpName));
				exsSampleRelativePath(i, s, smp, instrumentSampleCount,
					relative, sizeof (relative));

				fprintf(manifest,
					"[Instrument%02d.Sample%02d]\nInstrumentIndex=%d\nSampleIndex=%d\n"
					"InstrumentName=%s\nSampleName=%s\nFile=%s\nLengthFrames=%d\n"
					"SourceBitDepth=%d\nRelativeNote=%d\nFinetune=%d\nDefaultVolume=%u\n"
					"DefaultPanning=%u\nLoopType=%s\nLoopStart=%d\nLoopLength=%d\n"
					"C4Frequency=%d\nFlags=%u\n\n",
					i, s, i, s, insName, smpName, relative, smp->length,
					(smp->flags & SAMPLE_16BIT) ? 16 : 8, smp->relativeNote, smp->finetune,
					smp->volume, smp->panning, exsLoopName(smp), smp->loopStart,
					smp->loopLength, getSampleC4Hz(smp), smp->flags);
			}
		}

		if (ferror(manifest) || fclose(manifest) != 0)
			success = false;
		manifest = NULL;
	}

	if (manifest != NULL)
		fclose(manifest);
	saveRangeFlag = oldSaveRangeFlag;
	setMouseBusy(false);

	if (!success)
	{
		exsCleanup(root, selected);
		okBoxThreadSafe(0, "EXS - Export XM Samples",
			"Export failed. The incomplete new export directory was removed.", NULL);
		return false;
	}

	editor.diskOpReadDir = true;
	char message[128];
	snprintf(message, sizeof (message), "Exported %d sample%s from %d instrument%s.",
		sampleCount, sampleCount == 1 ? "" : "s", instrumentCount,
		instrumentCount == 1 ? "" : "s");
	okBoxThreadSafe(0, "EXS - Export XM Samples", message, NULL);
	return true;
}

static int32_t saveSampleThread(void *ptr)
{
	if (editor.tmpFilenameU == NULL)
	{
		okBoxThreadSafe(0, "System message", "General I/O error during saving! Is the file in use?", NULL);
		return false;
	}

	const UNICHAR *oldPathU = getDiskOpCurPath();

	// in "save range mode", we must enter the sample directory
	if (saveRangeFlag)
		UNICHAR_CHDIR(getDiskOpSmpPath());

	switch (editor.sampleSaveMode)
	{
		         case SMP_SAVE_MODE_RAW: saveRawSample(editor.tmpFilenameU, saveRangeFlag); break;
		         case SMP_SAVE_MODE_IFF: saveIFFSample(editor.tmpFilenameU, saveRangeFlag); break;
		         case SMP_SAVE_MODE_EXS: exportSampleSet(editor.tmpFilenameU, exsUsedOnly); break;
		default: case SMP_SAVE_MODE_WAV: saveWAVSample(editor.tmpFilenameU, saveRangeFlag); break;
	}

	// set back old working directory if we changed it
	if (saveRangeFlag)
		UNICHAR_CHDIR(oldPathU);

	return true;

	(void)ptr;
}

void saveSample(UNICHAR *filenameU, bool saveAsRange)
{
	saveRangeFlag = saveAsRange;
	UNICHAR_STRCPY(editor.tmpFilenameU, filenameU);

	mouseAnimOn();
	thread = SDL_CreateThread(saveSampleThread, "sample save thread", NULL);
	if (thread == NULL)
	{
		okBoxThreadSafe(0, "System message", "Couldn't create thread!", NULL);
		return;
	}

	SDL_DetachThread(thread);
}
