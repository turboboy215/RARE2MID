/*RARE (GB/GBC) to MIDI converter*/
/*By Will Trowbridge*/
/*Portions based on code by ValleyBell*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#define bankSize 16384

FILE* rom, * txt, * cfg;
long bank;
long tablePtrLoc;
long tableOffset;
long tempoTablePtrLoc;
long tempoTableOffset;
int i, j;
char outfile[1000000];
int songNum;
int numSongs;
long songPtrs[4];
long firstPtrs[4];
long curSpeed;
long bankAmt;
long nextPtr;
int masterBank;
int cfgPtr = 0;
int exitError = 0;
int fileExit = 0;
int foundTable = 0;
int ptrOverride = 0;
int bankOverride = 0;
long base = 0;
long subBase = 0;
int curTrack;
int tempoPos = 0;
int tempo = 0;
int format = 1;

const unsigned char MagicBytes[7] = { 0x69, 0x26, 0x00, 0x29, 0x29, 0x29, 0x01 };
const unsigned char MagicBytesTempo[8] = { 0xE0, 0x10, 0x18, 0xBD, 0x4F, 0xAF, 0x47, 0x21 };
const unsigned char MagicBytesTempo2[7] = { 0xE0, 0x10, 0xC9, 0x4F, 0x06, 0x00, 0x21 };
const unsigned char MagicBytesTempo3[8] = { 0xE0, 0x10, 0x18, 0xB9, 0x4F, 0xAF, 0x47, 0x21 };
const unsigned char MagicBytesTempo4[6] = { 0xE0, 0x1C, 0xE0, 0x21, 0x47, 0x21 };
const unsigned char MagicBytesTempo5[6] = { 0xD0, 0xC9, 0x4F, 0x06, 0x00, 0x21 };

char string1[100];
char string2[100];
char checkStrings[6][100] = { "masterBank=", "numSongs=", "bank=", "base=", "auto", "format="};
unsigned static char* romData;
unsigned static char* exRomData;
unsigned static char* cfgData;

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
unsigned short ReadBE16(unsigned char* Data);
static void Write8B(unsigned char* buffer, unsigned int value);
static void WriteBE32(unsigned char* buffer, unsigned long value);
static void WriteBE24(unsigned char* buffer, unsigned long value);
static void WriteBE16(unsigned char* buffer, unsigned int value);
void song2txt(int songNum, long ptrList[4]);

/*Convert little-endian pointer to big-endian*/
unsigned short ReadLE16(unsigned char* Data)
{
	return (Data[0] << 0) | (Data[1] << 8);
}

/*Store big-endian pointer*/
unsigned short ReadBE16(unsigned char* Data)
{
	return (Data[0] << 8) | (Data[1] << 0);
}

static void Write8B(unsigned char* buffer, unsigned int value)
{
	buffer[0x00] = value;
}

static void WriteBE32(unsigned char* buffer, unsigned long value)
{
	buffer[0x00] = (value & 0xFF000000) >> 24;
	buffer[0x01] = (value & 0x00FF0000) >> 16;
	buffer[0x02] = (value & 0x0000FF00) >> 8;
	buffer[0x03] = (value & 0x000000FF) >> 0;

	return;
}

static void WriteBE24(unsigned char* buffer, unsigned long value)
{
	buffer[0x00] = (value & 0xFF0000) >> 16;
	buffer[0x01] = (value & 0x00FF00) >> 8;
	buffer[0x02] = (value & 0x0000FF) >> 0;

	return;
}

static void WriteBE16(unsigned char* buffer, unsigned int value)
{
	buffer[0x00] = (value & 0xFF00) >> 8;
	buffer[0x01] = (value & 0x00FF) >> 0;

	return;
}

int main(int args, char* argv[])
{
	printf("RARE (GB/GBC) to TXT converter\n");
	if (args != 3)
	{
		printf("Usage: RARE2TXT <rom> <config>\n");
		return -1;
	}
	else
	{
		if ((rom = fopen(argv[1], "rb")) == NULL)
		{
			printf("ERROR: Unable to open ROM file %s!\n", argv[1]);
			exit(1);
		}
		else
		{
			if ((cfg = fopen(argv[2], "r")) == NULL)
			{
				printf("ERROR: Unable to open configuration file %s!\n", argv[1]);
				exit(1);
			}
			else
			{
				/*Get the "master bank" value*/
				fgets(string1, 12, cfg);

				if (memcmp(string1, checkStrings[0], 1))
				{
					printf("ERROR: Invalid CFG data!\n");
					exit(1);
						
				}
				fgets(string1, 3, cfg);
				masterBank = strtol(string1, NULL, 16);
				printf("Master bank: %01X\n", masterBank);

				/*Skip new line*/
				fgets(string1, 2, cfg);
				/*Get the total number of songs*/
				fgets(string1, 10, cfg);

				if (memcmp(string1, checkStrings[1], 1))
				{
					printf("ERROR: Invalid CFG data!\n");
					exit(1);

				}
				fgets(string1, 3, cfg);
				numSongs = strtod(string1, NULL);
				printf("Total # of songs: %i\n", numSongs);

				/*Skip new line*/
				fgets(string1, 2, cfg);
				/*Get the format number*/
				fgets(string1, 8, cfg);

				if (memcmp(string1, checkStrings[5], 1))
				{
					printf("ERROR: Invalid CFG data!\n");
					exit(1);

				}
				fgets(string1, 3, cfg);
				format = strtod(string1, NULL);
				printf("Format: %i\n", format);

				/*Copy the current bank's ROM data*/
				if (masterBank != 1)
				{
					bankAmt = bankSize;
					fseek(rom, ((masterBank - 1) * bankSize), SEEK_SET);
					romData = (unsigned char*)malloc(bankSize);
					fread(romData, 1, bankSize, rom);
				}

				else
				{
					bankAmt = 0;
					fseek(rom, ((masterBank - 1) * bankSize * 2), SEEK_SET);
					romData = (unsigned char*)malloc(bankSize * 2);
					fread(romData, 1, bankSize * 2, rom);
				}

				/*Try to search the bank for base table*/
				for (i = 0; i < bankSize; i++)
				{
					if ((!memcmp(&romData[i], MagicBytes, 7)) && foundTable != 1)
					{
						tablePtrLoc = bankAmt + i + 7;
						printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
						tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
						printf("Song table starts at 0x%04x...\n", tableOffset);
						break;
					}
				}

				/*Now search for the tempo table*/
				for (i = 0; i < bankSize; i++)
				{
					if (!memcmp(&romData[i], MagicBytesTempo, 8))
					{
						tempoTablePtrLoc = bankAmt + i + 8;
						printf("Found pointer to tempo table at address 0x%04x!\n", tempoTablePtrLoc);
						tempoTableOffset = ReadLE16(&romData[tempoTablePtrLoc - bankAmt]);
						printf("Tempo table starts at 0x%04x...\n", tempoTableOffset);
						break;
					}
				}

				/*Another method*/
				for (i = 0; i < bankSize; i++)
				{
					if (!memcmp(&romData[i], MagicBytesTempo2, 7))
					{
						tempoTablePtrLoc = bankAmt + i + 7;
						printf("Found pointer to tempo table at address 0x%04x!\n", tempoTablePtrLoc);
						tempoTableOffset = ReadLE16(&romData[tempoTablePtrLoc - bankAmt]);
						printf("Tempo table starts at 0x%04x...\n", tempoTableOffset);
						break;
					}
				}

				/*Another method*/
				for (i = 0; i < bankSize; i++)
				{
					if (!memcmp(&romData[i], MagicBytesTempo3, 8))
					{
						tempoTablePtrLoc = bankAmt + i + 8;
						printf("Found pointer to tempo table at address 0x%04x!\n", tempoTablePtrLoc);
						tempoTableOffset = ReadLE16(&romData[tempoTablePtrLoc - bankAmt]);
						printf("Tempo table starts at 0x%04x...\n", tempoTableOffset);
						break;
					}
				}

				/*Another method*/
				for (i = 0; i < bankSize; i++)
				{
					if (!memcmp(&romData[i], MagicBytesTempo4, 6))
					{
						tempoTablePtrLoc = bankAmt + i + 6;
						printf("Found pointer to tempo table at address 0x%04x!\n", tempoTablePtrLoc);
						tempoTableOffset = ReadLE16(&romData[tempoTablePtrLoc - bankAmt]);
						printf("Tempo table starts at 0x%04x...\n", tempoTableOffset);
						break;
					}
				}

				/*Another method*/
				for (i = 0; i < bankSize; i++)
				{
					if (!memcmp(&romData[i], MagicBytesTempo5, 6))
					{
						tempoTablePtrLoc = bankAmt + i + 6;
						printf("Found pointer to tempo table at address 0x%04x!\n", tempoTablePtrLoc);
						tempoTableOffset = ReadLE16(&romData[tempoTablePtrLoc - bankAmt]);
						printf("Tempo table starts at 0x%04x...\n", tempoTableOffset);
						break;
					}
				}

				i = tableOffset - bankAmt;
				songNum = 1;
				tempoPos = tempoTableOffset - bankAmt;

				while (fileExit == 0 && exitError == 0)
				{
					bankOverride = 0;
					ptrOverride = 0;

					if (songNum > numSongs)
					{
						fileExit = 1;
					}
					if (fileExit == 0)
					{
						/*Skip new line*/
						fgets(string1, 2, cfg);
						/*Skip the first line*/
						fgets(string1, 10, cfg);

						/*Now look for the "bank"*/
						fgets(string1, 6, cfg);
						if (memcmp(string1, checkStrings[2], 1))
						{
							exitError = 1;
						}
						/*Check if the parameter is "auto" or not*/
						fgets(string1, 5, cfg);
						if (memcmp(string1, checkStrings[4], 1))
						{
							bankOverride = 1;
							bank = strtol(string1, NULL, 16);
						}
						/*Skip new line*/
						fgets(string1, 2, cfg);

						/*Now look for the "base"*/
						fgets(string1, 6, cfg);
						if (memcmp(string1, checkStrings[3], 1))
						{
							exitError = 1;
						}
						/*Check if the parameter is "auto" or not*/
						fgets(string1, 5, cfg);
						if (memcmp(string1, checkStrings[4], 1))
						{
							ptrOverride = 1;
							base = strtol(string1, NULL, 16);
						}

						if (bankOverride == 0)
						{
							bank = masterBank;
						}
						printf("Song %i bank: %01X\n", songNum, bank);

						fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
						exRomData = (unsigned char*)malloc(bankSize);
						fread(exRomData, 1, bankSize, rom);

						if (ptrOverride == 0)
						{
							base = 0x4000;
							subBase = 0x4000;
						}
						else if (ptrOverride == 1)
						{
							base = strtol(string1, NULL, 16);
							fgets(string1, 2, cfg);
							fgets(string1, 5, cfg);
							subBase = strtol(string1, NULL, 16);
						}
						printf("Song %i base: %01X\n", songNum, base);

						/*Now get the pointers*/
						for (curTrack = 0; curTrack < 4; curTrack++)
						{
							songPtrs[curTrack] = ReadLE16(&romData[i]);
							if (ptrOverride == 1)
							{
								songPtrs[curTrack] = songPtrs[curTrack] - base + subBase;
							}
							printf("Song %i channel %i: %04X\n", songNum, (curTrack + 1), songPtrs[curTrack]);
							i += 2;
						}

						tempo = romData[tempoPos];
						printf("Song %i tempo: %01X\n", songNum, tempo);
						tempoPos++;
						
						if (songPtrs[0] != 0x0000 && songPtrs[0] < 0x8000 && songPtrs[0] > bankAmt)
						{
							song2txt(songNum, songPtrs);
						}

						songNum++;
					}
					
				}

				if (exitError == 1)
				{
					printf("ERROR: Invalid CFG data!\n");
					exit(1);
				}

				songNum = 1;
				printf("The operation was successfully completed!\n");
				exit(0);
			}
		}
	}
}

/*Convert the song data to TXT*/
void song2txt(int songNum, long ptrList[4])
{
	long command[5];
	long romPos = 0;
	long seqPos = 0;
	int endSeq = 0;
	int curTempo = tempo;
	int transpose1 = 0;
	int transpose2 = 0;
	int curNote = 0;
	int curNoteLen = 0;
	long jumpPos = 0;
	long macro1aPos = 0;
	long macro1aRet = 0;
	long macro1bPos = 0;
	long macro1bRet = 0;
	int macro1aTimes = 0;
	int macro1bTimes = 0;
	long macro2aPos = 0;
	long macro2aRet = 0;
	long macro2bPos = 0;
	long macro2bRet = 0;
	int macro2aTimes = 0;
	int macro2bTimes = 0;
	long macro3aPos = 0;
	long macro3aRet = 0;
	long macro3bPos = 0;
	long macro3bRet = 0;
	int macro3aTimes = 0;
	int macro3bTimes = 0;
	int curVol = 0;
	int automaticCtrl = 0;
	int automaticSpeed = 0;
	int octaveFlag = 0;
	long repeatPt = 0;
	int repeatTimes = 0;
	int repNoteVal = 0;
	int repNoteTimes = 0;

	sprintf(outfile, "song%d.txt", songNum);
	if ((txt = fopen(outfile, "wb")) == NULL)
	{
		printf("ERROR: Unable to write to file song%d.txt!\n", songNum);
		exit(2);
	}
	else
	{
		for (curTrack = 0; curTrack < 4; curTrack++)
		{
			if (bank > 1)
			{
				bankAmt = 0x4000;
			}
			seqPos = ptrList[curTrack] - bankAmt;
			fprintf(txt, "Channel %i:\n", (curTrack + 1));

			endSeq = 0;
			automaticCtrl = 0;


			/*DKL1*/
			if (format == 1)
			{
				while (endSeq == 0)
				{
					command[0] = exRomData[seqPos];
					command[1] = exRomData[seqPos + 1];
					command[2] = exRomData[seqPos + 2];
					command[3] = exRomData[seqPos + 3];
					command[4] = exRomData[seqPos + 4];

					if (command[0] == 0x00)
					{
						fprintf(txt, "End of sequence\n\n");
						endSeq = 1;
					}

					else if (command[0] == 0x01)
					{
						jumpPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						fprintf(txt, "Jump to position: 0x%04X\n", jumpPos);
						seqPos += 3;
					}

					else if (command[0] == 0x02)
					{
						fprintf(txt, "Go to loop point: %01X\n\n", command[1]);
						endSeq = 1;
					}

					else if (command[0] == 0x03)
					{
						fprintf(txt, "Set channel 3 envelope: %01X, %01X, %01X\n", command[1], command[2], command[3]);
						seqPos += 4;
					}

					else if (command[0] == 0x04)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase;
						macro1aRet = seqPos + 4;
						macro1aTimes = exRomData[seqPos + 1];
						fprintf(txt, "Go to macro 1 (manual times): 0x%04X, %i times\n", macro1aPos, macro1aTimes);
						seqPos += 4;
					}

					else if (command[0] == 0x05)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 1;
						fprintf(txt, "Go to macro 1 %i time: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x06)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 2;
						fprintf(txt, "Go to macro 1 %i times: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x07)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 3;
						fprintf(txt, "Go to macro 1 %i times: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x08)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 4;
						fprintf(txt, "Go to macro 1 %i times: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x09)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 6;
						fprintf(txt, "Go to macro 1 %i times: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x0A)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 8;
						fprintf(txt, "Go to macro 1 %i times: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x0B)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 10;
						fprintf(txt, "Go to macro 1 %i times: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x0C)
					{
						fprintf(txt, "End of macro 1\n");
						seqPos++;
					}

					else if (command[0] == 0x0D)
					{
						automaticCtrl = 1;
						automaticSpeed = command[1];;
						fprintf(txt, "Turn on automatic note length mode (manual) at speed %i\n", automaticSpeed);
						seqPos += 2;
					}

					else if (command[0] == 0x0E)
					{
						automaticCtrl = 1;
						automaticSpeed = 1;
						fprintf(txt, "Turn on automatic note length mode at speed %i\n", automaticSpeed);
						seqPos++;
					}


					else if (command[0] == 0x0F)
					{
						automaticCtrl = 1;
						automaticSpeed = 2;
						fprintf(txt, "Turn on automatic note length mode at speed %i\n", automaticSpeed);
						seqPos++;
					}


					else if (command[0] == 0x10)
					{
						automaticCtrl = 1;
						automaticSpeed = 4;
						fprintf(txt, "Turn on automatic note length mode at speed %i\n", automaticSpeed);
						seqPos++;
					}


					else if (command[0] == 0x11)
					{
						automaticCtrl = 1;
						automaticSpeed = 6;
						fprintf(txt, "Turn on automatic note length mode at speed %i\n", automaticSpeed);
						seqPos++;
					}


					else if (command[0] == 0x12)
					{
						automaticCtrl = 1;
						automaticSpeed = 8;
						fprintf(txt, "Turn on automatic note length mode at speed %i\n", automaticSpeed);
						seqPos++;
					}

					else if (command[0] == 0x13)
					{
						fprintf(txt, "Turn off automatic note length mode\n");
						automaticCtrl = 0;
						seqPos++;
					}

					else if (command[0] == 0x14)
					{
						fprintf(txt, "Turn on vibrato (v2): %01X, %01X, %01X\n", command[1], command[2], command[3]);
						seqPos += 4;
					}

					else if (command[0] == 0x15)
					{
						fprintf(txt, "Turn off vibrato\n");
						seqPos++;
					}

					else if (command[0] == 0x16)
					{
						octaveFlag = 0;
						fprintf(txt, "Turn off transpose\n");
						seqPos++;
					}

					else if (command[0] == 0x17)
					{
						transpose1 = 12;
						fprintf(txt, "Set transpose to %i\n", transpose1);
						seqPos++;
					}

					else if (command[0] == 0x18)
					{
						transpose1 = (signed char)command[1];
						fprintf(txt, "Set transpose (v1): %i\n", transpose1);
						seqPos += 2;
					}

					else if (command[0] == 0x19)
					{
						transpose2 = (signed char)command[1];
						fprintf(txt, "Set transpose (v2): %i\n", transpose2);
						seqPos += 2;
					}

					else if (command[0] == 0x1A)
					{
						transpose1 = 2;
						fprintf(txt, "Set transpose to %i\n", transpose1);
						seqPos++;
					}

					else if (command[0] == 0x1B)
					{
						transpose1 = -2;
						fprintf(txt, "Set transpose to %i\n", transpose1);
						seqPos++;
					}

					else if (command[0] == 0x1C)
					{
						transpose1 = 5;
						fprintf(txt, "Set transpose to %i\n", transpose1);
						seqPos++;
					}

					else if (command[0] == 0x1D)
					{
						transpose1 = -5;
						fprintf(txt, "Set transpose to %i\n", transpose1);
						seqPos++;
					}

					else if (command[0] == 0x1E)
					{
						fprintf(txt, "Turn on vibrato: %01X, %01X, %01X, %01X\n", command[1], command[2], command[3], command[4]);
						seqPos += 5;
					}

					else if (command[0] == 0x1F)
					{
						curVol = 0x10;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x20)
					{
						curVol = 0x20;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x21)
					{
						curVol = 0x30;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x22)
					{
						curVol = 0x40;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x23)
					{
						curVol = 0x50;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x24)
					{
						curVol = 0x60;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x25)
					{
						curVol = 0x70;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x26)
					{
						fprintf(txt, "Switch to duty 1, envelope: %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x27)
					{
						fprintf(txt, "Switch to duty 2, envelope: %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x28)
					{
						fprintf(txt, "Switch to duty 3, envelope: %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x29)
					{
						fprintf(txt, "Set channel envelope (v2): %01X, %01X\n", command[1], command[2]);
						seqPos += 3;
					}

					else if (command[0] == 0x2A)
					{
						macro2aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro2aRet = seqPos + 3;
						fprintf(txt, "Go to macro 2: 0x%04X\n", macro2aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x2B)
					{
						fprintf(txt, "End of macro 2 and turn off automatic note length mode\n");
						seqPos++;
					}

					else if (command[0] == 0x2C)
					{
						fprintf(txt, "Switch waveform: %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x2D)
					{
						fprintf(txt, "Switch to duty 4, envelope: %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x2E)
					{
						command[1] = 0x23;
						fprintf(txt, "Change note size to %01X\n", command[1]);
						seqPos++;
					}

					else if (command[0] == 0x2F)
					{
						command[1] = 0x51;
						fprintf(txt, "Change note size to %01X\n", command[1]);
						seqPos++;
					}

					else if (command[0] == 0x30)
					{
						command[1] = 0x71;
						fprintf(txt, "Change note size to %01X\n", command[1]);
						seqPos++;
					}

					else if (command[0] == 0x31)
					{
						command[1] = 0x61;
						fprintf(txt, "Change note size to %01X\n", command[1]);
						seqPos++;
					}

					else if (command[0] == 0x32)
					{
						command[1] = 0x37;
						fprintf(txt, "Change note size to %01X\n", command[1]);
						seqPos++;
					}

					else if (command[0] == 0x33)
					{
						command[1] = 0x38;
						fprintf(txt, "Change note size to %01X\n", command[1]);
						seqPos++;
					}

					else if (command[0] == 0x34)
					{
						command[1] = 0x53;
						fprintf(txt, "Change note size to %01X\n", command[1]);
						seqPos++;
					}

					else if (command[0] == 0x35)
					{
						command[1] = 0x67;
						fprintf(txt, "Change note size to %01X\n", command[1]);
						seqPos++;
					}

					else if (command[0] == 0x36)
					{
						command[1] = 0xA1;
						fprintf(txt, "Change note size to %01X\n", command[1]);
						seqPos++;
					}

					else if (command[0] == 0x37)
					{
						command[1] = 0x58;
						fprintf(txt, "Change note size to %01X\n", command[1]);
						seqPos++;
					}

					else if (command[0] == 0x38)
					{
						command[1] = 0x68;
						fprintf(txt, "Change note size to %01X\n", command[1]);
						seqPos++;
					}

					else if (command[0] == 0x39)
					{
						command[1] = 0x32;
						fprintf(txt, "Change note size to %01X\n", command[1]);
						seqPos++;
					}

					else if (command[0] == 0x3A)
					{
						command[1] = 0x33;
						fprintf(txt, "Change note size to %01X\n", command[1]);
						seqPos++;
					}

					else if (command[0] == 0x3B)
					{
						command[1] = 0x22;
						fprintf(txt, "Change note size to %01X\n", command[1]);
						seqPos++;
					}

					else if (command[0] == 0x3C)
					{
						fprintf(txt, "Change note size: %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x3D)
					{
						fprintf(txt, "End of macro 2\n");
						seqPos++;
					}

					else if (command[0] == 0x3E)
					{
						repeatPt = seqPos + 2;
						repeatTimes = command[1];
						fprintf(txt, "Repeat section (manual): %i times\n", repeatTimes);
						seqPos += 2;
					}

					else if (command[0] == 0x3F)
					{
						repeatPt = seqPos + 1;
						repeatTimes = 2;
						fprintf(txt, "Repeat section %i times\n", repeatTimes);
						seqPos++;
					}

					else if (command[0] == 0x40)
					{
						repeatPt = seqPos + 1;
						repeatTimes = 3;
						fprintf(txt, "Repeat section %i times\n", repeatTimes);
						seqPos++;
					}

					else if (command[0] == 0x41)
					{
						repeatPt = seqPos + 1;
						repeatTimes = 4;
						fprintf(txt, "Repeat section %i times\n", repeatTimes);
						seqPos++;
					}

					else if (command[0] == 0x42)
					{
						fprintf(txt, "End of repeat section\n");
						seqPos++;
					}

					else if (command[0] == 0x43)
					{
						transpose1 = 0;
						fprintf(txt, "End of macro 1 and turn off transpose\n");
						seqPos++;
					}

					else if (command[0] == 0x44)
					{
						macro3aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro3aRet = seqPos + 3;
						fprintf(txt, "Go to macro 3: 0x%04X\n", macro3aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x45)
					{
						fprintf(txt, "End of macro 3\n");
						seqPos++;
					}

					else if (command[0] == 0x46)
					{
						fprintf(txt, "End of macro 3 and turn off automatic note length mode\n");
						seqPos++;
					}

					else if (command[0] == 0x47)
					{
						fprintf(txt, "End of macro 3 and turn on automatic note length mode\n");
						seqPos++;
					}

					else if (command[0] == 0x48)
					{
						fprintf(txt, "Change note size (v2): %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] > 0x48 && command[0] < 0x80)
					{
						curNote = command[0];
						fprintf(txt, "Play note (1): %01X\n", command[0]);
						seqPos++;
					}

					else if (command[0] == 0x80)
					{
						if (automaticCtrl == 0)
						{
							curNoteLen = command[1];
							fprintf(txt, "Rest: %01X length\n", curNoteLen);
							seqPos += 2;
						}
						else if (automaticCtrl == 1)
						{
							fprintf(txt, "Rest\n");
							seqPos++;
						}

					}

					else if (command[0] > 0x80 && command[0] < 0xC0)
					{
						if (automaticCtrl == 0)
						{
							curNote = command[0];
							curNoteLen = command[1];
							fprintf(txt, "Play note: %01X, length: %01X\n", curNote, curNoteLen);
							seqPos += 2;
						}
						else if (automaticCtrl == 1)
						{
							curNote = command[0];
							fprintf(txt, "Play note: %01X\n", curNote);
							seqPos++;
						}

					}

					else if (command[0] >= 0xC0 && command[0] < 0xD0)
					{
						if (automaticCtrl == 0)
						{
							curNote = command[0];
							curNoteLen = command[1];
							fprintf(txt, "Play drum note: %01X, length: %01X\n", curNote, curNoteLen);
							seqPos += 2;
						}
						else if (automaticCtrl == 1)
						{
							curNote = command[0];
							fprintf(txt, "Play drum note: %01X\n", curNote);
							seqPos++;
						}
					}

					else if (command[0] >= 0xD0)
					{
						curNote = command[0];
						fprintf(txt, "Play note (3): %01X\n", command[0]);
						seqPos++;
					}

					else
					{
						fprintf(txt, "Unknown command: %01X\n", command[0]);
						seqPos++;
					}
				}

			}

			/*DKL2/3*/
			else if (format == 2)
			{
				while (endSeq == 0)
				{
					command[0] = exRomData[seqPos];
					command[1] = exRomData[seqPos + 1];
					command[2] = exRomData[seqPos + 2];
					command[3] = exRomData[seqPos + 3];
					command[4] = exRomData[seqPos + 4];

					if (command[0] == 0x00)
					{
						fprintf(txt, "End of sequence\n\n");
						endSeq = 1;
					}

					else if (command[0] == 0x01)
					{
						jumpPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						fprintf(txt, "Jump to position: 0x%04X\n", jumpPos);
						seqPos += 3;
					}

					else if (command[0] == 0x02)
					{
						fprintf(txt, "Go to loop point: %01X\n\n", command[1]);
						endSeq = 1;
					}

					else if (command[0] == 0x03)
					{
						fprintf(txt, "Set channel 3 envelope: %01X, %01X, %01X\n", command[1], command[2], command[3]);
						seqPos += 4;
					}

					else if (command[0] == 0x04)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase;
						macro1aRet = seqPos + 4;
						macro1aTimes = exRomData[seqPos + 1];
						fprintf(txt, "Go to macro (manual times): 0x%04X, %i times\n", macro1aPos, macro1aTimes);
						seqPos += 4;
					}

					else if (command[0] == 0x05)
					{
						fprintf(txt, "End of macro\n");
						seqPos++;
					}

					else if (command[0] == 0x06)
					{
						fprintf(txt, "Set panning 1\n");
						seqPos++;
					}

					else if (command[0] == 0x07)
					{
						fprintf(txt, "Set panning 2\n");
						seqPos++;
					}

					else if (command[0] == 0x08)
					{
						fprintf(txt, "Set panning 3\n");
						seqPos++;
					}

					else if (command[0] == 0x0A)
					{
						automaticCtrl = 1;
						automaticSpeed = command[1];;
						fprintf(txt, "Turn on automatic note length mode (manual) at speed %i\n", automaticSpeed);
						seqPos += 2;
					}

					else if (command[0] == 0x0B)
					{
						fprintf(txt, "Turn off automatic note length mode\n");
						automaticCtrl = 0;
						seqPos++;
					}

					else if (command[0] == 0x0C)
					{
						fprintf(txt, "Turn on vibrato (v2): %01X, %01X, %01X\n", command[1], command[2], command[3]);
						seqPos += 5;
					}

					else if (command[0] == 0x0D)
					{
						fprintf(txt, "Turn off vibrato\n");
						seqPos++;
					}

					else if (command[0] == 0x0E || command[0] == 0x0F)
					{
						fprintf(txt, "Portamento: %01X, %01X, %01X, %01X\n", command[1], command[2], command[3], command[4]);
						seqPos += 5;
					}

					else if (command[0] == 0x10)
					{
						fprintf(txt, "Pitch bend effect: %01X, %01X, %01X\n", command[1], command[2], command[3]);
						seqPos += 4;
					}

					else if (command[0] == 0x12)
					{
						fprintf(txt, "Change tempo: %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x13)
					{
						transpose2 = (signed char)command[1];
						fprintf(txt, "Set transpose (absolute value): %i\n", transpose2);
						seqPos += 2;
					}

					else if (command[0] == 0x14)
					{
						transpose1 = (signed char)command[1];
						fprintf(txt, "Set transpose (add to current value): %i\n", transpose1);
						seqPos += 2;
					}

					else if (command[0] == 0x16)
					{
						fprintf(txt, "Turn on vibrato: %01X, %01X, %01X, %01X\n", command[1], command[2], command[3], command[4]);
						seqPos += 5;
					}

					else if (command[0] == 0x17)
					{
						curVol = 0x10;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x18)
					{
						curVol = 0x20;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x19)
					{
						curVol = 0x30;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x1A)
					{
						curVol = 0x40;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x1B)
					{
						curVol = 0x50;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x1C)
					{
						curVol = 0x60;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x1D)
					{
						curVol = 0x70;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x1E)
					{
						curVol = 0x80;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x1F)
					{
						curVol = 0x90;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x20)
					{
						curVol = 0xA0;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x21)
					{
						curVol = 0xB0;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x22)
					{
						curVol = 0xC0;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x23)
					{
						curVol = 0xD0;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x24)
					{
						curVol = 0xE0;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x25)
					{
						curVol = 0xF0;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x26)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 1;
						fprintf(txt, "Go to macro %i time: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x27)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 2;
						fprintf(txt, "Go to macro %i times: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x28)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 3;
						fprintf(txt, "Go to macro %i times: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x29)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 4;
						fprintf(txt, "Go to macro %i times: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x2A)
					{
						fprintf(txt, "Set channel envelope (v2): %01X, %01X\n", command[1], command[2]);
						seqPos += 3;
					}

					else if (command[0] == 0x2B)
					{
						automaticCtrl = 1;
						automaticSpeed = 2;
						fprintf(txt, "Turn on automatic note length mode at speed %i\n", automaticSpeed);
						seqPos++;
					}

					else if (command[0] == 0x2C)
					{
						automaticCtrl = 1;
						automaticSpeed = 1;
						fprintf(txt, "Turn on automatic note length mode at speed %i\n", automaticSpeed);
						seqPos++;
					}

					else if (command[0] == 0x2D)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 8;
						fprintf(txt, "Go to macro %i times: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x2E)
					{
						automaticCtrl = 1;
						automaticSpeed = 8;
						fprintf(txt, "Turn on automatic note length mode at speed %i\n", automaticSpeed);
						seqPos++;
					}

					else if (command[0] == 0x2F)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 10;
						fprintf(txt, "Go to macro %i times: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x30)
					{
						fprintf(txt, "Invalid command 30\n");
						seqPos++;
					}

					else if (command[0] == 0x31)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 5;
						fprintf(txt, "Go to macro %i times: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x33)
					{
						fprintf(txt, "Switch to duty 1, envelope: %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x34)
					{
						fprintf(txt, "Switch to duty 2, envelope: %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x35)
					{
						fprintf(txt, "Switch to duty 3, envelope: %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x36)
					{
						fprintf(txt, "Switch to duty 4, envelope: %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x37)
					{
						fprintf(txt, "Turn off echo/envelope effect\n");
						seqPos++;
					}

					else if (command[0] == 0x38)
					{
						automaticCtrl = 1;
						automaticSpeed = 6;
						fprintf(txt, "Turn on automatic note length mode at speed %i\n", automaticSpeed);
						seqPos++;
					}

					else if (command[0] == 0x3B)
					{
						fprintf(txt, "Switch waveform: %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x3F)
					{
						fprintf(txt, "Change note size: %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x40)
					{
						fprintf(txt, "Change note size (v2?): %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x41)
					{
						automaticCtrl = 1;
						automaticSpeed = 4;
						fprintf(txt, "Turn on automatic note length mode at speed %i\n", automaticSpeed);
						seqPos++;
					}

					else if (command[0] == 0x4F)
					{
						octaveFlag = 0;
						fprintf(txt, "Turn off transpose\n");
						seqPos++;
					}

					else if (command[0] == 0x50)
					{
						transpose1 = (signed char)command[1];
						fprintf(txt, "Set transpose 1 by 1 octave\n");
						seqPos++;
					}

					else if (command[0] == 0x80)
					{
						if (automaticCtrl == 0)
						{
							curNoteLen = command[1];
							fprintf(txt, "Rest: %01X length\n", curNoteLen);
							seqPos += 2;
						}
						else if (automaticCtrl == 1)
						{
							fprintf(txt, "Rest\n");
							seqPos++;
						}

					}

					else if (command[0] > 0x80)
					{
						if (automaticCtrl == 0)
						{
							curNote = command[0];
							curNoteLen = command[1];
							fprintf(txt, "Play note: %01X, length: %01X\n", curNote, curNoteLen);
							seqPos += 2;
						}
						else if (automaticCtrl == 1)
						{
							curNote = command[0];
							fprintf(txt, "Play note: %01X\n", curNote);
							seqPos++;
						}

					}

					else
					{
						fprintf(txt, "Unknown command: %01X\n", command[0]);
						seqPos++;
					}
				}
			}

			/*Monster Max*/
			else if (format == 3)
			{
				while (endSeq == 0)
				{
					command[0] = exRomData[seqPos];
					command[1] = exRomData[seqPos + 1];
					command[2] = exRomData[seqPos + 2];
					command[3] = exRomData[seqPos + 3];
					command[4] = exRomData[seqPos + 4];

					if (command[0] == 0x00)
					{
						fprintf(txt, "End of sequence\n\n");
						endSeq = 1;
					}

					else if (command[0] == 0x01)
					{
						jumpPos = ReadBE16(&exRomData[seqPos + 1]) - base + subBase;
						fprintf(txt, "Jump to position: 0x%04X\n\n", jumpPos);
						endSeq = 1;
					}

					else if (command[0] == 0x02)
					{
						fprintf(txt, "Switch waveform: %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x03)
					{
						fprintf(txt, "Set channel envelope: %01X, %01X, %01X\n", command[1], command[2], command[3]);
						seqPos += 4;
					}

					else if (command[0] == 0x04)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase;
						macro1aRet = seqPos + 4;
						macro1aTimes = exRomData[seqPos + 1];
						fprintf(txt, "Go to macro (manual times): 0x%04X, %i times\n", macro1aPos, macro1aTimes);
						seqPos += 4;
					}

					else if (command[0] == 0x05)
					{
						fprintf(txt, "End of macro\n");
						seqPos++;
					}

					else if (command[0] == 0x06)
					{
						fprintf(txt, "Set panning 1\n");
						seqPos++;
					}

					else if (command[0] == 0x07)
					{
						fprintf(txt, "Set panning 2\n");
						seqPos++;
					}

					else if (command[0] == 0x08)
					{
						fprintf(txt, "Set panning 3\n");
						seqPos++;
					}

					else if (command[0] == 0x09)
					{
						fprintf(txt, "Set distortion effect?: %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x0A)
					{
						automaticCtrl = 1;
						automaticSpeed = command[1];;
						fprintf(txt, "Turn on automatic note length mode (manual) at speed %i\n", automaticSpeed);
						seqPos += 2;
					}

					else if (command[0] == 0x0B)
					{
						fprintf(txt, "Turn off automatic note length mode\n");
						automaticCtrl = 0;
						seqPos++;
					}

					else if (command[0] == 0x0C)
					{
						fprintf(txt, "Volume effect: %01X, %01X, %01X\n", command[1], command[2], command[3]);
						seqPos += 4;
					}

					else if (command[0] == 0x0D)
					{
						fprintf(txt, "Reset noise?\n");
						seqPos++;
					}

					else if (command[0] == 0x0E)
					{
						fprintf(txt, "Portamento: %01X, %01X, %01X, %01X, %01X\n", command[1], command[2], command[3], command[4], exRomData[seqPos + 5]);
						seqPos += 6;
					}

					else if (command[0] == 0x10)
					{
						fprintf(txt, "Manually set envelope: %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x12)
					{
						fprintf(txt, "Change tempo: %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x13)
					{
						transpose1 = (signed char)command[1];
						fprintf(txt, "Set transpose: %i\n", transpose1);
						seqPos += 2;
					}

					else if (command[0] == 0x14)
					{
						transpose1 = (signed char)command[1];
						fprintf(txt, "Add to transpose: %i\n", transpose1);
						seqPos += 2;
					}

					else if (command[0] == 0x15)
					{
						transpose2 = (signed char)command[1];
						fprintf(txt, "Set distortion pitch effect?: %i\n", transpose2);
						seqPos += 2;
					}

					else if (command[0] == 0x16)
					{
						fprintf(txt, "Turn on vibrato: %01X, %01X, %01X, %01X\n", command[1], command[2], command[3], command[4]);
						seqPos += 5;
					}

					else if (command[0] == 0x17)
					{
						fprintf(txt, "Turn off vibrato?\n");
						seqPos++;
					}

					else if (command[0] == 0x18)
					{
						fprintf(txt, "Change volume to 60\n");
						seqPos++;
					}

					else if (command[0] == 0x19)
					{
						fprintf(txt, "Change volume to 30\n");
						seqPos++;
					}

					else if (command[0] == 0x1A)
					{
						fprintf(txt, "Change volume to 80\n");
						seqPos++;
					}

					else if (command[0] == 0x1B)
					{
						fprintf(txt, "Change volume to 70\n");
						seqPos++;
					}

					else if (command[0] == 0x1C)
					{
						fprintf(txt, "Change volume to 20\n");
						seqPos++;
					}

					else if (command[0] == 0x1D)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 1;
						fprintf(txt, "Go to macro %i time: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x1E)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 2;
						fprintf(txt, "Go to macro %i times: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x1F)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 3;
						fprintf(txt, "Go to macro %i times: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x20)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 4;
						fprintf(txt, "Go to macro %i times: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x21)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 8;
						fprintf(txt, "Go to macro %i times: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x22)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 6;
						fprintf(txt, "Go to macro %i times: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x23)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 7;
						fprintf(txt, "Go to macro %i times: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x24)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 5;
						fprintf(txt, "Go to macro %i times: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x25)
					{
						fprintf(txt, "Set channel envelope and duty (0): %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x26)
					{
						fprintf(txt, "Set channel envelope and duty (40): %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x27)
					{
						fprintf(txt, "Set note size: %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x28)
					{
						fprintf(txt, "Set channel envelope and duty (80): %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x29)
					{
						fprintf(txt, "Set duty to 0\n");
						seqPos++;
					}

					else if (command[0] == 0x2A)
					{
						automaticCtrl = 0;
						fprintf(txt, "Turn off automatic note length mode (v2)?\n");
						seqPos++;
					}

					else if (command[0] == 0x2B)
					{
						fprintf(txt, "Set duty effect?: %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x2C)
					{
						fprintf(txt, "Turn off vibrato (v2?)");
						seqPos++;
					}

					else if (command[0] == 0x2E)
					{
						fprintf(txt, "Change volume to 90\n");
						seqPos++;
					}

					else if (command[0] == 0x80)
					{
						if (automaticCtrl == 0)
						{
							curNoteLen = command[1];
							fprintf(txt, "Rest: %01X length\n", curNoteLen);
							seqPos += 2;
						}
						else if (automaticCtrl == 1)
						{
							fprintf(txt, "Rest\n");
							seqPos++;
						}

					}

					else if (command[0] > 0x80 && command[0] < 0xC0)
					{
						if (automaticCtrl == 0)
						{
							curNote = command[0];
							curNoteLen = command[1];
							fprintf(txt, "Play note: %01X, length: %01X\n", curNote, curNoteLen);
							seqPos += 2;
						}
						else if (automaticCtrl == 1)
						{
							curNote = command[0];
							fprintf(txt, "Play note: %01X\n", curNote);
							seqPos++;
						}

					}

					else if (command[0] >= 0xC0 && command[0] < 0xD0)
					{
						if (automaticCtrl == 0)
						{
							curNote = command[0];
							curNoteLen = command[1];
							fprintf(txt, "Play drum note: %01X, length: %01X\n", curNote, curNoteLen);
							seqPos += 2;
						}
						else if (automaticCtrl == 1)
						{
							curNote = command[0];
							fprintf(txt, "Play drum note: %01X\n", curNote);
							seqPos++;
						}
					}

					else
					{
						fprintf(txt, "Unknown command: %01X\n", command[0]);
						seqPos++;
					}
				}

			}

			/*Battletoads 2/3*/
			else if (format == 4)
			{
				while (endSeq == 0)
				{
					command[0] = exRomData[seqPos];
					command[1] = exRomData[seqPos + 1];
					command[2] = exRomData[seqPos + 2];
					command[3] = exRomData[seqPos + 3];
					command[4] = exRomData[seqPos + 4];

					if (command[0] == 0x00)
					{
						fprintf(txt, "End of sequence\n\n");
						endSeq = 1;
					}

					else if (command[0] == 0x01)
					{
						jumpPos = ReadBE16(&exRomData[seqPos + 1]) - base + subBase;
						fprintf(txt, "Jump to position: 0x%04X\n", jumpPos);
						seqPos += 3;
					}

					else if (command[0] == 0x02)
					{
						fprintf(txt, "Switch waveform: %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x03)
					{
						fprintf(txt, "Set channel 3 envelope: %01X, %01X, %01X\n", command[1], command[2], command[3]);
						seqPos += 4;
					}

					else if (command[0] == 0x04)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase;
						macro1aRet = seqPos + 4;
						macro1aTimes = exRomData[seqPos + 1];
						fprintf(txt, "Go to macro (manual times): 0x%04X, %i times\n", macro1aPos, macro1aTimes);
						seqPos += 4;
					}

					else if (command[0] == 0x05)
					{
						fprintf(txt, "End of macro\n");
						seqPos++;
					}

					else if (command[0] == 0x06)
					{
						fprintf(txt, "Set panning 1\n");
						seqPos++;
					}

					else if (command[0] == 0x07)
					{
						fprintf(txt, "Set panning 2\n");
						seqPos++;
					}

					else if (command[0] == 0x08)
					{
						fprintf(txt, "Set panning 3\n");
						seqPos++;
					}

					else if (command[0] == 0x09)
					{
						fprintf(txt, "Set distortion effect?: %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x0A)
					{
						automaticCtrl = 1;
						automaticSpeed = command[1];;
						fprintf(txt, "Turn on automatic note length mode (manual) at speed %i\n", automaticSpeed);
						seqPos += 2;
					}

					else if (command[0] == 0x0B)
					{
						fprintf(txt, "Turn off automatic note length mode\n");
						automaticCtrl = 0;
						seqPos++;
					}

					else if (command[0] == 0x0C)
					{
						fprintf(txt, "Turn on decay effect?: %01X, %01X, %01X\n", command[1], command[2], command[3]);
						seqPos += 4;
					}

					else if (command[0] == 0x0D)
					{
						fprintf(txt, "Reset noise?\n");
						seqPos++;
					}

					else if (command[0] == 0x0E)
					{
						fprintf(txt, "Portamento up: %01X, %01X, %01X, %01X\n", command[1], command[2], command[3], command[4]);
						seqPos += 6;
					}

					else if (command[0] == 0x0F)
					{
						fprintf(txt, "Portamento down: %01X, %01X, %01X, %01X\n", command[1], command[2], command[3], command[4]);
						seqPos += 6;
					}

					else if (command[0] == 0x10)
					{
						fprintf(txt, "Set drum length?: %i\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x12)
					{
						fprintf(txt, "Change tempo: %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x13)
					{
						transpose2 = (signed char)command[1];
						fprintf(txt, "Set transpose 1: %i\n", transpose2);
						seqPos += 2;
					}

					else if (command[0] == 0x14)
					{
						transpose1 = (signed char)command[1];
						fprintf(txt, "Set transpose 2: %i\n", transpose1);
						seqPos += 2;
					}

					else if (command[0] == 0x15)
					{
						transpose2 = (signed char)command[1];
						fprintf(txt, "Set distortion pitch effect?: %i\n", transpose2);
						seqPos += 2;
					}

					else if (command[0] == 0x16)
					{
						fprintf(txt, "Turn on vibrato: %01X, %01X, %01X, %01X\n", command[1], command[2], command[3], command[4]);
						seqPos += 5;
					}

					else if (command[0] == 0x17)
					{
						curVol = 0x10;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x18)
					{
						curVol = 0x20;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x19)
					{
						curVol = 0x30;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x1A)
					{
						curVol = 0x40;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x1B)
					{
						curVol = 0x50;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x1C)
					{
						curVol = 0x60;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x1D)
					{
						curVol = 0x70;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x1E)
					{
						curVol = 0x80;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x1F)
					{
						curVol = 0x90;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x20)
					{
						octaveFlag = 0;
						fprintf(txt, "Turn off transpose\n");
						seqPos++;
					}

					else if (command[0] == 0x21)
					{
						transpose1 = (signed char)command[1];
						fprintf(txt, "Set transpose 1 by 1 octave\n");
						seqPos++;
					}

					else if (command[0] == 0x22)
					{
						octaveFlag = 0;
						fprintf(txt, "Set note size to 40\n");
						seqPos++;
					}

					else if (command[0] == 0x23)
					{
						automaticCtrl = 1;
						automaticSpeed = 4;
						fprintf(txt, "Turn on automatic note length mode at speed %i\n", automaticSpeed);
						seqPos++;
					}

					else if (command[0] == 0x24)
					{
						curVol = 0xA0;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x25)
					{
						curVol = 0xB0;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x26)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 1;
						fprintf(txt, "Go to macro %i time: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x27)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 2;
						fprintf(txt, "Go to macro %i times: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x28)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 4;
						fprintf(txt, "Go to macro %i times: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x29)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 6;
						fprintf(txt, "Go to macro %i times: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x2A)
					{
						fprintf(txt, "Set note size to F0\n");
						seqPos++;
					}

					else if (command[0] == 0x2B)
					{
						automaticCtrl = 1;
						automaticSpeed = 2;
						fprintf(txt, "Turn on automatic note length mode at speed %i\n", automaticSpeed);
						seqPos++;
					}

					else if (command[0] == 0x2C)
					{
						automaticCtrl = 1;
						automaticSpeed = 1;
						fprintf(txt, "Turn on automatic note length mode at speed %i\n", automaticSpeed);
						seqPos++;
					}

					else if (command[0] == 0x2D)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 8;
						fprintf(txt, "Go to macro %i times: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x2E)
					{
						automaticCtrl = 1;
						automaticSpeed = 8;
						fprintf(txt, "Turn on automatic note length mode at speed %i\n", automaticSpeed);
						seqPos++;
					}

					else if (command[0] == 0x2F)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 10;
						fprintf(txt, "Go to macro %i times: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x30)
					{
						curVol = 0xC0;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x31)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 5;
						fprintf(txt, "Go to macro %i times: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x32)
					{
						automaticCtrl = 0;
						fprintf(txt, "Turn off automatic note length and end of macro\n");
						seqPos++;
					}

					else if (command[0] == 0x33)
					{
						fprintf(txt, "Set channel envelope/volume: %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x34)
					{
						fprintf(txt, "Change note size: %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x35)
					{
						fprintf(txt, "Set duty and change note size: %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x36)
					{
						fprintf(txt, "Change note size with echo effect?: %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x37)
					{
						fprintf(txt, "Turn off volume effect\n");
						seqPos++;
					}

					else if (command[0] == 0x38)
					{
						automaticCtrl = 1;
						automaticSpeed = 6;
						fprintf(txt, "Turn on automatic note length mode at speed %i\n", automaticSpeed);
						seqPos++;
					}

					else if (command[0] == 0x39)
					{
						fprintf(txt, "Turn off echo effect?\n");
						seqPos++;
					}

					else if (command[0] == 0x3A)
					{
						fprintf(txt, "Set stereo flag?\n");
						seqPos++;
					}

					else if (command[0] == 0x3B)
					{
						jumpPos = (signed char)command[1];
						fprintf(txt, "Go back: %i\n\n", jumpPos);
						endSeq = 1;
					}

					else if (command[0] == 0x80)
					{
						if (automaticCtrl == 0)
						{
							curNoteLen = command[1];
							fprintf(txt, "Rest: %01X length\n", curNoteLen);
							seqPos += 2;
						}
						else if (automaticCtrl == 1)
						{
							fprintf(txt, "Rest\n");
							seqPos++;
						}

					}

					else if (command[0] > 0x80 && command[0] < 0xC0)
					{
						if (automaticCtrl == 0)
						{
							curNote = command[0];
							curNoteLen = command[1];
							fprintf(txt, "Play note: %01X, length: %01X\n", curNote, curNoteLen);
							seqPos += 2;
						}
						else if (automaticCtrl == 1)
						{
							curNote = command[0];
							fprintf(txt, "Play note: %01X\n", curNote);
							seqPos++;
						}

					}

					else if (command[0] >= 0xC0 && command[0] < 0xD0)
					{
						if (automaticCtrl == 0)
						{
							curNote = command[0];
							curNoteLen = command[1];
							fprintf(txt, "Play drum note: %01X, length: %01X\n", curNote, curNoteLen);
							seqPos += 2;
						}
						else if (automaticCtrl == 1)
						{
							curNote = command[0];
							fprintf(txt, "Play drum note: %01X\n", curNote);
							seqPos++;
						}
					}

					else if (command[0] == 0x3F)
					{
						fprintf(txt, "Set channel envelope/volume (v2): %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x40)
					{
						fprintf(txt, "Set duty?: %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x41)
					{
						fprintf(txt, "Set volume to E0\n");
						seqPos++;
					}

					else if (command[0] == 0x42)
					{
						fprintf(txt, "Set note size to D0\n");
						seqPos++;
					}

					else if (command[0] == 0x43)
					{
						fprintf(txt, "Turn off effects\n");
						seqPos++;
					}

					else if (command[0] == 0x44)
					{
						repNoteVal = command[1];
						repNoteTimes = command[2];
						fprintf(txt, "Repeat note: %01X for %i times\n", repNoteVal, repNoteTimes);
						seqPos += 3;
					}

					else if (command[0] == 0x45)
					{
						automaticCtrl = 1;
						automaticSpeed = command[1];;
						fprintf(txt, "Turn on automatic note length mode (manual) at speed %i (v2?)\n", automaticSpeed);
						seqPos += 2;
					}

					else
					{
						fprintf(txt, "Unknown command: %01X\n", command[0]);
						seqPos++;
					}
				}
			}

			/*Battletoads 1*/
			else if (format == 5)
			{
				while (endSeq == 0)
				{
					command[0] = exRomData[seqPos];
					command[1] = exRomData[seqPos + 1];
					command[2] = exRomData[seqPos + 2];
					command[3] = exRomData[seqPos + 3];
					command[4] = exRomData[seqPos + 4];

					if (command[0] == 0x00)
					{
						fprintf(txt, "End of sequence\n\n");
						endSeq = 1;
					}

					else if (command[0] == 0x01)
					{
						jumpPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						fprintf(txt, "Jump to position: 0x%04X\n\n", jumpPos);
						endSeq = 1;
					}

					else if (command[0] == 0x02)
					{
						fprintf(txt, "Switch waveform: %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x03)
					{
						fprintf(txt, "Set channel 3 envelope: %01X, %01X, %01X\n", command[1], command[2], command[3]);
						seqPos += 4;
					}

					else if (command[0] == 0x04)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase;
						macro1aRet = seqPos + 4;
						macro1aTimes = exRomData[seqPos + 1];
						fprintf(txt, "Go to macro (manual times): 0x%04X, %i times\n", macro1aPos, macro1aTimes);
						seqPos += 4;
					}

					else if (command[0] == 0x05)
					{
						fprintf(txt, "End of macro\n");
						seqPos++;
					}

					else if (command[0] == 0x06)
					{
						fprintf(txt, "Set panning 1\n");
						seqPos++;
					}

					else if (command[0] == 0x07)
					{
						fprintf(txt, "Set panning 2\n");
						seqPos++;
					}

					else if (command[0] == 0x08)
					{
						fprintf(txt, "Set panning 3\n");
						seqPos++;
					}

					else if (command[0] == 0x09)
					{
						fprintf(txt, "Set distortion effect?: %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x0A)
					{
						automaticCtrl = 1;
						automaticSpeed = command[1];;
						fprintf(txt, "Turn on automatic note length mode (manual) at speed %i\n", automaticSpeed);
						seqPos += 2;
					}

					else if (command[0] == 0x0B)
					{
						fprintf(txt, "Turn off automatic note length mode\n");
						automaticCtrl = 0;
						seqPos++;
					}

					else if (command[0] == 0x0C)
					{
						fprintf(txt, "Set duty noise, parameters?: %01X, %01X, %01X\n", command[1], command[2], command[3]);
						seqPos += 4;
					}

					else if (command[0] == 0x0D)
					{
						fprintf(txt, "Reset noise?\n");
						seqPos++;
					}

					else if (command[0] == 0x0E)
					{
						fprintf(txt, "Portamento up: %01X, %01X, %01X, %01X\n", command[1], command[2], command[3], command[4]);
						seqPos += 6;
					}

					else if (command[0] == 0x0F)
					{
						fprintf(txt, "Portamento down: %01X, %01X, %01X, %01X\n", command[1], command[2], command[3], command[4]);
						seqPos += 6;
					}

					else if (command[0] == 0x10)
					{
						fprintf(txt, "Set drum length?: %i\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x12)
					{
						fprintf(txt, "Change tempo: %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x13)
					{
						transpose2 = (signed char)command[1];
						fprintf(txt, "Set transpose 1: %i\n", transpose2);
						seqPos += 2;
					}

					else if (command[0] == 0x14)
					{
						transpose1 = (signed char)command[1];
						fprintf(txt, "Set transpose 2: %i\n", transpose1);
						seqPos += 2;
					}

					else if (command[0] == 0x15)
					{
						transpose2 = (signed char)command[1];
						fprintf(txt, "Set distortion pitch effect?: %i\n", transpose2);
						seqPos += 2;
					}

					else if (command[0] == 0x16)
					{
						fprintf(txt, "Turn on vibrato: %01X, %01X, %01X, %01X\n", command[1], command[2], command[3], command[4]);
						seqPos += 5;
					}

					else if (command[0] == 0x17)
					{
						automaticCtrl = 1;
						automaticSpeed = 4;
						fprintf(txt, "Turn on automatic note length mode at speed %i\n", automaticSpeed);
						seqPos++;
					}

					else if (command[0] == 0x18)
					{
						automaticCtrl = 1;
						automaticSpeed = 8;
						fprintf(txt, "Turn on automatic note length mode at speed %i\n", automaticSpeed);
						seqPos++;
					}

					else if (command[0] == 0x19)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 1;
						fprintf(txt, "Go to macro %i time: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x1A)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 2;
						fprintf(txt, "Go to macro %i times: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x1B)
					{
						fprintf(txt, "Turn off effects?\n");
						seqPos++;
					}

					else if (command[0] == 0x1C)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 3;
						fprintf(txt, "Go to macro %i times: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x1D)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 4;
						fprintf(txt, "Go to macro %i times: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x1E)
					{
						automaticCtrl = 1;
						automaticSpeed = 2;
						fprintf(txt, "Turn on automatic note length mode at speed %i\n", automaticSpeed);
						seqPos++;
					}

					else if (command[0] == 0x1F)
					{
						transpose1 = 0;
						fprintf(txt, "Turn off transpose\n");
						seqPos++;
					}

					else if (command[0] == 0x20)
					{
						transpose1 = 7;
						fprintf(txt, "Set transpose to %i\n", transpose1);
						seqPos++;
					}

					else if (command[0] == 0x21)
					{
						transpose1 = 12;
						fprintf(txt, "Set transpose to %i\n", transpose1);
						seqPos++;
					}

					else if (command[0] == 0x22)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 8;
						fprintf(txt, "Go to macro %i times: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x23)
					{
						automaticCtrl = 1;
						automaticSpeed = 12;
						fprintf(txt, "Turn on automatic note length mode at speed %i\n", automaticSpeed);
						seqPos++;
					}

					else if (command[0] == 0x24)
					{
						automaticCtrl = 1;
						automaticSpeed = 1;
						fprintf(txt, "Turn on automatic note length mode at speed %i\n", automaticSpeed);
						seqPos++;
					}

					else if (command[0] == 0x25)
					{
						automaticCtrl = 1;
						automaticSpeed = 3;
						fprintf(txt, "Turn on automatic note length mode at speed %i\n", automaticSpeed);
						seqPos++;
					}

					else if (command[0] == 0x26)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 16;
						fprintf(txt, "Go to macro %i times: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x27)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 13;
						fprintf(txt, "Go to macro %i times: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x28)
					{
						fprintf(txt, "Set decay?: %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x29)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 7;
						fprintf(txt, "Go to macro %i times: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x2A)
					{
						fprintf(txt, "Load duty value with 80: %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x2B)
					{
						fprintf(txt, "Load duty value with 40: %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x2C)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 6;
						fprintf(txt, "Go to macro %i times: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x2D)
					{
						fprintf(txt, "Load duty value with D0: %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x2E)
					{
						fprintf(txt, "Switch wave instrument? (v1)\n");
						seqPos++;
					}

					else if (command[0] == 0x2F)
					{
						fprintf(txt, "Switch wave instrument? (v2)\n");
						seqPos++;
					}

					else if (command[0] == 0x30)
					{
						automaticCtrl = 0;
						fprintf(txt, "Turn off automatic note length and end of macro\n");
						seqPos++;
					}

					else if (command[0] == 0x31)
					{
						automaticCtrl = 1;
						fprintf(txt, "Turn on automatic note length and end of macro?\n");
						seqPos++;
					}

					else if (command[0] > 0x31 && command[0] < 0x80)
					{
						fprintf(txt, "Change envelope/volume: %01X\n", command[0]);
						seqPos++;
					}

					else if (command[0] == 0x80)
					{
						if (automaticCtrl == 0)
						{
							curNoteLen = command[1];
							fprintf(txt, "Rest: %01X length\n", curNoteLen);
							seqPos += 2;
						}
						else if (automaticCtrl == 1)
						{
							fprintf(txt, "Rest\n");
							seqPos++;
						}

					}

					else if (command[0] > 0x80 && command[0] < 0xC0)
					{
						if (automaticCtrl == 0)
						{
							curNote = command[0];
							curNoteLen = command[1];
							fprintf(txt, "Play note: %01X, length: %01X\n", curNote, curNoteLen);
							seqPos += 2;
						}
						else if (automaticCtrl == 1)
						{
							curNote = command[0];
							fprintf(txt, "Play note: %01X\n", curNote);
							seqPos++;
						}

					}

					else if (command[0] >= 0xC0 && command[0] < 0xD0)
					{
						if (automaticCtrl == 0)
						{
							curNote = command[0];
							curNoteLen = command[1];
							fprintf(txt, "Play drum note: %01X, length: %01X\n", curNote, curNoteLen);
							seqPos += 2;
						}
						else if (automaticCtrl == 1)
						{
							curNote = command[0];
							fprintf(txt, "Play drum note: %01X\n", curNote);
							seqPos++;
						}
					}

					else if (command[0] == 0xE0)
					{
						if (automaticCtrl == 0)
						{
							curNote = command[0];
							curNoteLen = command[1];
							fprintf(txt, "Play note E0: %01X, length: %01X\n", curNote, curNoteLen);
							seqPos += 2;
						}
						else if (automaticCtrl == 1)
						{
							curNote = command[0];
							fprintf(txt, "Play note E0: %01X\n", curNote);
							seqPos++;
						}
					}


					else
					{
						fprintf(txt, "Unknown command: %01X\n", command[0]);
						seqPos++;
					}
				}
			}

			/*WWF Superstars*/
			else if (format == 6)
			{
				while (endSeq == 0)
				{
					command[0] = exRomData[seqPos];
					command[1] = exRomData[seqPos + 1];
					command[2] = exRomData[seqPos + 2];
					command[3] = exRomData[seqPos + 3];
					command[4] = exRomData[seqPos + 4];

					if (command[0] == 0x00)
					{
						fprintf(txt, "End of sequence\n\n");
						endSeq = 1;
					}

					else if (command[0] == 0x01)
					{
						jumpPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						fprintf(txt, "Jump to position: 0x%04X\n\n", jumpPos);
						endSeq = 1;
					}

					else if (command[0] == 0x02)
					{
						fprintf(txt, "Switch waveform: %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x03)
					{
						fprintf(txt, "Set channel 3 envelope: %01X, %01X, %01X\n", command[1], command[2], command[3]);
						seqPos += 4;
					}

					else if (command[0] == 0x04)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase;
						macro1aRet = seqPos + 4;
						macro1aTimes = exRomData[seqPos + 1];
						fprintf(txt, "Go to macro (manual times): 0x%04X, %i times\n", macro1aPos, macro1aTimes);
						seqPos += 4;
					}

					else if (command[0] == 0x05)
					{
						fprintf(txt, "End of macro\n");
						seqPos++;
					}

					else if (command[0] == 0x06)
					{
						fprintf(txt, "Set panning 1\n");
						seqPos++;
					}

					else if (command[0] == 0x07)
					{
						fprintf(txt, "Set panning 2\n");
						seqPos++;
					}

					else if (command[0] == 0x08)
					{
						fprintf(txt, "Set panning 3\n");
						seqPos++;
					}

					else if (command[0] == 0x09)
					{
						fprintf(txt, "Set distortion effect?: %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x0A)
					{
						automaticCtrl = 1;
						automaticSpeed = command[1];;
						fprintf(txt, "Turn on automatic note length mode (manual) at speed %i\n", automaticSpeed);
						seqPos += 2;
					}

					else if (command[0] == 0x0B)
					{
						fprintf(txt, "Turn off automatic note length mode\n");
						automaticCtrl = 0;
						seqPos++;
					}

					else if (command[0] == 0x0C)
					{
						fprintf(txt, "Set duty noise, parameters?: %01X, %01X, %01X\n", command[1], command[2], command[3]);
						seqPos += 4;
					}

					else if (command[0] == 0x0D)
					{
						fprintf(txt, "Reset noise?\n");
						seqPos++;
					}

					else if (command[0] == 0x0E)
					{
						fprintf(txt, "Portamento up: %01X, %01X, %01X, %01X\n", command[1], command[2], command[3], command[4]);
						seqPos += 6;
					}

					else if (command[0] == 0x0F)
					{
						fprintf(txt, "Portamento down: %01X, %01X, %01X, %01X\n", command[1], command[2], command[3], command[4]);
						seqPos += 6;
					}

					else if (command[0] == 0x10)
					{
						fprintf(txt, "Set drum length?: %i\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x12)
					{
						fprintf(txt, "Change tempo: %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x13)
					{
						transpose2 = (signed char)command[1];
						fprintf(txt, "Set transpose 1: %i\n", transpose2);
						seqPos += 2;
					}

					else if (command[0] == 0x14)
					{
						transpose1 = (signed char)command[1];
						fprintf(txt, "Set transpose 2: %i\n", transpose1);
						seqPos += 2;
					}

					else if (command[0] == 0x15)
					{
						transpose2 = (signed char)command[1];
						fprintf(txt, "Set distortion pitch effect?: %i\n", transpose2);
						seqPos += 2;
					}

					else if (command[0] == 0x16)
					{
						fprintf(txt, "Turn on vibrato: %01X, %01X, %01X, %01X\n", command[1], command[2], command[3], command[4]);
						seqPos += 5;
					}

					else if (command[0] == 0x17)
					{
						automaticCtrl = 1;
						automaticSpeed = 4;
						fprintf(txt, "Turn on automatic note length mode at speed %i\n", automaticSpeed);
						seqPos++;
					}

					else if (command[0] == 0x18)
					{
						fprintf(txt, "Set volume to 40\n");
						seqPos++;
					}

					else if (command[0] == 0x19)
					{
						fprintf(txt, "Set volume to 80\n");
						seqPos++;
					}

					else if (command[0] == 0x1A)
					{
						fprintf(txt, "Set volume to 20\n");
						seqPos++;
					}

					else if (command[0] == 0x1B)
					{
						automaticCtrl = 1;
						automaticSpeed = 8;
						fprintf(txt, "Turn on automatic note length mode at speed %i\n", automaticSpeed);
						seqPos++;
					}

					else if (command[0] == 0x1C)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 1;
						fprintf(txt, "Go to macro %i time: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x1D)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 2;
						fprintf(txt, "Go to macro %i times: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x1E)
					{
						fprintf(txt, "Turn off vibrato?\n");
						seqPos++;
					}

					else if (command[0] == 0x1F)
					{
						fprintf(txt, "Set volume to 70\n");
						seqPos++;
					}

					else if (command[0] == 0x20)
					{
						fprintf(txt, "Set volume to 30\n");
						seqPos++;
					}


					else if (command[0] > 0x20 && command[0] < 0x80)
					{
						fprintf(txt, "Change envelope/volume?: %01X\n", command[0]);
						seqPos++;
					}

					else if (command[0] == 0x80)
					{
						if (automaticCtrl == 0)
						{
							curNoteLen = command[1];
							fprintf(txt, "Rest: %01X length\n", curNoteLen);
							seqPos += 2;
						}
						else if (automaticCtrl == 1)
						{
							fprintf(txt, "Rest\n");
							seqPos++;
						}

					}

					else if (command[0] > 0x80 && command[0] < 0xC0)
					{
						if (automaticCtrl == 0)
						{
							curNote = command[0];
							curNoteLen = command[1];
							fprintf(txt, "Play note: %01X, length: %01X\n", curNote, curNoteLen);
							seqPos += 2;
						}
						else if (automaticCtrl == 1)
						{
							curNote = command[0];
							fprintf(txt, "Play note: %01X\n", curNote);
							seqPos++;
						}

					}

					else if (command[0] >= 0xC0 && command[0] < 0xD0)
					{
						if (automaticCtrl == 0)
						{
							curNote = command[0];
							curNoteLen = command[1];
							fprintf(txt, "Play drum note: %01X, length: %01X\n", curNote, curNoteLen);
							seqPos += 2;
						}
						else if (automaticCtrl == 1)
						{
							curNote = command[0];
							fprintf(txt, "Play drum note: %01X\n", curNote);
							seqPos++;
						}
					}

					else if (command[0] == 0xFF)
					{
						if (automaticCtrl == 0)
						{
							curNote = command[0];
							curNoteLen = command[1];
							fprintf(txt, "Play note FF: %01X, length: %01X\n", curNote, curNoteLen);
							seqPos += 2;
						}
						else if (automaticCtrl == 1)
						{
							curNote = command[0];
							fprintf(txt, "Play note FF: %01X\n", curNote);
							seqPos++;
						}
					}

					else
					{
						fprintf(txt, "Unknown command: %01X\n", command[0]);
						seqPos++;
					}
				}
			}

			/*Beetlejuice*/
			else if (format == 7)
			{
				while (endSeq == 0)
				{
					command[0] = exRomData[seqPos];
					command[1] = exRomData[seqPos + 1];
					command[2] = exRomData[seqPos + 2];
					command[3] = exRomData[seqPos + 3];
					command[4] = exRomData[seqPos + 4];

					if (command[0] == 0x00)
					{
						fprintf(txt, "End of sequence\n\n");
						endSeq = 1;
					}

					else if (command[0] == 0x01)
					{
						jumpPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						fprintf(txt, "Jump to position: 0x%04X\n\n", jumpPos);
						endSeq = 1;
					}

					else if (command[0] == 0x02)
					{
						fprintf(txt, "Switch waveform: %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x03)
					{
						fprintf(txt, "Set channel 3 envelope: %01X, %01X, %01X\n", command[1], command[2], command[3]);
						seqPos += 4;
					}

					else if (command[0] == 0x04)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase;
						macro1aRet = seqPos + 4;
						macro1aTimes = exRomData[seqPos + 1];
						fprintf(txt, "Go to macro (manual times): 0x%04X, %i times\n", macro1aPos, macro1aTimes);
						seqPos += 4;
					}

					else if (command[0] == 0x05)
					{
						fprintf(txt, "End of macro\n");
						seqPos++;
					}

					else if (command[0] == 0x06)
					{
						fprintf(txt, "Set panning 1\n");
						seqPos++;
					}

					else if (command[0] == 0x07)
					{
						fprintf(txt, "Set panning 2\n");
						seqPos++;
					}

					else if (command[0] == 0x08)
					{
						fprintf(txt, "Set panning 3\n");
						seqPos++;
					}

					else if (command[0] == 0x09)
					{
						fprintf(txt, "Set distortion effect?: %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x0A)
					{
						automaticCtrl = 1;
						automaticSpeed = command[1];;
						fprintf(txt, "Turn on automatic note length mode (manual) at speed %i\n", automaticSpeed);
						seqPos += 2;
					}

					else if (command[0] == 0x0B)
					{
						fprintf(txt, "Turn off automatic note length mode\n");
						automaticCtrl = 0;
						seqPos++;
					}

					else if (command[0] == 0x0C)
					{
						fprintf(txt, "Set duty noise, parameters?: %01X, %01X, %01X\n", command[1], command[2], command[3]);
						seqPos += 4;
					}

					else if (command[0] == 0x0D)
					{
						fprintf(txt, "Reset noise?\n");
						seqPos++;
					}

					else if (command[0] == 0x0E)
					{
						fprintf(txt, "Portamento up: %01X, %01X, %01X, %01X\n", command[1], command[2], command[3], command[4]);
						seqPos += 6;
					}

					else if (command[0] == 0x0F)
					{
						fprintf(txt, "Portamento down: %01X, %01X, %01X, %01X\n", command[1], command[2], command[3], command[4]);
						seqPos += 6;
					}

					else if (command[0] == 0x10)
					{
						fprintf(txt, "Set drum length?: %i\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x11)
					{
						fprintf(txt, "Change tempo (absolute?): %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x12)
					{
						fprintf(txt, "Change tempo (add to amount): %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x13)
					{
						transpose2 = (signed char)command[1];
						fprintf(txt, "Set transpose 1: %i\n", transpose2);
						seqPos += 2;
					}

					else if (command[0] == 0x14)
					{
						transpose1 = (signed char)command[1];
						fprintf(txt, "Set transpose 2: %i\n", transpose1);
						seqPos += 2;
					}

					else if (command[0] == 0x15)
					{
						transpose2 = (signed char)command[1];
						fprintf(txt, "Set distortion pitch effect?: %i\n", transpose2);
						seqPos += 2;
					}

					else if (command[0] == 0x16)
					{
						fprintf(txt, "Turn on vibrato: %01X, %01X, %01X, %01X\n", command[1], command[2], command[3], command[4]);
						seqPos += 5;
					}

					else if (command[0] == 0x17)
					{
						automaticCtrl = 1;
						automaticSpeed = 4;
						fprintf(txt, "Turn on automatic note length mode at speed %i\n", automaticSpeed);
						seqPos++;
					}

					else if (command[0] == 0x18)
					{
						fprintf(txt, "Set volume to 40\n");
						seqPos++;
					}

					else if (command[0] == 0x19)
					{
						fprintf(txt, "Set volume to 80\n");
						seqPos++;
					}

					else if (command[0] == 0x1A)
					{
						fprintf(txt, "Set volume to 20\n");
						seqPos++;
					}

					else if (command[0] == 0x1B)
					{
						automaticCtrl = 1;
						automaticSpeed = 8;
						fprintf(txt, "Turn on automatic note length mode at speed %i\n", automaticSpeed);
						seqPos++;
					}

					else if (command[0] == 0x1C)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 1;
						fprintf(txt, "Go to macro %i time: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x1D)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 2;
						fprintf(txt, "Go to macro %i times: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x1E)
					{
						fprintf(txt, "Turn off effects?\n");
						seqPos++;
					}

					else if (command[0] == 0x1F)
					{
						fprintf(txt, "Set volume to 70\n");
						seqPos++;
					}

					else if (command[0] == 0x20)
					{
						fprintf(txt, "Set volume to 30\n");
						seqPos++;
					}

					else if (command[0] == 0x21)
					{
						automaticCtrl = 1;
						automaticSpeed = 2;
						fprintf(txt, "Turn on automatic note length mode at speed %i\n", automaticSpeed);
						seqPos++;
					}

					else if (command[0] == 0x22)
					{
						fprintf(txt, "Set volume to 50\n");
						seqPos++;
					}

					else if (command[0] == 0x23)
					{
						fprintf(txt, "Set volume to 90\n");
						seqPos++;
					}

					else if (command[0] == 0x24)
					{
						automaticCtrl = 1;
						automaticSpeed = 12;
						fprintf(txt, "Turn on automatic note length mode at speed %i\n", automaticSpeed);
						seqPos++;
					}

					else if (command[0] > 0x24 && command[0] < 0x80)
					{
						fprintf(txt, "Change envelope/volume: %01X\n", command[0]);
						seqPos++;
					}

					else if (command[0] == 0x80)
					{
						if (automaticCtrl == 0)
						{
							curNoteLen = command[1];
							fprintf(txt, "Rest: %01X length\n", curNoteLen);
							seqPos += 2;
						}
						else if (automaticCtrl == 1)
						{
							fprintf(txt, "Rest\n");
							seqPos++;
						}

					}

					else if (command[0] > 0x80 && command[0] < 0xC0)
					{
						if (automaticCtrl == 0)
						{
							curNote = command[0];
							curNoteLen = command[1];
							fprintf(txt, "Play note: %01X, length: %01X\n", curNote, curNoteLen);
							seqPos += 2;
						}
						else if (automaticCtrl == 1)
						{
							curNote = command[0];
							fprintf(txt, "Play note: %01X\n", curNote);
							seqPos++;
						}

					}

					else if (command[0] >= 0xC0 && command[0] < 0xD0)
					{
						if (automaticCtrl == 0)
						{
							curNote = command[0];
							curNoteLen = command[1];
							fprintf(txt, "Play drum note: %01X, length: %01X\n", curNote, curNoteLen);
							seqPos += 2;
						}
						else if (automaticCtrl == 1)
						{
							curNote = command[0];
							fprintf(txt, "Play drum note: %01X\n", curNote);
							seqPos++;
						}
					}

					else if (command[0] == 0xE0)
					{
						if (automaticCtrl == 0)
						{
							curNote = command[0];
							curNoteLen = command[1];
							fprintf(txt, "Play note E0: %01X, length: %01X\n", curNote, curNoteLen);
							seqPos += 2;
						}
						else if (automaticCtrl == 1)
						{
							curNote = command[0];
							fprintf(txt, "Play note E0: %01X\n", curNote);
							seqPos++;
						}
					}


					else
					{
						fprintf(txt, "Unknown command: %01X\n", command[0]);
						seqPos++;
					}
				}
			}

			/*Sneaky Snakes*/
			else if (format == 8)
			{
				while (endSeq == 0)
				{
					command[0] = exRomData[seqPos];
					command[1] = exRomData[seqPos + 1];
					command[2] = exRomData[seqPos + 2];
					command[3] = exRomData[seqPos + 3];
					command[4] = exRomData[seqPos + 4];

					if (command[0] == 0x00)
					{
						fprintf(txt, "End of sequence\n\n");
						endSeq = 1;
					}

					else if (command[0] == 0x01)
					{
						jumpPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						fprintf(txt, "Jump to position: 0x%04X\n\n", jumpPos);
						endSeq = 1;
					}

					else if (command[0] == 0x02)
					{
						fprintf(txt, "Switch waveform: %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x03)
					{
						fprintf(txt, "Set channel 3 envelope: %01X, %01X, %01X\n", command[1], command[2], command[3]);
						seqPos += 4;
					}

					else if (command[0] == 0x04)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase;
						macro1aRet = seqPos + 4;
						macro1aTimes = exRomData[seqPos + 1];
						fprintf(txt, "Go to macro (manual times): 0x%04X, %i times\n", macro1aPos, macro1aTimes);
						seqPos += 4;
					}

					else if (command[0] == 0x05)
					{
						fprintf(txt, "End of macro\n");
						seqPos++;
					}

					else if (command[0] == 0x06)
					{
						fprintf(txt, "Set panning 1\n");
						seqPos++;
					}

					else if (command[0] == 0x07)
					{
						fprintf(txt, "Set panning 2\n");
						seqPos++;
					}

					else if (command[0] == 0x08)
					{
						fprintf(txt, "Set panning 3\n");
						seqPos++;
					}

					else if (command[0] == 0x09)
					{
						fprintf(txt, "Set distortion effect?: %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x0A)
					{
						automaticCtrl = 1;
						automaticSpeed = command[1];;
						fprintf(txt, "Turn on automatic note length mode (manual) at speed %i\n", automaticSpeed);
						seqPos += 2;
					}

					else if (command[0] == 0x0B)
					{
						fprintf(txt, "Turn off automatic note length mode\n");
						automaticCtrl = 0;
						seqPos++;
					}

					else if (command[0] == 0x0C)
					{
						fprintf(txt, "Set duty noise, parameters?: %01X, %01X, %01X\n", command[1], command[2], command[3]);
						seqPos += 4;
					}

					else if (command[0] == 0x0D)
					{
						fprintf(txt, "Reset noise?\n");
						seqPos++;
					}

					else if (command[0] == 0x0E)
					{
						fprintf(txt, "Portamento up: %01X, %01X, %01X, %01X\n", command[1], command[2], command[3], command[4]);
						seqPos += 6;
					}

					else if (command[0] == 0x0F)
					{
						fprintf(txt, "Portamento down: %01X, %01X, %01X, %01X\n", command[1], command[2], command[3], command[4]);
						seqPos += 6;
					}

					else if (command[0] == 0x10)
					{
						fprintf(txt, "Set drum length?: %i\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x11)
					{
						fprintf(txt, "Change tempo (absolute?): %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x12)
					{
						fprintf(txt, "Change tempo (add to amount): %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x13)
					{
						transpose2 = (signed char)command[1];
						fprintf(txt, "Set transpose 1: %i\n", transpose2);
						seqPos += 2;
					}

					else if (command[0] == 0x14)
					{
						transpose1 = (signed char)command[1];
						fprintf(txt, "Set transpose 2: %i\n", transpose1);
						seqPos += 2;
					}

					else if (command[0] == 0x15)
					{
						transpose2 = (signed char)command[1];
						fprintf(txt, "Set distortion pitch effect?: %i\n", transpose2);
						seqPos += 2;
					}

					else if (command[0] == 0x16)
					{
						fprintf(txt, "Turn on vibrato: %01X, %01X, %01X, %01X\n", command[1], command[2], command[3], command[4]);
						seqPos += 5;
					}

					else if (command[0] == 0x17)
					{
						fprintf(txt, "Set duty to 40 (v1)\n");
						seqPos++;
					}

					else if (command[0] == 0x18)
					{
						fprintf(txt, "Set duty to 40 (v2)\n");
						seqPos++;
					}

					else if (command[0] == 0x19)
					{
						fprintf(txt, "Set duty to 80 (v1)\n");
						seqPos++;
					}

					else if (command[0] == 0x1A)
					{
						fprintf(txt, "Set duty to 80 (v2)\n");
						seqPos++;
					}

					else if (command[0] > 0x1A && command[0] < 0x80)
					{
						fprintf(txt, "Change envelope/volume?: %01X\n", command[0]);
						seqPos++;
					}

					else if (command[0] == 0x80)
					{
						if (automaticCtrl == 0)
						{
							curNoteLen = command[1];
							fprintf(txt, "Rest: %01X length\n", curNoteLen);
							seqPos += 2;
						}
						else if (automaticCtrl == 1)
						{
							fprintf(txt, "Rest\n");
							seqPos++;
						}

					}

					else if (command[0] > 0x80 && command[0] < 0xC0)
					{
						if (automaticCtrl == 0)
						{
							curNote = command[0];
							curNoteLen = command[1];
							fprintf(txt, "Play note: %01X, length: %01X\n", curNote, curNoteLen);
							seqPos += 2;
						}
						else if (automaticCtrl == 1)
						{
							curNote = command[0];
							fprintf(txt, "Play note: %01X\n", curNote);
							seqPos++;
						}

					}

					else if (command[0] >= 0xC0 && command[0] < 0xD0)
					{
						if (automaticCtrl == 0)
						{
							curNote = command[0];
							curNoteLen = command[1];
							fprintf(txt, "Play drum note: %01X, length: %01X\n", curNote, curNoteLen);
							seqPos += 2;
						}
						else if (automaticCtrl == 1)
						{
							curNote = command[0];
							fprintf(txt, "Play drum note: %01X\n", curNote);
							seqPos++;
						}
					}

					else if (command[0] == 0xE0)
					{
						if (automaticCtrl == 0)
						{
							curNote = command[0];
							curNoteLen = command[1];
							fprintf(txt, "Play note E0: %01X, length: %01X\n", curNote, curNoteLen);
							seqPos += 2;
						}
						else if (automaticCtrl == 1)
						{
							curNote = command[0];
							fprintf(txt, "Play note E0: %01X\n", curNote);
							seqPos++;
						}
					}


					else
					{
						fprintf(txt, "Unknown command: %01X\n", command[0]);
						seqPos++;
					}
				}
			}

			/*Croc*/
			else if (format == 9)
			{
				while (endSeq == 0)
				{
					command[0] = exRomData[seqPos];
					command[1] = exRomData[seqPos + 1];
					command[2] = exRomData[seqPos + 2];
					command[3] = exRomData[seqPos + 3];
					command[4] = exRomData[seqPos + 4];

					if (command[0] == 0x00)
					{
						fprintf(txt, "End of sequence\n\n");
						endSeq = 1;
					}

					else if (command[0] == 0x01)
					{
						jumpPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						fprintf(txt, "Jump to position: 0x%04X\n", jumpPos);
						seqPos += 3;
					}

					else if (command[0] == 0x02)
					{
						fprintf(txt, "Go to loop point: %01X\n\n", command[1]);
						endSeq = 1;
					}

					else if (command[0] == 0x03)
					{
						fprintf(txt, "Set channel 3 envelope (v1): %01X, %01X, %01X\n", command[1], command[2], command[3]);
						seqPos += 4;
					}

					else if (command[0] == 0x04)
					{
						fprintf(txt, "Set channel 3 envelope (v2): %01X, %01X\n", command[1], command[2]);
						seqPos += 3;
					}

					else if (command[0] == 0x05)
					{
						fprintf(txt, "Switch waveform: %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x06)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase;
						macro1aRet = seqPos + 4;
						macro1aTimes = exRomData[seqPos + 1];
						fprintf(txt, "Go to macro (manual times): 0x%04X, %i times\n", macro1aPos, macro1aTimes);
						seqPos += 4;
					}

					else if (command[0] == 0x07)
					{
						fprintf(txt, "End of macro\n");
						seqPos++;
					}

					else if (command[0] == 0x08)
					{
						fprintf(txt, "Turn on automatic note length mode\n");
						automaticCtrl = 1;
						automaticSpeed = curNoteLen;
						seqPos++;
					}

					else if (command[0] == 0x09)
					{
						fprintf(txt, "Turn off automatic note length mode\n");
						automaticCtrl = 0;
						seqPos++;
					}

					else if (command[0] == 0x0A)
					{
						fprintf(txt, "Change note size (v1): %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x0B)
					{
						fprintf(txt, "Change note size (v2): %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x0C)
					{
						fprintf(txt, "Change note size (v3): %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x0D)
					{
						automaticCtrl = 1;
						automaticSpeed = command[1];;
						fprintf(txt, "Turn on automatic note length mode (manual) at speed %i\n", automaticSpeed);
						seqPos += 2;
					}

					else if (command[0] == 0x0E)
					{
						fprintf(txt, "Special effect, parameters?: %01X, %01X, %01X\n", command[1], command[2], command[3]);
						seqPos += 4;
					}

					else if (command[0] == 0x0F)
					{
						fprintf(txt, "Turn off special effect\n");
						seqPos++;
					}

					else if (command[0] == 0x10)
					{
						fprintf(txt, "Portamento: %01X, %01X, %01X, %01X\n", command[1], command[2], command[3], command[4]);
						seqPos += 5;
					}

					else if (command[0] == 0x11)
					{
						fprintf(txt, "Pitch bend effect: %01X, %01X, %01X\n", command[1], command[2], command[3]);
						seqPos += 4;
					}

					else if (command[0] == 0x12)
					{
						fprintf(txt, "Distortion effect (v1): %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x13)
					{
						fprintf(txt, "Distortion effect (v2): %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x14)
					{
						fprintf(txt, "Turn on vibrato: %01X, %01X, %01X, %01X\n", command[1], command[2], command[3], command[4]);
						seqPos += 5;
					}

					else if (command[0] == 0x15)
					{
						fprintf(txt, "Switch to duty 1, envelope: %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x16)
					{
						fprintf(txt, "Switch to duty 2, envelope: %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x17)
					{
						fprintf(txt, "Switch to duty 3, envelope: %01X\n", command[1]);
						seqPos += 2;
					}

					else if (command[0] == 0x18)
					{
						curVol = 0x00;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x19)
					{
						curVol = 0x10;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x1A)
					{
						curVol = 0x20;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x1B)
					{
						curVol = 0x30;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x1C)
					{
						curVol = 0x40;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x1D)
					{
						curVol = 0x50;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x1E)
					{
						curVol = 0x60;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x1F)
					{
						curVol = 0x70;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x20)
					{
						curVol = 0x80;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x21)
					{
						curVol = 0x90;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x22)
					{
						curVol = 0xA0;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x23)
					{
						curVol = 0xB0;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x24)
					{
						curVol = 0xC0;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x25)
					{
						curVol = 0xD0;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x26)
					{
						curVol = 0xE0;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x27)
					{
						curVol = 0xF0;
						fprintf(txt, "Set volume: %1X\n", curVol);
						seqPos++;
					}

					else if (command[0] == 0x28)
					{
						automaticCtrl = 1;
						automaticSpeed = 0;
						fprintf(txt, "Turn on automatic note length mode at speed %i\n", automaticSpeed);
						seqPos++;
					}

					else if (command[0] == 0x29)
					{
						automaticCtrl = 1;
						automaticSpeed = 1;
						fprintf(txt, "Turn on automatic note length mode at speed %i\n", automaticSpeed);
						seqPos++;
					}

					else if (command[0] == 0x2A)
					{
						automaticCtrl = 1;
						automaticSpeed = 2;
						fprintf(txt, "Turn on automatic note length mode at speed %i\n", automaticSpeed);
						seqPos++;
					}

					else if (command[0] == 0x2B)
					{
						automaticCtrl = 1;
						automaticSpeed = 3;
						fprintf(txt, "Turn on automatic note length mode at speed %i\n", automaticSpeed);
						seqPos++;
					}

					else if (command[0] == 0x2C)
					{
						automaticCtrl = 1;
						automaticSpeed = 4;
						fprintf(txt, "Turn on automatic note length mode at speed %i\n", automaticSpeed);
						seqPos++;
					}

					else if (command[0] == 0x2D)
					{
						automaticCtrl = 1;
						automaticSpeed = 5;
						fprintf(txt, "Turn on automatic note length mode at speed %i\n", automaticSpeed);
						seqPos++;
					}

					else if (command[0] == 0x2E)
					{
						automaticCtrl = 1;
						automaticSpeed = 6;
						fprintf(txt, "Turn on automatic note length mode at speed %i\n", automaticSpeed);
						seqPos++;
					}

					else if (command[0] == 0x2F)
					{
						automaticCtrl = 1;
						automaticSpeed = 7;
						fprintf(txt, "Turn on automatic note length mode at speed %i\n", automaticSpeed);
						seqPos++;
					}

					else if (command[0] == 0x30)
					{
						automaticCtrl = 1;
						automaticSpeed = 8;
						fprintf(txt, "Turn on automatic note length mode at speed %i\n", automaticSpeed);
						seqPos++;
					}

					else if (command[0] == 0x31)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 1;
						fprintf(txt, "Go to macro %i time: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x32)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 2;
						fprintf(txt, "Go to macro %i times: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x33)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 3;
						fprintf(txt, "Go to macro %i times: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x34)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 4;
						fprintf(txt, "Go to macro %i times: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x35)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 5;
						fprintf(txt, "Go to macro %i times: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x36)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 6;
						fprintf(txt, "Go to macro %i times: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x37)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 8;
						fprintf(txt, "Go to macro %i times: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x38)
					{
						macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase;
						macro1aRet = seqPos + 3;
						macro1aTimes = 16;
						fprintf(txt, "Go to macro %i times: 0x%04X\n", macro1aTimes, macro1aPos);
						seqPos += 3;
					}

					else if (command[0] == 0x80)
					{
						if (automaticCtrl == 0)
						{
							curNoteLen = command[1];
							fprintf(txt, "Rest: %01X length\n", curNoteLen);
							seqPos += 2;
						}
						else if (automaticCtrl == 1)
						{
							fprintf(txt, "Rest\n");
							seqPos++;
						}

						}

					else if (command[0] > 0x80 && command[0] < 0xB0)
					{
						if (automaticCtrl == 0)
						{
							curNote = command[0];
							curNoteLen = command[1];
							fprintf(txt, "Play note: %01X, length: %01X\n", curNote, curNoteLen);
							seqPos += 2;
						}
						else if (automaticCtrl == 1)
						{
							curNote = command[0];
							fprintf(txt, "Play note: %01X\n", curNote);
							seqPos++;
						}

					}

					else if (command[0] >= 0xB0)
					{
						if (automaticCtrl == 0)
						{
							curNote = command[0];
							curNoteLen = command[1];
							fprintf(txt, "Play drum note: %01X, length: %01X\n", curNote, curNoteLen);
							seqPos += 2;
						}
						else if (automaticCtrl == 1)
						{
							curNote = command[0];
							fprintf(txt, "Play drum note: %01X\n", curNote);
							seqPos++;
						}
					}

					else
					{
						fprintf(txt, "Unknown command: %01X\n", command[0]);
						seqPos++;
					}


				}
			}

		}

		fclose(txt);
	}
}