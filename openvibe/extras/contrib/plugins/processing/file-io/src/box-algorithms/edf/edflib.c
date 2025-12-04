/*
*****************************************************************************
*
* Copyright (c) 2009, 2010, 2011 Teunis van Beelen
* All rights reserved.
*
* email: teuniz@gmail.com
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY Teunis van Beelen ''AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL Teunis van Beelen BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*****************************************************************************
*/

/* compile with options "-D_LARGEFILE64_SOURCE -D_LARGEFILE_SOURCE" */

#include "edflib.h"

#define EDFLIB_VERSION 109
#define EDFLIB_MAXFILES 64

#if defined(__APPLE__) || defined(__MACH__) || defined(__APPLE_CC__)
#define fopeno fopen
#else
#define fseeko fseeko64
#define ftello ftello64
#define fopeno fopen64
#endif

#ifdef _WIN32
#ifndef __MINGW32__
/* needed for visual c */
#undef fseeko
#define fseeko _fseeki64
#undef ftello
#define ftello _ftelli64
#undef fopeno
#define fopeno fopen
#endif
#endif

/* max size of annotationtext */
#define EDFLIB_WRITE_MAX_ANNOTATION_LEN 40
/* bytes in datarecord for EDF annotations, must be a multiple of three and two */
#define EDFLIB_ANNOTATION_BYTES 114

struct edfparamblock
{
	char label[17];
	char transducer[81];
	char physdimension[9];
	double phys_min;
	double phys_max;
	int dig_min;
	int dig_max;
	char prefilter[81];
	int smp_per_record;
	char reserved[33];
	int offset;
	int buf_offset;
	double bitvalue;
	int annotation;
	long long sample_pntr;
};

struct edfhdrblock
{
	FILE* file_hdl;
	char path[1024];
	int writemode;
	char version[32];
	char patient[81];
	char recording[81];
	char plus_patientcode[81];
	char plus_gender[16];
	char plus_birthdate[16];
	char plus_patient_name[81];
	char plus_patient_additional[81];
	char plus_startdate[16];
	char plus_admincode[81];
	char plus_technician[81];
	char plus_equipment[81];
	char plus_recording_additional[81];
	long long l_starttime;
	int startdate_day;
	int startdate_month;
	int startdate_year;
	int starttime_second;
	int starttime_minute;
	int starttime_hour;
	char reserved[45];
	int hdrsize;
	int edfsignals;
	long long datarecords;
	int recordsize;
	int annot_ch[EDFLIB_MAXSIGNALS];
	int nr_annot_chns;
	int mapped_signals[EDFLIB_MAXSIGNALS];
	int edf;
	int edfplus;
	int bdf;
	int bdfplus;
	int discontinuous;
	int signal_write_sequence_pos;
	long long starttime_offset;
	double data_record_duration;
	long long long_data_record_duration;
	long long annots_in_file;
	struct edfparamblock* edfparam;
};

struct edf_annotationblock
{
	long long onset;
	char duration[16];
	char annotation[EDFLIB_MAX_ANNOTATION_LEN + 1];
	struct edf_annotationblock* former_annotation;
	struct edf_annotationblock* next_annotation;
} * annotationslist[EDFLIB_MAXFILES];

struct edf_write_annotationblock
{
	long long onset;
	long long duration;
	char annotation[EDFLIB_WRITE_MAX_ANNOTATION_LEN + 1];
	struct edf_write_annotationblock* former_annotation;
	struct edf_write_annotationblock* next_annotation;
} * write_annotationslist[EDFLIB_MAXFILES];

static int files_open = 0;

static struct edfhdrblock* hdrlist[EDFLIB_MAXFILES];

struct edfhdrblock* CheckEdfFile(FILE* inputfile, int* edfError);
int IsIntegerNumber(char* str);
int IsNumber(char* str);
int IsDurationNumber(char* str);
int IsOnsetNumber(char* str);
long long GetLongDuration(const char* str);
int GetAnnotations(struct edfhdrblock* edfhdr, int hdl, int annotations);
long long GetLongTime(char* str);
int WriteEdfHeader(struct edfhdrblock* hdr);
void Latin1ToASCII(char* str, const int len);
void Latin12UTF8(char* latin, const int len);
void RemovePaddingTrailingSpaces(char* str);
int AtoiNonlocalized(const char* str);
double AtofNonlocalized(const char* str);
int SprintNumberNonlocalized(char* str, const double nr);
int SprintIntNumberNonlocalized(char* str, int q, int minimum, const int sign);
int SprintLLNumberNonlocalized(char* str, long long q, int minimum, const int sign);
int FprintIntNumberNonlocalized(FILE* file, int q, int minimum, const int sign);
int FprintLLNumberNonlocalized(FILE* file, long long q, int minimum, const int sign);

int EdfopenFileReadonly(const char* path, struct edf_hdr_struct* edfhdr, const int readAnnotations)
{
	int i, error;

	struct edf_annotationblock* annot;

	if (readAnnotations < 0)
	{
		edfhdr->filetype = EDFLIB_INVALID_READ_ANNOTS_VALUE;
		return -1;
	}

	if (readAnnotations > 2)
	{
		edfhdr->filetype = EDFLIB_INVALID_READ_ANNOTS_VALUE;
		return -1;
	}

	memset(edfhdr, 0, sizeof(struct edf_hdr_struct));

	if (files_open >= EDFLIB_MAXFILES)
	{
		edfhdr->filetype = EDFLIB_MAXFILES_REACHED;

		return -1;
	}

	for (i = 0; i < EDFLIB_MAXFILES; ++i)
	{
		if (hdrlist[i] != NULL)
		{
			if (!(strcmp(path, hdrlist[i]->path)))
			{
				edfhdr->filetype = EDFLIB_FILE_ALREADY_OPENED;
				return -1;
			}
		}
	}

	FILE* file = fopeno(path, "rb");
	if (file == NULL)
	{
		edfhdr->filetype = EDFLIB_NO_SUCH_FILE_OR_DIRECTORY;
		return -1;
	}

	struct edfhdrblock* hdr = CheckEdfFile(file, &error);
	if (hdr == NULL)
	{
		edfhdr->filetype = error;
		fclose(file);
		return -1;
	}

	if (hdr->discontinuous)
	{
		edfhdr->filetype = EDFLIB_FILE_IS_DISCONTINUOUS;
		free(hdr->edfparam);
		free(hdr);
		fclose(file);
		return -1;
	}

	hdr->writemode = 0;

	for (i = 0; i < EDFLIB_MAXFILES; ++i)
	{
		if (hdrlist[i] == NULL)
		{
			hdrlist[i]     = hdr;
			edfhdr->handle = i;
			break;
		}
	}

	if ((hdr->edf) && (!(hdr->edfplus))) { edfhdr->filetype = EDFLIB_FILETYPE_EDF; }
	if (hdr->edfplus) { edfhdr->filetype = EDFLIB_FILETYPE_EDFPLUS; }
	if ((hdr->bdf) && (!(hdr->bdfplus))) { edfhdr->filetype = EDFLIB_FILETYPE_BDF; }
	if (hdr->bdfplus) { edfhdr->filetype = EDFLIB_FILETYPE_BDFPLUS; }

	edfhdr->edfsignals          = hdr->edfsignals - hdr->nr_annot_chns;
	edfhdr->file_duration       = hdr->long_data_record_duration * hdr->datarecords;
	edfhdr->startdate_day       = hdr->startdate_day;
	edfhdr->startdate_month     = hdr->startdate_month;
	edfhdr->startdate_year      = hdr->startdate_year;
	edfhdr->starttime_hour      = hdr->starttime_hour;
	edfhdr->starttime_second    = hdr->starttime_second;
	edfhdr->starttime_minute    = hdr->starttime_minute;
	edfhdr->starttime_subsecond = hdr->starttime_offset;
	edfhdr->datarecords_in_file = hdr->datarecords;
	edfhdr->datarecord_duration = hdr->long_data_record_duration;

	if ((!(hdr->edfplus)) && (!(hdr->bdfplus)))
	{
		strcpy(edfhdr->patient, hdr->patient);
		strcpy(edfhdr->recording, hdr->recording);
		edfhdr->patientcode[0]          = 0;
		edfhdr->gender[0]               = 0;
		edfhdr->birthdate[0]            = 0;
		edfhdr->patient_name[0]         = 0;
		edfhdr->patient_additional[0]   = 0;
		edfhdr->admincode[0]            = 0;
		edfhdr->technician[0]           = 0;
		edfhdr->equipment[0]            = 0;
		edfhdr->recording_additional[0] = 0;
	}
	else
	{
		edfhdr->patient[0]   = 0;
		edfhdr->recording[0] = 0;
		strcpy(edfhdr->patientcode, hdr->plus_patientcode);
		strcpy(edfhdr->gender, hdr->plus_gender);
		strcpy(edfhdr->birthdate, hdr->plus_birthdate);
		strcpy(edfhdr->patient_name, hdr->plus_patient_name);
		strcpy(edfhdr->patient_additional, hdr->plus_patient_additional);
		strcpy(edfhdr->admincode, hdr->plus_admincode);
		strcpy(edfhdr->technician, hdr->plus_technician);
		strcpy(edfhdr->equipment, hdr->plus_equipment);
		strcpy(edfhdr->recording_additional, hdr->plus_recording_additional);

		annotationslist[edfhdr->handle] = NULL;

		if ((readAnnotations == EDFLIB_READ_ANNOTATIONS) || (readAnnotations == EDFLIB_READ_ALL_ANNOTATIONS))
		{
			if (GetAnnotations(hdr, edfhdr->handle, readAnnotations))
			{
				edfhdr->filetype = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;

				if (annotationslist[edfhdr->handle])
				{
					annot = annotationslist[edfhdr->handle];

					while (annot->next_annotation)
					{
						annot = annot->next_annotation;
						free(annot->former_annotation);
					}

					free(annot);
				}

				fclose(file);
				free(hdr->edfparam);
				free(hdr);
				return -1;
			}
		}
	}

	if (annotationslist[edfhdr->handle])
	{
		hdr->annots_in_file++;
		annot = annotationslist[edfhdr->handle];

		while (annot->next_annotation)
		{
			hdr->annots_in_file++;
			annot = annot->next_annotation;
		}
	}

	edfhdr->annotations_in_file = hdr->annots_in_file;
	strcpy(hdr->path, path);
	files_open++;
	int j = 0;

	for (i = 0; i < hdr->edfsignals; ++i) { if (!(hdr->edfparam[i].annotation)) { hdr->mapped_signals[j++] = i; } }

	for (i = 0; i < edfhdr->edfsignals; ++i)
	{
		const int channel = hdr->mapped_signals[i];

		strcpy(edfhdr->signalparam[i].label, hdr->edfparam[channel].label);
		strcpy(edfhdr->signalparam[i].transducer, hdr->edfparam[channel].transducer);
		strcpy(edfhdr->signalparam[i].physdimension, hdr->edfparam[channel].physdimension);
		strcpy(edfhdr->signalparam[i].prefilter, hdr->edfparam[channel].prefilter);
		edfhdr->signalparam[i].smp_in_file       = hdr->edfparam[channel].smp_per_record * hdr->datarecords;
		edfhdr->signalparam[i].phys_max          = hdr->edfparam[channel].phys_max;
		edfhdr->signalparam[i].phys_min          = hdr->edfparam[channel].phys_min;
		edfhdr->signalparam[i].dig_max           = hdr->edfparam[channel].dig_max;
		edfhdr->signalparam[i].dig_min           = hdr->edfparam[channel].dig_min;
		edfhdr->signalparam[i].smp_in_datarecord = hdr->edfparam[channel].smp_per_record;
	}

	return 0;
}

int EdfcloseFile(const int handle)
{
	struct edf_write_annotationblock* annot2;
	int i, n, p;
	char str[EDFLIB_ANNOTATION_BYTES * 2];

	if (handle < 0) { return -1; }
	if (handle >= EDFLIB_MAXFILES) { return -1; }
	if (hdrlist[handle] == NULL) { return -1; }

	struct edfhdrblock* hdr = hdrlist[handle];

	if (hdr->writemode)
	{
		if (hdr->datarecords == 0LL)
		{
			if (WriteEdfHeader(hdr)) { return -1; }

			annot2 = write_annotationslist[handle];

			while (annot2)
			{
				p = FprintLLNumberNonlocalized(hdr->file_hdl, (hdr->datarecords * hdr->long_data_record_duration) / EDFLIB_TIME_DIMENSION, 0, 1);

				if (hdr->long_data_record_duration % EDFLIB_TIME_DIMENSION)
				{
					fputc('.', hdr->file_hdl);
					p++;
					p += FprintLLNumberNonlocalized(hdr->file_hdl, (hdr->datarecords * hdr->long_data_record_duration) % EDFLIB_TIME_DIMENSION, 7, 0);
				}
				fputc(20, hdr->file_hdl);
				fputc(20, hdr->file_hdl);
				p += 2;
				for (; p < EDFLIB_ANNOTATION_BYTES; ++p) { fputc(0, hdr->file_hdl); }

				hdr->datarecords++;
				annot2 = annot2->next_annotation;
			}
		}

		if (hdr->datarecords < 100000000LL)
		{
			fseeko(hdr->file_hdl, 236LL, SEEK_SET);
			p = FprintIntNumberNonlocalized(hdr->file_hdl, (int)(hdr->datarecords), 0, 0);
			if (p < 2) { fputc(' ', hdr->file_hdl); }
		}

		annot2                = write_annotationslist[handle];
		long long datarecords = 0LL;
		long long offset      = (long long)((hdr->edfsignals + 2) * 256);
		int datrecsize        = EDFLIB_ANNOTATION_BYTES;

		for (i = 0; i < hdr->edfsignals; ++i)
		{
			if (hdr->edf)
			{
				offset += (long long)(hdr->edfparam[i].smp_per_record * 2);
				datrecsize += (hdr->edfparam[i].smp_per_record * 2);
			}
			else
			{
				offset += (long long)(hdr->edfparam[i].smp_per_record * 3);
				datrecsize += (hdr->edfparam[i].smp_per_record * 3);
			}
		}

		while (annot2 != NULL)
		{
			if (fseeko(hdr->file_hdl, offset, SEEK_SET)) { break; }

			p = SprintLLNumberNonlocalized(str, (datarecords * hdr->long_data_record_duration) / EDFLIB_TIME_DIMENSION, 0, 1);

			if (hdr->long_data_record_duration % EDFLIB_TIME_DIMENSION)
			{
				str[p++] = '.';
				n        = SprintLLNumberNonlocalized(str + p, (datarecords * hdr->long_data_record_duration) % EDFLIB_TIME_DIMENSION, 7, 0);
				p += n;
			}
			str[p++] = 20;
			str[p++] = 20;
			str[p++] = 0;

			n = SprintLLNumberNonlocalized(str + p, annot2->onset / 10000LL, 0, 1);
			p += n;
			if (annot2->onset % 10000LL)
			{
				str[p++] = '.';
				n        = SprintLLNumberNonlocalized(str + p, annot2->onset % 10000LL, 4, 0);
				p += n;
			}
			if (annot2->duration >= 0LL)
			{
				str[p++] = 21;
				n        = SprintLLNumberNonlocalized(str + p, annot2->duration / 10000LL, 0, 0);
				p += n;
				if (annot2->duration % 10000LL)
				{
					str[p++] = '.';
					n        = SprintLLNumberNonlocalized(str + p, annot2->duration % 10000LL, 4, 0);
					p += n;
				}
			}
			str[p++] = 20;
			for (i = 0; i < EDFLIB_WRITE_MAX_ANNOTATION_LEN; ++i)
			{
				if (annot2->annotation[i] == 0) { break; }
				str[p++] = annot2->annotation[i];
			}
			str[p++] = 20;

			for (; p < EDFLIB_ANNOTATION_BYTES; ++p) { str[p] = 0; }

			fwrite(str, EDFLIB_ANNOTATION_BYTES, 1, hdr->file_hdl);
			offset += datrecsize;
			datarecords++;

			if (datarecords >= hdr->datarecords) { break; }
			annot2 = annot2->next_annotation;
		}

		fclose(hdr->file_hdl);

		if (write_annotationslist[handle] != NULL)
		{
			annot2 = write_annotationslist[handle];

			while (annot2->next_annotation)
			{
				annot2 = annot2->next_annotation;
				free(annot2->former_annotation);
			}

			free(annot2);
		}

		free(hdr->edfparam);
		free(hdr);
		hdrlist[handle] = NULL;
		files_open--;
		return 0;
	}
	if (annotationslist[handle] != NULL)
	{
		struct edf_annotationblock* annot = annotationslist[handle];

		while (annot->next_annotation)
		{
			annot = annot->next_annotation;
			free(annot->former_annotation);
		}
		free(annot);
	}

	fclose(hdr->file_hdl);
	free(hdr->edfparam);
	free(hdr);
	hdrlist[handle] = NULL;
	files_open--;
	return 0;
}

long long edfseek(const int handle, const int edfsignal, const long long offset, const int whence)
{
	if (handle < 0) { return -1; }
	if (handle >= EDFLIB_MAXFILES) { return -1; }
	if (hdrlist[handle] == NULL) { return -1; }
	if (edfsignal < 0) { return -1; }
	if (hdrlist[handle]->writemode) { return -1; }
	if (edfsignal >= (hdrlist[handle]->edfsignals - hdrlist[handle]->nr_annot_chns)) { return -1; }

	const int channel           = hdrlist[handle]->mapped_signals[edfsignal];
	const long long smp_in_file = hdrlist[handle]->edfparam[channel].smp_per_record * hdrlist[handle]->datarecords;

	if (whence == EDFSEEK_SET) { hdrlist[handle]->edfparam[channel].sample_pntr = offset; }
	if (whence == EDFSEEK_CUR) { hdrlist[handle]->edfparam[channel].sample_pntr += offset; }
	if (whence == EDFSEEK_END)
	{
		hdrlist[handle]->edfparam[channel].sample_pntr = (hdrlist[handle]->edfparam[channel].smp_per_record * hdrlist[handle]->datarecords) + offset;
	}
	if (hdrlist[handle]->edfparam[channel].sample_pntr > smp_in_file) { hdrlist[handle]->edfparam[channel].sample_pntr = smp_in_file; }
	if (hdrlist[handle]->edfparam[channel].sample_pntr < 0LL) { hdrlist[handle]->edfparam[channel].sample_pntr = 0LL; }
	return hdrlist[handle]->edfparam[channel].sample_pntr;
}

long long edftell(const int handle, const int edfsignal)
{
	if (handle < 0) { return -1; }
	if (handle >= EDFLIB_MAXFILES) { return -1; }
	if (hdrlist[handle] == NULL) { return -1; }
	if (edfsignal < 0) { return -1; }
	if (hdrlist[handle]->writemode) { return -1; }
	if (edfsignal >= (hdrlist[handle]->edfsignals - hdrlist[handle]->nr_annot_chns)) { return -1; }
	const int channel = hdrlist[handle]->mapped_signals[edfsignal];
	return hdrlist[handle]->edfparam[channel].sample_pntr;
}

void edfrewind(const int handle, const int edfsignal)
{
	if (handle < 0) { return; }
	if (handle >= EDFLIB_MAXFILES) { return; }
	if (hdrlist[handle] == NULL) { return; }
	if (edfsignal < 0) { return; }
	if (hdrlist[handle]->writemode) { return; }
	if (edfsignal >= (hdrlist[handle]->edfsignals - hdrlist[handle]->nr_annot_chns)) { return; }
	const int channel                              = hdrlist[handle]->mapped_signals[edfsignal];
	hdrlist[handle]->edfparam[channel].sample_pntr = 0LL;
}

int EdfreadPhysicalSamples(const int handle, const int edfsignal, int n, double* buf)
{
	int nBytes = 2, tmp, i;

	union
	{
		unsigned int one;
		signed int one_signed;
		unsigned short two[2];
		signed short two_signed[2];
		unsigned char four[4];
	} var;

	if (handle < 0) { return -1; }
	if (handle >= EDFLIB_MAXFILES) { return -1; }
	if (hdrlist[handle] == NULL) { return -1; }
	if (edfsignal < 0) { return -1; }
	if (hdrlist[handle]->writemode) { return -1; }
	if (edfsignal >= (hdrlist[handle]->edfsignals - hdrlist[handle]->nr_annot_chns)) { return -1; }

	const int channel = hdrlist[handle]->mapped_signals[edfsignal];

	if (n < 0LL) { return -1; }
	if (n == 0LL) { return 0LL; }

	struct edfhdrblock* hdr = hdrlist[handle];

	if (hdr->edf) { nBytes = 2; }
	if (hdr->bdf) { nBytes = 3; }

	const long long smpInFile = hdr->edfparam[channel].smp_per_record * hdr->datarecords;

	if ((hdr->edfparam[channel].sample_pntr + n) > smpInFile)
	{
		n = (int)(smpInFile - hdr->edfparam[channel].sample_pntr);

		if (n == 0) { return 0LL; }
		if (n < 0) { return -1; }
	}

	FILE* file = hdr->file_hdl;

	long long offset = hdr->hdrsize;
	offset += (hdr->edfparam[channel].sample_pntr / hdr->edfparam[channel].smp_per_record) * hdr->recordsize;
	offset += hdr->edfparam[channel].buf_offset;
	offset += ((hdr->edfparam[channel].sample_pntr % hdr->edfparam[channel].smp_per_record) * nBytes);

	fseeko(file, offset, SEEK_SET);

	long long samplePntr      = hdr->edfparam[channel].sample_pntr;
	const long long nSmp      = hdr->edfparam[channel].smp_per_record;
	const long long jump      = hdr->recordsize - (nSmp * nBytes);
	const double physBitvalue = hdr->edfparam[channel].bitvalue;
	const int physOffset      = hdr->edfparam[channel].offset;

	if (hdr->edf)
	{
		for (i = 0; i < n; ++i)
		{
			if (!(samplePntr % nSmp)) { if (i) { fseeko(file, jump, SEEK_CUR); } }

			var.four[0] = fgetc(file);
			tmp         = fgetc(file);
			if (tmp == EOF) { return -1; }
			var.four[1] = tmp;
			var.two_signed[0] += (short)physOffset;
			buf[i] = physBitvalue * (double)var.two_signed[0];
			samplePntr++;
		}
	}

	if (hdr->bdf)
	{
		for (i = 0; i < n; ++i)
		{
			if (!(samplePntr % nSmp)) { if (i) { fseeko(file, jump, SEEK_CUR); } }

			var.four[0] = fgetc(file);
			var.four[1] = fgetc(file);
			tmp         = fgetc(file);
			if (tmp == EOF) { return -1; }
			var.four[2] = tmp;

			if (var.four[2] & 0x80) { var.four[3] = 0xff; }
			else { var.four[3] = 0x00; }

			var.one_signed += physOffset;
			buf[i] = physBitvalue * (double)var.one_signed;
		}
	}

	hdr->edfparam[channel].sample_pntr = samplePntr;
	return n;
}

int EdfreadDigitalSamples(const int handle, const int edfsignal, int n, int* buf)
{
	int nBytes = 2, tmp, i;

	union
	{
		unsigned int one;
		signed int one_signed;
		unsigned short two[2];
		signed short two_signed[2];
		unsigned char four[4];
	} var;

	if (handle < 0) { return -1; }
	if (handle >= EDFLIB_MAXFILES) { return -1; }
	if (hdrlist[handle] == NULL) { return -1; }
	if (edfsignal < 0) { return -1; }
	if (hdrlist[handle]->writemode) { return -1; }
	if (edfsignal >= (hdrlist[handle]->edfsignals - hdrlist[handle]->nr_annot_chns)) { return -1; }
	if (n < 0LL) { return -1; }
	if (n == 0LL) { return 0LL; }

	const int channel = hdrlist[handle]->mapped_signals[edfsignal];


	struct edfhdrblock* hdr = hdrlist[handle];

	if (hdr->edf) { nBytes = 2; }
	if (hdr->bdf) { nBytes = 3; }

	const long long nSmp = hdr->edfparam[channel].smp_per_record * hdr->datarecords;

	if ((hdr->edfparam[channel].sample_pntr + n) > nSmp)
	{
		n = (int)(nSmp - hdr->edfparam[channel].sample_pntr);

		if (n == 0) { return 0LL; }
		if (n < 0) { return -1; }
	}

	FILE* file = hdr->file_hdl;

	long long offset = hdr->hdrsize;
	offset += (hdr->edfparam[channel].sample_pntr / hdr->edfparam[channel].smp_per_record) * hdr->recordsize;
	offset += hdr->edfparam[channel].buf_offset;
	offset += ((hdr->edfparam[channel].sample_pntr % hdr->edfparam[channel].smp_per_record) * nBytes);

	fseeko(file, offset, SEEK_SET);

	long long samplePntr         = hdr->edfparam[channel].sample_pntr;
	const long long smpPerRecord = hdr->edfparam[channel].smp_per_record;
	const long long jump         = hdr->recordsize - (smpPerRecord * nBytes);

	if (hdr->edf)
	{
		for (i = 0; i < n; ++i)
		{
			if (!(samplePntr % smpPerRecord)) { if (i) { fseeko(file, jump, SEEK_CUR); } }

			var.four[0] = fgetc(file);
			tmp         = fgetc(file);
			if (tmp == EOF) { return -1; }
			var.four[1] = tmp;
			buf[i]      = var.two_signed[0];
			samplePntr++;
		}
	}

	if (hdr->bdf)
	{
		for (i = 0; i < n; ++i)
		{
			if (!(samplePntr % smpPerRecord)) { if (i) { fseeko(file, jump, SEEK_CUR); } }

			var.four[0] = fgetc(file);
			var.four[1] = fgetc(file);
			tmp         = fgetc(file);
			if (tmp == EOF) { return -1; }
			var.four[2] = tmp;

			if (var.four[2] & 0x80) { var.four[3] = 0xff; }
			else { var.four[3] = 0x00; }

			buf[i] = var.one_signed;
			samplePntr++;
		}
	}
	hdr->edfparam[channel].sample_pntr = samplePntr;
	return n;
}

int EdfGetAnnotation(const int handle, const int n, struct edf_annotation_struct* annot)
{
	memset(annot, 0, sizeof(struct edf_annotation_struct));
	if (handle < 0) { return -1; }
	if (handle >= EDFLIB_MAXFILES) { return -1; }
	if (hdrlist[handle] == NULL) { return -1; }
	if (hdrlist[handle]->writemode) { return -1; }
	if (n < 0) { return -1; }
	if (n >= hdrlist[handle]->annots_in_file) { return -1; }
	struct edf_annotationblock* listAnnot = annotationslist[handle];
	if (listAnnot == NULL) { return -1; }

	for (int i = 0; i < n; ++i)
	{
		if (listAnnot->next_annotation == NULL) { return -1; }
		listAnnot = listAnnot->next_annotation;
	}

	annot->onset = listAnnot->onset;
	strcpy(annot->duration, listAnnot->duration);
	strcpy(annot->annotation, listAnnot->annotation);

	return 0;
}

struct edfhdrblock* CheckEdfFile(FILE* inputfile, int* edfError)
{
	int i, j, p, r = 0, n, dotposition, error;
	char *edfHdr, scratchpad[128], scratchpad2[64];
	struct edfhdrblock* edfhdr;

	/***************** check header ******************************/

	edfHdr = (char*)calloc(1, 256);
	if (edfHdr == NULL)
	{
		*edfError = EDFLIB_MALLOC_ERROR;
		return NULL;
	}

	edfhdr = (struct edfhdrblock*)calloc(1, sizeof(struct edfhdrblock));
	if (edfhdr == NULL)
	{
		free(edfHdr);
		*edfError = EDFLIB_MALLOC_ERROR;
		return NULL;
	}

	rewind(inputfile);
	if (fread(edfHdr, 256, 1, inputfile) != 1)
	{
		*edfError = EDFLIB_FILE_READ_ERROR;
		free(edfHdr);
		free(edfhdr);
		return NULL;
	}

	/**************************** VERSION ***************************************/

	strncpy(scratchpad, edfHdr, 8);
	scratchpad[8] = 0;

	if (scratchpad[0] == -1)   /* BDF-file */
	{
		for (i = 1; i < 8; ++i)
		{
			if ((scratchpad[i] < 32) || (scratchpad[i] > 126))
			{
				*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
				free(edfHdr);
				free(edfhdr);
				return NULL;
			}
		}

		if (strcmp(scratchpad + 1, "BIOSEMI") != 0)
		{
			*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
			free(edfHdr);
			free(edfhdr);
			return NULL;
		}

		edfhdr->bdf = 1;
	}
	else    /* EDF-file */
	{
		for (i = 0; i < 8; ++i)
		{
			if ((scratchpad[i] < 32) || (scratchpad[i] > 126))
			{
				*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
				free(edfHdr);
				free(edfhdr);
				return NULL;
			}
		}

		if (strcmp(scratchpad, "0       ") != 0)
		{
			*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
			free(edfHdr);
			free(edfhdr);
			return NULL;
		}

		edfhdr->edf = 1;
	}

	strncpy(edfhdr->version, edfHdr, 8);
	edfhdr->version[8] = 0;
	if (edfhdr->bdf) { edfhdr->version[0] = '.'; }

	/********************* PATIENTNAME *********************************************/

	strncpy(scratchpad, edfHdr + 8, 80);
	scratchpad[80] = 0;
	for (i = 0; i < 80; ++i)
	{
		if ((((unsigned char*)scratchpad)[i] < 32) || (((unsigned char*)scratchpad)[i] > 126))
		{
			*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
			free(edfHdr);
			free(edfhdr);
			return NULL;
		}
	}

	strncpy(edfhdr->patient, edfHdr + 8, 80);
	edfhdr->patient[80] = 0;

	/********************* RECORDING *********************************************/

	strncpy(scratchpad, edfHdr + 88, 80);
	scratchpad[80] = 0;
	for (i = 0; i < 80; ++i)
	{
		if ((((unsigned char*)scratchpad)[i] < 32) || (((unsigned char*)scratchpad)[i] > 126))
		{
			*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
			free(edfHdr);
			free(edfhdr);
			return NULL;
		}
	}

	strncpy(edfhdr->recording, edfHdr + 88, 80);
	edfhdr->recording[80] = 0;

	/********************* STARTDATE *********************************************/

	strncpy(scratchpad, edfHdr + 168, 8);
	scratchpad[8] = 0;
	for (i = 0; i < 8; ++i)
	{
		if ((scratchpad[i] < 32) || (scratchpad[i] > 126))
		{
			*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
			free(edfHdr);
			free(edfhdr);
			return NULL;
		}
	}

	error = 0;

	if ((edfHdr[170] != '.') || (edfHdr[173] != '.')) { error = 1; }
	if ((edfHdr[168] < 48) || (edfHdr[168] > 57)) { error = 1; }
	if ((edfHdr[169] < 48) || (edfHdr[169] > 57)) { error = 1; }
	if ((edfHdr[171] < 48) || (edfHdr[171] > 57)) { error = 1; }
	if ((edfHdr[172] < 48) || (edfHdr[172] > 57)) { error = 1; }
	if ((edfHdr[174] < 48) || (edfHdr[174] > 57)) { error = 1; }
	if ((edfHdr[175] < 48) || (edfHdr[175] > 57)) { error = 1; }
	strncpy(scratchpad, edfHdr + 168, 8);

	if (error)
	{
		scratchpad[8] = 0;
		*edfError     = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
		free(edfHdr);
		free(edfhdr);
		return NULL;
	}

	scratchpad[2] = 0;
	scratchpad[5] = 0;
	scratchpad[8] = 0;

	if ((AtofNonlocalized(scratchpad) < 1) || (AtofNonlocalized(scratchpad) > 31))
	{
		strncpy(scratchpad, edfHdr + 168, 8);
		scratchpad[8] = 0;
		*edfError     = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
		free(edfHdr);
		free(edfhdr);
		return NULL;
	}

	if ((AtofNonlocalized(scratchpad + 3) < 1) || (AtofNonlocalized(scratchpad + 3) > 12))
	{
		strncpy(scratchpad, edfHdr + 168, 8);
		scratchpad[8] = 0;
		*edfError     = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
		free(edfHdr);
		free(edfhdr);
		return NULL;
	}

	edfhdr->startdate_day   = (int)AtofNonlocalized(scratchpad);
	edfhdr->startdate_month = (int)AtofNonlocalized(scratchpad + 3);
	edfhdr->startdate_year  = (int)AtofNonlocalized(scratchpad + 6);
	if (edfhdr->startdate_year > 84) { edfhdr->startdate_year += 1900; }
	else { edfhdr->startdate_year += 2000; }

	/********************* STARTTIME *********************************************/

	strncpy(scratchpad, edfHdr + 176, 8);
	scratchpad[8] = 0;
	for (i = 0; i < 8; ++i)
	{
		if ((scratchpad[i] < 32) || (scratchpad[i] > 126))
		{
			*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
			free(edfHdr);
			free(edfhdr);
			return NULL;
		}
	}

	error = 0;

	if ((edfHdr[178] != '.') || (edfHdr[181] != '.')) { error = 1; }
	if ((edfHdr[176] < 48) || (edfHdr[176] > 57)) { error = 1; }
	if ((edfHdr[177] < 48) || (edfHdr[177] > 57)) { error = 1; }
	if ((edfHdr[179] < 48) || (edfHdr[179] > 57)) { error = 1; }
	if ((edfHdr[180] < 48) || (edfHdr[180] > 57)) { error = 1; }
	if ((edfHdr[182] < 48) || (edfHdr[182] > 57)) { error = 1; }
	if ((edfHdr[183] < 48) || (edfHdr[183] > 57)) { error = 1; }

	strncpy(scratchpad, edfHdr + 176, 8);

	if (error)
	{
		strncpy(scratchpad, edfHdr + 176, 8);
		scratchpad[8] = 0;
		*edfError     = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
		free(edfHdr);
		free(edfhdr);
		return NULL;
	}

	scratchpad[2] = 0;
	scratchpad[5] = 0;
	scratchpad[8] = 0;

	if (AtofNonlocalized(scratchpad) > 23)
	{
		strncpy(scratchpad, edfHdr + 176, 8);
		scratchpad[8] = 0;
		*edfError     = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
		free(edfHdr);
		free(edfhdr);
		return NULL;
	}

	if (AtofNonlocalized(scratchpad + 3) > 59)
	{
		strncpy(scratchpad, edfHdr + 176, 8);
		scratchpad[8] = 0;
		*edfError     = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
		free(edfHdr);
		free(edfhdr);
		return NULL;
	}

	if (AtofNonlocalized(scratchpad + 6) > 59)
	{
		strncpy(scratchpad, edfHdr + 176, 8);
		scratchpad[8] = 0;
		*edfError     = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
		free(edfHdr);
		free(edfhdr);
		return NULL;
	}

	edfhdr->starttime_hour   = (int)AtofNonlocalized(scratchpad);
	edfhdr->starttime_minute = (int)AtofNonlocalized(scratchpad + 3);
	edfhdr->starttime_second = (int)AtofNonlocalized(scratchpad + 6);

	edfhdr->l_starttime = (long long)(3600 * AtofNonlocalized(scratchpad));
	edfhdr->l_starttime += (long long)(60 * AtofNonlocalized(scratchpad + 3));
	edfhdr->l_starttime += (long long)AtofNonlocalized(scratchpad + 6);

	edfhdr->l_starttime *= EDFLIB_TIME_DIMENSION;

	/***************** NUMBER OF SIGNALS IN HEADER *******************************/

	strncpy(scratchpad, edfHdr + 252, 4);
	scratchpad[4] = 0;
	for (i = 0; i < 4; ++i)
	{
		if ((scratchpad[i] < 32) || (scratchpad[i] > 126))
		{
			*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
			free(edfHdr);
			free(edfhdr);
			return NULL;
		}
	}

	if (IsIntegerNumber(scratchpad))
	{
		*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
		free(edfHdr);
		free(edfhdr);
		return NULL;
	}
	edfhdr->edfsignals = (int)AtofNonlocalized(scratchpad);
	if (edfhdr->edfsignals < 1)
	{
		*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
		free(edfHdr);
		free(edfhdr);
		return NULL;
	}

	if (edfhdr->edfsignals > EDFLIB_MAXSIGNALS)
	{
		*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
		free(edfHdr);
		free(edfhdr);
		return NULL;
	}

	/***************** NUMBER OF BYTES IN HEADER *******************************/

	strncpy(scratchpad, edfHdr + 184, 8);
	scratchpad[8] = 0;

	for (i = 0; i < 8; ++i)
	{
		if ((scratchpad[i] < 32) || (scratchpad[i] > 126))
		{
			*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
			free(edfHdr);
			free(edfhdr);
			return NULL;
		}
	}

	if (IsIntegerNumber(scratchpad))
	{
		*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
		free(edfHdr);
		free(edfhdr);
		return NULL;
	}

	n = (int)AtofNonlocalized(scratchpad);
	if ((edfhdr->edfsignals * 256 + 256) != n)
	{
		*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
		free(edfHdr);
		free(edfhdr);
		return NULL;
	}

	/********************* RESERVED FIELD *************************************/

	edfhdr->edfplus       = 0;
	edfhdr->discontinuous = 0;
	strncpy(scratchpad, edfHdr + 192, 44);
	scratchpad[44] = 0;

	for (i = 0; i < 44; ++i)
	{
		if ((scratchpad[i] < 32) || (scratchpad[i] > 126))
		{
			*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
			free(edfHdr);
			free(edfhdr);
			return NULL;
		}
	}

	if (edfhdr->edf)
	{
		if (!strncmp(scratchpad, "EDF+C", 5)) { edfhdr->edfplus = 1; }

		if (!strncmp(scratchpad, "EDF+D", 5))
		{
			edfhdr->edfplus       = 1;
			edfhdr->discontinuous = 1;
		}
	}

	if (edfhdr->bdf)
	{
		if (!strncmp(scratchpad, "BDF+C", 5)) { edfhdr->bdfplus = 1; }

		if (!strncmp(scratchpad, "BDF+D", 5))
		{
			edfhdr->bdfplus       = 1;
			edfhdr->discontinuous = 1;
		}
	}

	strncpy(edfhdr->reserved, edfHdr + 192, 44);
	edfhdr->reserved[44] = 0;

	/********************* NUMBER OF DATARECORDS *************************************/

	strncpy(scratchpad, edfHdr + 236, 8);
	scratchpad[8] = 0;

	for (i = 0; i < 8; ++i)
	{
		if ((scratchpad[i] < 32) || (scratchpad[i] > 126))
		{
			*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
			free(edfHdr);
			free(edfhdr);
			return NULL;
		}
	}

	if (IsIntegerNumber(scratchpad))
	{
		*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
		free(edfHdr);
		free(edfhdr);
		return NULL;
	}

	edfhdr->datarecords = (long long)AtofNonlocalized(scratchpad);
	if (edfhdr->datarecords < 1)
	{
		*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
		free(edfHdr);
		free(edfhdr);
		return NULL;
	}

	/********************* DATARECORD DURATION *************************************/

	strncpy(scratchpad, edfHdr + 244, 8);
	scratchpad[8] = 0;

	for (i = 0; i < 8; ++i)
	{
		if ((scratchpad[i] < 32) || (scratchpad[i] > 126))
		{
			*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
			free(edfHdr);
			free(edfhdr);
			return NULL;
		}
	}

	if (IsNumber(scratchpad))
	{
		*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
		free(edfHdr);
		free(edfhdr);
		return NULL;
	}

	edfhdr->data_record_duration = AtofNonlocalized(scratchpad);
	if (edfhdr->data_record_duration < -0.000001)
	{
		*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
		free(edfHdr);
		free(edfhdr);
		return NULL;
	}

	edfhdr->long_data_record_duration = GetLongDuration(scratchpad);

	free(edfHdr);

	/********************* START WITH THE SIGNALS IN THE HEADER *********************/

	edfHdr = (char*)calloc(1, (edfhdr->edfsignals + 1) * 256);
	if (edfHdr == NULL)
	{
		*edfError = EDFLIB_MALLOC_ERROR;
		free(edfhdr);
		return NULL;
	}

	rewind(inputfile);
	if (fread(edfHdr, (edfhdr->edfsignals + 1) * 256, 1, inputfile) != 1)
	{
		*edfError = EDFLIB_FILE_READ_ERROR;
		free(edfHdr);
		free(edfhdr);
		return NULL;
	}

	edfhdr->edfparam = (struct edfparamblock*)calloc(1, sizeof(struct edfparamblock) * edfhdr->edfsignals);
	if (edfhdr->edfparam == NULL)
	{
		*edfError = EDFLIB_MALLOC_ERROR;
		free(edfHdr);
		free(edfhdr);
		return NULL;
	}

	/**************************** LABELS *************************************/

	edfhdr->nr_annot_chns = 0;
	for (i = 0; i < edfhdr->edfsignals; ++i)
	{
		strncpy(scratchpad, edfHdr + 256 + (i * 16), 16);
		for (j = 0; j < 16; ++j)
		{
			if ((scratchpad[j] < 32) || (scratchpad[j] > 126))
			{
				*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
				free(edfHdr);
				free(edfhdr->edfparam);
				free(edfhdr);
				return NULL;
			}
		}
		if (edfhdr->edfplus)
		{
			if (!strncmp(scratchpad, "EDF Annotations ", 16))
			{
				edfhdr->annot_ch[edfhdr->nr_annot_chns] = i;
				edfhdr->nr_annot_chns++;
				edfhdr->edfparam[i].annotation = 1;
			}
		}
		if (edfhdr->bdfplus)
		{
			if (!strncmp(scratchpad, "BDF Annotations ", 16))
			{
				edfhdr->annot_ch[edfhdr->nr_annot_chns] = i;
				edfhdr->nr_annot_chns++;
				edfhdr->edfparam[i].annotation = 1;
			}
		}
		strncpy(edfhdr->edfparam[i].label, edfHdr + 256 + (i * 16), 16);
		edfhdr->edfparam[i].label[16] = 0;
	}
	if (edfhdr->edfplus && (!edfhdr->nr_annot_chns))
	{
		*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
		free(edfHdr);
		free(edfhdr->edfparam);
		free(edfhdr);
		return NULL;
	}
	if (edfhdr->bdfplus && (!edfhdr->nr_annot_chns))
	{
		*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
		free(edfHdr);
		free(edfhdr->edfparam);
		free(edfhdr);
		return NULL;
	}
	if ((edfhdr->edfsignals != edfhdr->nr_annot_chns) || ((!edfhdr->edfplus) && (!edfhdr->bdfplus)))
	{
		if (edfhdr->data_record_duration < 0.0000001)
		{
			*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
			free(edfHdr);
			free(edfhdr->edfparam);
			free(edfhdr);
			return NULL;
		}
	}

	/**************************** TRANSDUCER TYPES *************************************/

	for (i = 0; i < edfhdr->edfsignals; ++i)
	{
		strncpy(scratchpad, edfHdr + 256 + (edfhdr->edfsignals * 16) + (i * 80), 80);
		for (j = 0; j < 80; ++j)
		{
			if ((scratchpad[j] < 32) || (scratchpad[j] > 126))
			{
				*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
				free(edfHdr);
				free(edfhdr->edfparam);
				free(edfhdr);
				return NULL;
			}
		}
		strncpy(edfhdr->edfparam[i].transducer, edfHdr + 256 + (edfhdr->edfsignals * 16) + (i * 80), 80);
		edfhdr->edfparam[i].transducer[80] = 0;

		if ((edfhdr->edfplus) || (edfhdr->bdfplus))
		{
			if (edfhdr->edfparam[i].annotation)
			{
				for (j = 0; j < 80; ++j)
				{
					if (edfhdr->edfparam[i].transducer[j] != ' ')
					{
						*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
						free(edfHdr);
						free(edfhdr->edfparam);
						free(edfhdr);
						return NULL;
					}
				}
			}
		}
	}

	/**************************** PHYSICAL DIMENSIONS *************************************/

	for (i = 0; i < edfhdr->edfsignals; ++i)
	{
		strncpy(scratchpad, edfHdr + 256 + (edfhdr->edfsignals * 96) + (i * 8), 8);
		for (j = 0; j < 8; ++j)
		{
			if ((scratchpad[j] < 32) || (scratchpad[j] > 126))
			{
				*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
				free(edfHdr);
				free(edfhdr->edfparam);
				free(edfhdr);
				return NULL;
			}
		}
		strncpy(edfhdr->edfparam[i].physdimension, edfHdr + 256 + (edfhdr->edfsignals * 96) + (i * 8), 8);
		edfhdr->edfparam[i].physdimension[8] = 0;
	}

	/**************************** PHYSICAL MINIMUMS *************************************/

	for (i = 0; i < edfhdr->edfsignals; ++i)
	{
		strncpy(scratchpad, edfHdr + 256 + (edfhdr->edfsignals * 104) + (i * 8), 8);
		scratchpad[8] = 0;

		for (j = 0; j < 8; ++j)
		{
			if ((scratchpad[j] < 32) || (scratchpad[j] > 126))
			{
				*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
				free(edfHdr);
				free(edfhdr->edfparam);
				free(edfhdr);
				return NULL;
			}
		}

		if (IsNumber(scratchpad))
		{
			*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
			free(edfHdr);
			free(edfhdr->edfparam);
			free(edfhdr);
			return NULL;
		}

		edfhdr->edfparam[i].phys_min = AtofNonlocalized(scratchpad);
	}

	/**************************** PHYSICAL MAXIMUMS *************************************/

	for (i = 0; i < edfhdr->edfsignals; ++i)
	{
		strncpy(scratchpad, edfHdr + 256 + (edfhdr->edfsignals * 112) + (i * 8), 8);
		scratchpad[8] = 0;

		for (j = 0; j < 8; ++j)
		{
			if ((scratchpad[j] < 32) || (scratchpad[j] > 126))
			{
				*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
				free(edfHdr);
				free(edfhdr->edfparam);
				free(edfhdr);
				return NULL;
			}
		}

		if (IsNumber(scratchpad))
		{
			*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
			free(edfHdr);
			free(edfhdr->edfparam);
			free(edfhdr);
			return NULL;
		}

		edfhdr->edfparam[i].phys_max = AtofNonlocalized(scratchpad);
		if (edfhdr->edfparam[i].phys_max == edfhdr->edfparam[i].phys_min)
		{
			*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
			free(edfHdr);
			free(edfhdr->edfparam);
			free(edfhdr);
			return NULL;
		}
	}

	/**************************** DIGITAL MINIMUMS *************************************/

	for (i = 0; i < edfhdr->edfsignals; ++i)
	{
		strncpy(scratchpad, edfHdr + 256 + (edfhdr->edfsignals * 120) + (i * 8), 8);
		scratchpad[8] = 0;

		for (j = 0; j < 8; ++j)
		{
			if ((scratchpad[j] < 32) || (scratchpad[j] > 126))
			{
				*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
				free(edfHdr);
				free(edfhdr->edfparam);
				free(edfhdr);
				return NULL;
			}
		}

		if (IsIntegerNumber(scratchpad))
		{
			*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
			free(edfHdr);
			free(edfhdr->edfparam);
			free(edfhdr);
			return NULL;
		}

		n = (int)AtofNonlocalized(scratchpad);
		if (edfhdr->edfplus)
		{
			if (edfhdr->edfparam[i].annotation)
			{
				if (n != -32768)
				{
					*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
					free(edfHdr);
					free(edfhdr->edfparam);
					free(edfhdr);
					return NULL;
				}
			}
		}
		if (edfhdr->bdfplus)
		{
			if (edfhdr->edfparam[i].annotation)
			{
				if (n != -8388608)
				{
					*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
					free(edfHdr);
					free(edfhdr->edfparam);
					free(edfhdr);
					return NULL;
				}
			}
		}
		if (edfhdr->edf)
		{
			if ((n > 32767) || (n < -32768))
			{
				*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
				free(edfHdr);
				free(edfhdr->edfparam);
				free(edfhdr);
				return NULL;
			}
		}
		if (edfhdr->bdf)
		{
			if ((n > 8388607) || (n < -8388608))
			{
				*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
				free(edfHdr);
				free(edfhdr->edfparam);
				free(edfhdr);
				return NULL;
			}
		}
		edfhdr->edfparam[i].dig_min = n;
	}

	/**************************** DIGITAL MAXIMUMS *************************************/

	for (i = 0; i < edfhdr->edfsignals; ++i)
	{
		strncpy(scratchpad, edfHdr + 256 + (edfhdr->edfsignals * 128) + (i * 8), 8);
		scratchpad[8] = 0;

		for (j = 0; j < 8; ++j)
		{
			if ((scratchpad[j] < 32) || (scratchpad[j] > 126))
			{
				*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
				free(edfHdr);
				free(edfhdr->edfparam);
				free(edfhdr);
				return NULL;
			}
		}

		if (IsIntegerNumber(scratchpad))
		{
			*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
			free(edfHdr);
			free(edfhdr->edfparam);
			free(edfhdr);
			return NULL;
		}

		n = (int)AtofNonlocalized(scratchpad);
		if (edfhdr->edfplus)
		{
			if (edfhdr->edfparam[i].annotation)
			{
				if (n != 32767)
				{
					*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
					free(edfHdr);
					free(edfhdr->edfparam);
					free(edfhdr);
					return NULL;
				}
			}
		}
		if (edfhdr->bdfplus)
		{
			if (edfhdr->edfparam[i].annotation)
			{
				if (n != 8388607)
				{
					*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
					free(edfHdr);
					free(edfhdr->edfparam);
					free(edfhdr);
					return NULL;
				}
			}
		}
		if (edfhdr->edf)
		{
			if ((n > 32767) || (n < -32768))
			{
				*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
				free(edfHdr);
				free(edfhdr->edfparam);
				free(edfhdr);
				return NULL;
			}
		}
		else
		{
			if ((n > 8388607) || (n < -8388608))
			{
				*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
				free(edfHdr);
				free(edfhdr->edfparam);
				free(edfhdr);
				return NULL;
			}
		}
		edfhdr->edfparam[i].dig_max = n;
		if (edfhdr->edfparam[i].dig_max < (edfhdr->edfparam[i].dig_min + 1))
		{
			*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
			free(edfHdr);
			free(edfhdr->edfparam);
			free(edfhdr);
			return NULL;
		}
	}

	/**************************** PREFILTER FIELDS *************************************/

	for (i = 0; i < edfhdr->edfsignals; ++i)
	{
		strncpy(scratchpad, edfHdr + 256 + (edfhdr->edfsignals * 136) + (i * 80), 80);
		for (j = 0; j < 80; ++j)
		{
			if ((scratchpad[j] < 32) || (scratchpad[j] > 126))
			{
				*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
				free(edfHdr);
				free(edfhdr->edfparam);
				free(edfhdr);
				return NULL;
			}
		}
		strncpy(edfhdr->edfparam[i].prefilter, edfHdr + 256 + (edfhdr->edfsignals * 136) + (i * 80), 80);
		edfhdr->edfparam[i].prefilter[80] = 0;

		if ((edfhdr->edfplus) || (edfhdr->bdfplus))
		{
			if (edfhdr->edfparam[i].annotation)
			{
				for (j = 0; j < 80; ++j)
				{
					if (edfhdr->edfparam[i].prefilter[j] != ' ')
					{
						*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
						free(edfHdr);
						free(edfhdr->edfparam);
						free(edfhdr);
						return NULL;
					}
				}
			}
		}
	}

	/*********************** NR OF SAMPLES IN EACH DATARECORD ********************/

	edfhdr->recordsize = 0;

	for (i = 0; i < edfhdr->edfsignals; ++i)
	{
		strncpy(scratchpad, edfHdr + 256 + (edfhdr->edfsignals * 216) + (i * 8), 8);
		scratchpad[8] = 0;

		for (j = 0; j < 8; ++j)
		{
			if ((scratchpad[j] < 32) || (scratchpad[j] > 126))
			{
				*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
				free(edfHdr);
				free(edfhdr->edfparam);
				free(edfhdr);
				return NULL;
			}
		}

		if (IsIntegerNumber(scratchpad))
		{
			*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
			free(edfHdr);
			free(edfhdr->edfparam);
			free(edfhdr);
			return NULL;
		}

		n = (int)AtofNonlocalized(scratchpad);
		if (n < 1)
		{
			*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
			free(edfHdr);
			free(edfhdr->edfparam);
			free(edfhdr);
			return NULL;
		}
		edfhdr->edfparam[i].smp_per_record = n;
		edfhdr->recordsize += n;
	}

	if (edfhdr->bdf)
	{
		edfhdr->recordsize *= 3;

		if (edfhdr->recordsize > 15728640)
		{
			*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
			free(edfHdr);
			free(edfhdr->edfparam);
			free(edfhdr);
			return NULL;
		}
	}
	else
	{
		edfhdr->recordsize *= 2;

		if (edfhdr->recordsize > 10485760)
		{
			*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
			free(edfHdr);
			free(edfhdr->edfparam);
			free(edfhdr);
			return NULL;
		}
	}

	/**************************** RESERVED FIELDS *************************************/

	for (i = 0; i < edfhdr->edfsignals; ++i)
	{
		strncpy(scratchpad, edfHdr + 256 + (edfhdr->edfsignals * 224) + (i * 32), 32);
		for (j = 0; j < 32; ++j)
		{
			if ((scratchpad[j] < 32) || (scratchpad[j] > 126))
			{
				*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
				free(edfHdr);
				free(edfhdr->edfparam);
				free(edfhdr);
				return NULL;
			}
		}
		strncpy(edfhdr->edfparam[i].reserved, edfHdr + 256 + (edfhdr->edfsignals * 224) + (i * 32), 32);
		edfhdr->edfparam[i].reserved[32] = 0;
	}

	/********************* EDF+ PATIENTNAME *********************************************/

	if (edfhdr->edfplus || edfhdr->bdfplus)
	{
		error       = 0;
		dotposition = 0;
		strncpy(scratchpad, edfHdr + 8, 80);
		scratchpad[80] = 0;
		for (i = 0; i < 80; ++i)
		{
			if (scratchpad[i] == ' ')
			{
				dotposition = i;
				break;
			}
		}
		dotposition++;
		if ((dotposition > 73) || (dotposition < 2)) { error = 1; }
		if (scratchpad[dotposition + 2] != 'X') { if (dotposition > 65) { error = 1; } }
		if ((scratchpad[dotposition] != 'M') && (scratchpad[dotposition] != 'F') && (scratchpad[dotposition] != 'X')) { error = 1; }
		dotposition++;
		if (scratchpad[dotposition] != ' ') { error = 1; }
		if (scratchpad[dotposition + 1] == 'X')
		{
			if (scratchpad[dotposition + 2] != ' ') { error = 1; }
			if (scratchpad[dotposition + 3] == ' ') { error = 1; }
		}
		else
		{
			if (scratchpad[dotposition + 12] != ' ') { error = 1; }
			if (scratchpad[dotposition + 13] == ' ') { error = 1; }
			dotposition++;
			strncpy(scratchpad2, scratchpad + dotposition, 11);
			scratchpad2[11] = 0;
			if ((scratchpad2[2] != '-') || (scratchpad2[6] != '-')) { error = 1; }
			scratchpad2[2] = 0;
			scratchpad2[6] = 0;
			if ((scratchpad2[0] < 48) || (scratchpad2[0] > 57)) { error = 1; }
			if ((scratchpad2[1] < 48) || (scratchpad2[1] > 57)) { error = 1; }
			if ((scratchpad2[7] < 48) || (scratchpad2[7] > 57)) { error = 1; }
			if ((scratchpad2[8] < 48) || (scratchpad2[8] > 57)) { error = 1; }
			if ((scratchpad2[9] < 48) || (scratchpad2[9] > 57)) { error = 1; }
			if ((scratchpad2[10] < 48) || (scratchpad2[10] > 57)) { error = 1; }
			if ((AtofNonlocalized(scratchpad2) < 1) || (AtofNonlocalized(scratchpad2) > 31)) { error = 1; }
			if (strcmp(scratchpad2 + 3, "JAN") != 0 && strcmp(scratchpad2 + 3, "FEB") != 0 && strcmp(scratchpad2 + 3, "MAR") != 0
				&& strcmp(scratchpad2 + 3, "APR") != 0 && strcmp(scratchpad2 + 3, "MAY") != 0 && strcmp(scratchpad2 + 3, "JUN") != 0
				&& strcmp(scratchpad2 + 3, "JUL") != 0 && strcmp(scratchpad2 + 3, "AUG") != 0 && strcmp(scratchpad2 + 3, "SEP") != 0
				&& strcmp(scratchpad2 + 3, "OCT") != 0 && strcmp(scratchpad2 + 3, "NOV") != 0 && strcmp(scratchpad2 + 3, "DEC") != 0) { error = 1; }
		}

		if (error)
		{
			*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
			free(edfHdr);
			free(edfhdr->edfparam);
			free(edfhdr);
			return NULL;
		}

		p = 0;
		if (edfhdr->patient[p] == 'X')
		{
			edfhdr->plus_patientcode[0] = 0;
			p += 2;
		}
		else
		{
			for (i = 0; i < (80 - p); ++i)
			{
				if (edfhdr->patient[i + p] == ' ') { break; }
				edfhdr->plus_patientcode[i] = edfhdr->patient[i + p];
				if (edfhdr->plus_patientcode[i] == '_') { edfhdr->plus_patientcode[i] = ' '; }
			}
			edfhdr->plus_patientcode[i] = 0;
			p += i + 1;
		}

		if (edfhdr->patient[p] == 'M') { strcpy(edfhdr->plus_gender, "Male"); }
		if (edfhdr->patient[p] == 'F') { strcpy(edfhdr->plus_gender, "Female"); }
		if (edfhdr->patient[p] == 'X') { edfhdr->plus_gender[0] = 0; }
		for (i = 0; i < (80 - p); ++i) { if (edfhdr->patient[i + p] == ' ') { break; } }
		p += i + 1;

		if (edfhdr->patient[p] == 'X')
		{
			edfhdr->plus_birthdate[0] = 0;
			p += 2;
		}
		else
		{
			for (i = 0; i < (80 - p); ++i)
			{
				if (edfhdr->patient[i + p] == ' ') { break; }
				edfhdr->plus_birthdate[i] = edfhdr->patient[i + p];
			}
			edfhdr->plus_birthdate[2] = ' ';
			edfhdr->plus_birthdate[3] += 32;
			edfhdr->plus_birthdate[4] += 32;
			edfhdr->plus_birthdate[5] += 32;
			edfhdr->plus_birthdate[6]  = ' ';
			edfhdr->plus_birthdate[11] = 0;
			p += i + 1;
		}

		for (i = 0; i < (80 - p); ++i)
		{
			if (edfhdr->patient[i + p] == ' ') { break; }
			edfhdr->plus_patient_name[i] = edfhdr->patient[i + p];
			if (edfhdr->plus_patient_name[i] == '_') { edfhdr->plus_patient_name[i] = ' '; }
		}
		edfhdr->plus_patient_name[i] = 0;
		p += i + 1;

		for (i = 0; i < (80 - p); ++i) { edfhdr->plus_patient_additional[i] = edfhdr->patient[i + p]; }
		edfhdr->plus_patient_additional[i] = 0;
		//p += i + 1;
	}

	/********************* EDF+ RECORDINGFIELD *********************************************/

	if (edfhdr->edfplus || edfhdr->bdfplus)
	{
		error = 0;
		strncpy(scratchpad, edfHdr + 88, 80);
		scratchpad[80] = 0;
		if (strncmp(scratchpad, "Startdate ", 10) != 0) { error = 1; }
		if (scratchpad[10] == 'X')
		{
			if (scratchpad[11] != ' ') { error = 1; }
			if (scratchpad[12] == ' ') { error = 1; }
			p = 12;
		}
		else
		{
			if (scratchpad[21] != ' ') { error = 1; }
			if (scratchpad[22] == ' ') { error = 1; }
			p = 22;
			strncpy(scratchpad2, scratchpad + 10, 11);
			scratchpad2[11] = 0;
			if ((scratchpad2[2] != '-') || (scratchpad2[6] != '-')) { error = 1; }
			scratchpad2[2] = 0;
			scratchpad2[6] = 0;
			if ((scratchpad2[0] < 48) || (scratchpad2[0] > 57)) { error = 1; }
			if ((scratchpad2[1] < 48) || (scratchpad2[1] > 57)) { error = 1; }
			if ((scratchpad2[7] < 48) || (scratchpad2[7] > 57)) { error = 1; }
			if ((scratchpad2[8] < 48) || (scratchpad2[8] > 57)) { error = 1; }
			if ((scratchpad2[9] < 48) || (scratchpad2[9] > 57)) { error = 1; }
			if ((scratchpad2[10] < 48) || (scratchpad2[10] > 57)) { error = 1; }
			if ((AtofNonlocalized(scratchpad2) < 1) || (AtofNonlocalized(scratchpad2) > 31)) { error = 1; }
			r = 0;
			if (!strcmp(scratchpad2 + 3, "JAN")) { r = 1; }
			else if (!strcmp(scratchpad2 + 3, "FEB")) { r = 2; }
			else if (!strcmp(scratchpad2 + 3, "MAR")) { r = 3; }
			else if (!strcmp(scratchpad2 + 3, "APR")) { r = 4; }
			else if (!strcmp(scratchpad2 + 3, "MAY")) { r = 5; }
			else if (!strcmp(scratchpad2 + 3, "JUN")) { r = 6; }
			else if (!strcmp(scratchpad2 + 3, "JUL")) { r = 7; }
			else if (!strcmp(scratchpad2 + 3, "AUG")) { r = 8; }
			else if (!strcmp(scratchpad2 + 3, "SEP")) { r = 9; }
			else if (!strcmp(scratchpad2 + 3, "OCT")) { r = 10; }
			else if (!strcmp(scratchpad2 + 3, "NOV")) { r = 11; }
			else if (!strcmp(scratchpad2 + 3, "DEC")) { r = 12; }
			else { error = 1; }
		}

		n = 0;
		for (i = p; i < 80; ++i)
		{
			if (i > 78)
			{
				error = 1;
				break;
			}
			if (scratchpad[i] == ' ')
			{
				n++;
				if (scratchpad[i + 1] == ' ')
				{
					error = 1;
					break;
				}
			}
			if (n > 1) { break; }
		}

		if (error)
		{
			*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
			free(edfHdr);
			free(edfhdr->edfparam);
			free(edfhdr);
			return NULL;
		}

		if (edfHdr[98] != 'X')
		{
			error = 0;

			strncpy(scratchpad, edfHdr + 168, 8);
			scratchpad[2] = 0;
			scratchpad[5] = 0;
			scratchpad[8] = 0;

			if (AtofNonlocalized(scratchpad) != AtofNonlocalized(scratchpad2)) { error = 1; }
			if (AtofNonlocalized(scratchpad + 3) != r) { error = 1; }
			if (AtofNonlocalized(scratchpad + 6) != AtofNonlocalized(scratchpad2 + 9)) { error = 1; }
			if (error)
			{
				*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
				free(edfHdr);
				free(edfhdr->edfparam);
				free(edfhdr);
				return NULL;
			}

			edfhdr->startdate_year = (int)AtofNonlocalized(scratchpad2 + 7);

			if (edfhdr->startdate_year < 1970)
			{
				*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
				free(edfHdr);
				free(edfhdr->edfparam);
				free(edfhdr);
				return NULL;
			}
		}

		p = 10;
		for (i = 0; i < (80 - p); ++i)
		{
			if (edfhdr->recording[i + p] == ' ') { break; }
			edfhdr->plus_startdate[i] = edfhdr->recording[i + p];
		}
		edfhdr->plus_startdate[2] = ' ';
		edfhdr->plus_startdate[3] += 32;
		edfhdr->plus_startdate[4] += 32;
		edfhdr->plus_startdate[5] += 32;
		edfhdr->plus_startdate[6]  = ' ';
		edfhdr->plus_startdate[11] = 0;
		p += i + 1;

		if (edfhdr->recording[p] == 'X')
		{
			edfhdr->plus_admincode[0] = 0;
			p += 2;
		}
		else
		{
			for (i = 0; i < (80 - p); ++i)
			{
				if (edfhdr->recording[i + p] == ' ') { break; }
				edfhdr->plus_admincode[i] = edfhdr->recording[i + p];
				if (edfhdr->plus_admincode[i] == '_') { edfhdr->plus_admincode[i] = ' '; }
			}
			edfhdr->plus_admincode[i] = 0;
			p += i + 1;
		}

		if (edfhdr->recording[p] == 'X')
		{
			edfhdr->plus_technician[0] = 0;
			p += 2;
		}
		else
		{
			for (i = 0; i < (80 - p); ++i)
			{
				if (edfhdr->recording[i + p] == ' ') { break; }
				edfhdr->plus_technician[i] = edfhdr->recording[i + p];
				if (edfhdr->plus_technician[i] == '_') { edfhdr->plus_technician[i] = ' '; }
			}
			edfhdr->plus_technician[i] = 0;
			p += i + 1;
		}

		if (edfhdr->recording[p] == 'X')
		{
			edfhdr->plus_equipment[0] = 0;
			p += 2;
		}
		else
		{
			for (i = 0; i < (80 - p); ++i)
			{
				if (edfhdr->recording[i + p] == ' ') { break; }
				edfhdr->plus_equipment[i] = edfhdr->recording[i + p];
				if (edfhdr->plus_equipment[i] == '_') { edfhdr->plus_equipment[i] = ' '; }
			}
			edfhdr->plus_equipment[i] = 0;
			p += i + 1;
		}

		for (i = 0; i < (80 - p); ++i) { edfhdr->plus_recording_additional[i] = edfhdr->recording[i + p]; }
		edfhdr->plus_recording_additional[i] = 0;
		//p += i + 1;
	}

	/********************* FILESIZE *********************************************/

	edfhdr->hdrsize = edfhdr->edfsignals * 256 + 256;

	fseeko(inputfile, 0LL, SEEK_END);
	if (ftello(inputfile) != (edfhdr->recordsize * edfhdr->datarecords + edfhdr->hdrsize))
	{
		*edfError = EDFLIB_FILE_CONTAINS_FORMAT_ERRORS;
		free(edfHdr);
		free(edfhdr->edfparam);
		free(edfhdr);
		return NULL;
	}

	n = 0;

	for (i = 0; i < edfhdr->edfsignals; ++i)
	{
		edfhdr->edfparam[i].buf_offset = n;
		if (edfhdr->bdf) { n += edfhdr->edfparam[i].smp_per_record * 3; }
		else { n += edfhdr->edfparam[i].smp_per_record * 2; }

		edfhdr->edfparam[i].bitvalue = (edfhdr->edfparam[i].phys_max - edfhdr->edfparam[i].phys_min) / (
										   edfhdr->edfparam[i].dig_max - edfhdr->edfparam[i].dig_min);
		edfhdr->edfparam[i].offset = (int)(edfhdr->edfparam[i].phys_max / edfhdr->edfparam[i].bitvalue - edfhdr->edfparam[i].dig_max);
	}

	edfhdr->file_hdl = inputfile;

	free(edfHdr);

	return edfhdr;
}

int IsIntegerNumber(char* str)
{
	int i = 0, hasspace = 0, digit = 0;	//hassign = 0, 

	const size_t l = strlen(str);

	if (!l) { return 1; }

	if ((str[0] == '+') || (str[0] == '-'))
	{
		//hassign++;
		i++;
	}

	for (; i < l; ++i)
	{
		if (str[i] == ' ')
		{
			if (!digit) { return 1; }
			hasspace++;
		}
		else
		{
			if ((str[i] < 48) || (str[i] > 57)) { return 1; }
			if (hasspace) { return 1; }
			digit++;
		}
	}

	if (digit) { return 0; }
	return 1;
}

int IsNumber(char* str)
{
	int i       = 0, hasspace = 0, digit = 0, hasdot = 0, hasexp = 0;	//hassign = 0, 
	const int l = (int)strlen(str);
	if (!l) { return 1; }

	if ((str[0] == '+') || (str[0] == '-'))
	{
		//hassign++;
		i++;
	}

	for (; i < l; ++i)
	{
		if ((str[i] == 'e') || (str[i] == 'E'))
		{
			if ((!digit) || hasexp) { return 1; }
			hasexp++;
			//hassign = 0;
			digit = 0;

			break;
		}

		if (str[i] == ' ')
		{
			if (!digit) { return 1; }
			hasspace++;
		}
		else
		{
			if (((str[i] < 48) || (str[i] > 57)) && str[i] != '.') { return 1; }
			if (hasspace) { return 1; }
			if (str[i] == '.')
			{
				if (hasdot) { return 1; }
				hasdot++;
			}
			else { digit++; }
		}
	}

	if (hasexp)
	{
		if (++i == l) { return 1; }

		if ((str[i] == '+') || (str[i] == '-'))
		{
			//hassign++;
			i++;
		}

		for (; i < l; ++i)
		{
			if (str[i] == ' ')
			{
				if (!digit) { return 1; }
				hasspace++;
			}
			else
			{
				if ((str[i] < 48) || (str[i] > 57)) { return 1; }
				if (hasspace) { return 1; }
				digit++;
			}
		}
	}

	if (digit) { return 0; }
	return 1;
}

long long GetLongDuration(const char* str)
{
	int i, len = 8, hasdot = 0, dotposition = 0;

	long long value = 0, radix;

	for (i = 0; i < 8; ++i)
	{
		if (str[i] == ' ')
		{
			len = i;
			break;
		}
	}

	for (i = 0; i < len; ++i)
	{
		if (str[i] == '.')
		{
			hasdot      = 1;
			dotposition = i;
			break;
		}
	}

	if (hasdot)
	{
		radix = EDFLIB_TIME_DIMENSION;

		for (i = dotposition - 1; i >= 0; i--)
		{
			value += ((long long)(str[i] - 48)) * radix;
			radix *= 10;
		}

		radix = EDFLIB_TIME_DIMENSION / 10;

		for (i = dotposition + 1; i < len; ++i)
		{
			value += ((long long)(str[i] - 48)) * radix;
			radix /= 10;
		}
	}
	else
	{
		radix = EDFLIB_TIME_DIMENSION;

		for (i = len - 1; i >= 0; i--)
		{
			value += ((long long)(str[i] - 48)) * radix;
			radix *= 10;
		}
	}

	return value;
}

int edflib_version() { return EDFLIB_VERSION; }

int GetAnnotations(struct edfhdrblock* edfhdr, int hdl, int annotations)
{
	int i, j, k, p, r = 0, n, edfsignals, datarecords, recordsize, discontinuous, *annotCh,
		nrAnnotChns, max, onset, duration,
		durationStart, zero, maxTalLn, error, annotsInRecord, annotsInTal, samplesize = 2;

	char *scratchpad, *cnvBuf, *timeInTxt, *durationInTxt;

	long long dataRecordDuration, elapsedtime, timeTmp = 0;

	FILE* inputfile;

	struct edfparamblock* edfparam;
	struct edf_annotationblock *newAnnotation = NULL,
							   *tmpAnnotation;

	inputfile          = edfhdr->file_hdl;
	edfsignals         = edfhdr->edfsignals;
	recordsize         = edfhdr->recordsize;
	edfparam           = edfhdr->edfparam;
	nrAnnotChns        = edfhdr->nr_annot_chns;
	datarecords        = (int)edfhdr->datarecords;
	dataRecordDuration = edfhdr->long_data_record_duration;
	discontinuous      = edfhdr->discontinuous;
	annotCh            = edfhdr->annot_ch;

	if (edfhdr->edfplus) { samplesize = 2; }
	if (edfhdr->bdfplus) { samplesize = 3; }

	cnvBuf = (char*)calloc(1, recordsize);
	if (cnvBuf == NULL) { return 1; }

	maxTalLn = 0;

	for (i = 0; i < nrAnnotChns; ++i)
	{
		if (maxTalLn < edfparam[annotCh[i]].smp_per_record * samplesize) { maxTalLn = edfparam[annotCh[i]].smp_per_record * samplesize; }
	}

	if (maxTalLn < 128) { maxTalLn = 128; }

	scratchpad = (char*)calloc(1, maxTalLn + 3);
	if (scratchpad == NULL)
	{
		free(cnvBuf);
		return 1;
	}

	timeInTxt = (char*)calloc(1, maxTalLn + 3);
	if (timeInTxt == NULL)
	{
		free(cnvBuf);
		free(scratchpad);
		return 1;
	}

	durationInTxt = (char*)calloc(1, maxTalLn + 3);
	if (durationInTxt == NULL)
	{
		free(cnvBuf);
		free(scratchpad);
		free(timeInTxt);
		return 1;
	}

	if (fseeko(inputfile, (long long)((edfsignals + 1) * 256), SEEK_SET))
	{
		free(cnvBuf);
		free(scratchpad);
		free(timeInTxt);
		free(durationInTxt);
		return 2;
	}

	elapsedtime = 0;

	for (i = 0; i < datarecords; ++i)
	{
		if (fread(cnvBuf, recordsize, 1, inputfile) != 1)
		{
			free(cnvBuf);
			free(scratchpad);
			free(timeInTxt);
			free(durationInTxt);
			return 2;
		}

		/************** process annotationsignals (if any) **************/

		error = 0;

		for (r = 0; r < nrAnnotChns; ++r)
		{
			n              = 0;
			zero           = 0;
			onset          = 0;
			duration       = 0;
			durationStart  = 0;
			scratchpad[0]  = 0;
			annotsInTal    = 0;
			annotsInRecord = 0;

			p   = edfparam[annotCh[r]].buf_offset;
			max = edfparam[annotCh[r]].smp_per_record * samplesize;

			/************** process one annotation signal ****************/

			if (cnvBuf[p + max - 1] != 0)
			{
				error = 5;
				goto END;
			}

			if (!r)  /* if it's the first annotation signal, then check */
			{       /* the timekeeping annotation */
				error = 1;

				for (k = 0; k < (max - 2); ++k)
				{
					scratchpad[k] = cnvBuf[p + k];

					if (scratchpad[k] == 20)
					{
						if (cnvBuf[p + k + 1] != 20)
						{
							error = 6;
							goto END;
						}
						scratchpad[k] = 0;
						if (IsOnsetNumber(scratchpad))
						{
							error = 36;
							goto END;
						}
						timeTmp = GetLongTime(scratchpad);
						if (i)
						{
							if (discontinuous)
							{
								if ((timeTmp - elapsedtime) < dataRecordDuration)
								{
									error = 4;
									goto END;
								}
							}
							else
							{
								if ((timeTmp - elapsedtime) != dataRecordDuration)
								{
									error = 3;
									goto END;
								}
							}
						}
						else
						{
							if (timeTmp >= EDFLIB_TIME_DIMENSION)
							{
								error = 2;
								goto END;
							}
							edfhdr->starttime_offset = timeTmp;
						}
						elapsedtime = timeTmp;
						error       = 0;
						break;
					}
				}
			}

			for (k = 0; k < max; ++k)
			{
				scratchpad[n] = cnvBuf[p + k];

				if (!scratchpad[n])
				{
					if (!zero)
					{
						if (k)
						{
							if (cnvBuf[p + k - 1] != 20)
							{
								error = 33;
								goto END;
							}
						}
						n             = 0;
						onset         = 0;
						duration      = 0;
						durationStart = 0;
						scratchpad[0] = 0;
						annotsInTal   = 0;
					}
					zero++;
					continue;
				}
				if (zero > 1)
				{
					error = 34;
					goto END;
				}
				zero = 0;

				if ((scratchpad[n] == 20) || (scratchpad[n] == 21))
				{
					if (scratchpad[n] == 21)
					{
						if (duration || durationStart || onset || annotsInTal)
						{               /* it's not allowed to have multiple duration fields */
							error = 35;   /* in one TAL or to have a duration field which is   */
							goto END;     /* not immediately behind the onsetfield             */
						}
						durationStart = 1;
					}

					if ((scratchpad[n] == 20) && onset && (!durationStart))
					{
						if (r || annotsInRecord)
						{
							if (n >= 0)
							{
								newAnnotation = (struct edf_annotationblock*)calloc(1, sizeof(struct edf_annotationblock));
								if (newAnnotation == NULL)
								{
									free(cnvBuf);
									free(scratchpad);
									free(timeInTxt);
									free(durationInTxt);
									return 1;
								}

								newAnnotation->next_annotation = NULL;

								newAnnotation->annotation[0] = 0;

								if (duration) { strcpy(newAnnotation->duration, durationInTxt); }
								else { newAnnotation->duration[0] = 0; }

								for (j = 0; j < n; ++j)
								{
									if (j == EDFLIB_MAX_ANNOTATION_LEN) { break; }
									newAnnotation->annotation[j] = scratchpad[j];
								}
								newAnnotation->annotation[j] = 0;

								newAnnotation->onset = GetLongTime(timeInTxt);

								if (annotationslist[hdl] == NULL)
								{
									newAnnotation->former_annotation = NULL;
									annotationslist[hdl]             = newAnnotation;
								}
								else
								{
									tmpAnnotation = annotationslist[hdl];
									while (tmpAnnotation->next_annotation) { tmpAnnotation = tmpAnnotation->next_annotation; }

									newAnnotation->former_annotation = tmpAnnotation;
									tmpAnnotation->next_annotation   = newAnnotation;
								}

								if (annotations == EDFLIB_READ_ANNOTATIONS)
								{
									if (!(strncmp(newAnnotation->annotation, "Recording ends", 14))) { if (nrAnnotChns == 1) { goto END; } }
								}
							}
						}

						annotsInTal++;
						annotsInRecord++;
						n = 0;
						continue;
					}

					if (!onset)
					{
						scratchpad[n] = 0;
						if (IsOnsetNumber(scratchpad))
						{
							error = 36;
							goto END;
						}
						onset = 1;
						n     = 0;
						strcpy(timeInTxt, scratchpad);
						continue;
					}

					if (durationStart)
					{
						scratchpad[n] = 0;
						if (IsDurationNumber(scratchpad))
						{
							error = 37;
							goto END;
						}

						for (j = 0; j < n; ++j)
						{
							if (j == 15) { break; }
							durationInTxt[j] = scratchpad[j];
							if ((durationInTxt[j] < 32) || (durationInTxt[j] > 126)) { durationInTxt[j] = '.'; }
						}
						durationInTxt[j] = 0;

						duration      = 1;
						durationStart = 0;
						n             = 0;
						continue;
					}
				}

				n++;
			}

		END:

			/****************** end ************************/

			if (error)
			{
				free(cnvBuf);
				free(scratchpad);
				free(timeInTxt);
				free(durationInTxt);
				return 9;
			}
		}
	}

	free(cnvBuf);
	free(scratchpad);
	free(timeInTxt);
	free(durationInTxt);

	return 0;
}

int IsDurationNumber(char* str)
{
	int hasdot = 0;

	const size_t l = strlen(str);

	if (!l) { return 1; }

	if ((str[0] == '.') || (str[l - 1] == '.')) { return 1; }

	for (size_t i = 0; i < l; ++i)
	{
		if (str[i] == '.')
		{
			if (hasdot) { return 1; }
			hasdot++;
		}
		else { if ((str[i] < 48) || (str[i] > 57)) { return 1; } }
	}

	return 0;
}

int IsOnsetNumber(char* str)
{
	int hasdot = 0;

	const size_t l = strlen(str);

	if (l < 2) { return 1; }

	if ((str[0] != '+') && (str[0] != '-')) { return 1; }

	if ((str[1] == '.') || (str[l - 1] == '.')) { return 1; }

	for (size_t i = 1; i < l; ++i)
	{
		if (str[i] == '.')
		{
			if (hasdot) { return 1; }
			hasdot++;
		}
		else { if ((str[i] < 48) || (str[i] > 57)) { return 1; } }
	}

	return 0;
}

long long GetLongTime(char* str)
{
	size_t hasdot = 0, dotposition = 0;

	long long value = 0, radix;

	str = str + 1;

	size_t len = strlen(str);

	for (size_t i = 0; i < len; ++i)
	{
		if (str[i] == '.')
		{
			hasdot      = 1;
			dotposition = i;
			break;
		}
	}

	if (hasdot)
	{
		radix = EDFLIB_TIME_DIMENSION;

		for (size_t i = dotposition - 1; i >= 0; i--)
		{
			value += ((long long)(str[i] - 48)) * radix;
			radix *= 10;
		}

		radix = EDFLIB_TIME_DIMENSION / 10;

		for (size_t i = dotposition + 1; i < len; ++i)
		{
			value += ((long long)(str[i] - 48)) * radix;
			radix /= 10;
		}
	}
	else
	{
		radix = EDFLIB_TIME_DIMENSION;

		for (size_t i = len - 1; i >= 0; i--)
		{
			value += ((long long)(str[i] - 48)) * radix;
			radix *= 10;
		}
	}

	if (str[-1] == '-') { value = -value; }

	return value;
}

void Latin1ToASCII(char* str, const int len)
{
	for (int i = 0; i < len; ++i)
	{
		const int value = *((unsigned char*)(str + i));

		if ((value > 31) && (value < 127)) { continue; }

		switch (value)
		{
			case 128: str[i] = 'E';
				break;

			case 130: str[i] = ',';
				break;

			case 131: str[i] = 'F';
				break;

			case 132: str[i] = '\"';
				break;

			case 133: str[i] = '.';
				break;

			case 134: str[i] = '+';
				break;

			case 135: str[i] = '+';
				break;

			case 136: str[i] = '^';
				break;

			case 137: str[i] = 'm';
				break;

			case 138: str[i] = 'S';
				break;

			case 139: str[i] = '<';
				break;

			case 140: str[i] = 'E';
				break;

			case 142: str[i] = 'Z';
				break;

			case 145: str[i] = '`';
				break;

			case 146: str[i] = '\'';
				break;

			case 147: str[i] = '\"';
				break;

			case 148: str[i] = '\"';
				break;

			case 149: str[i] = '.';
				break;

			case 150: str[i] = '-';
				break;

			case 151: str[i] = '-';
				break;

			case 152: str[i] = '~';
				break;

			case 154: str[i] = 's';
				break;

			case 155: str[i] = '>';
				break;

			case 156: str[i] = 'e';
				break;

			case 158: str[i] = 'z';
				break;

			case 159: str[i] = 'Y';
				break;

			case 171: str[i] = '<';
				break;

			case 180: str[i] = '\'';
				break;

			case 181: str[i] = 'u';
				break;

			case 187: str[i] = '>';
				break;

			case 191: str[i] = '\?';
				break;

			case 192: str[i] = 'A';
				break;

			case 193: str[i] = 'A';
				break;

			case 194: str[i] = 'A';
				break;

			case 195: str[i] = 'A';
				break;

			case 196: str[i] = 'A';
				break;

			case 197: str[i] = 'A';
				break;

			case 198: str[i] = 'E';
				break;

			case 199: str[i] = 'C';
				break;

			case 200: str[i] = 'E';
				break;

			case 201: str[i] = 'E';
				break;

			case 202: str[i] = 'E';
				break;

			case 203: str[i] = 'E';
				break;

			case 204: str[i] = 'I';
				break;

			case 205: str[i] = 'I';
				break;

			case 206: str[i] = 'I';
				break;

			case 207: str[i] = 'I';
				break;

			case 208: str[i] = 'D';
				break;

			case 209: str[i] = 'N';
				break;

			case 210: str[i] = 'O';
				break;

			case 211: str[i] = 'O';
				break;

			case 212: str[i] = 'O';
				break;

			case 213: str[i] = 'O';
				break;

			case 214: str[i] = 'O';
				break;

			case 215: str[i] = 'x';
				break;

			case 216: str[i] = 'O';
				break;

			case 217: str[i] = 'U';
				break;

			case 218: str[i] = 'U';
				break;

			case 219: str[i] = 'U';
				break;

			case 220: str[i] = 'U';
				break;

			case 221: str[i] = 'Y';
				break;

			case 222: str[i] = 'I';
				break;

			case 223: str[i] = 's';
				break;

			case 224: str[i] = 'a';
				break;

			case 225: str[i] = 'a';
				break;

			case 226: str[i] = 'a';
				break;

			case 227: str[i] = 'a';
				break;

			case 228: str[i] = 'a';
				break;

			case 229: str[i] = 'a';
				break;

			case 230: str[i] = 'e';
				break;

			case 231: str[i] = 'c';
				break;

			case 232: str[i] = 'e';
				break;

			case 233: str[i] = 'e';
				break;

			case 234: str[i] = 'e';
				break;

			case 235: str[i] = 'e';
				break;

			case 236: str[i] = 'i';
				break;

			case 237: str[i] = 'i';
				break;

			case 238: str[i] = 'i';
				break;

			case 239: str[i] = 'i';
				break;

			case 240: str[i] = 'd';
				break;

			case 241: str[i] = 'n';
				break;

			case 242: str[i] = 'o';
				break;

			case 243: str[i] = 'o';
				break;

			case 244: str[i] = 'o';
				break;

			case 245: str[i] = 'o';
				break;

			case 246: str[i] = 'o';
				break;

			case 247: str[i] = '-';
				break;

			case 248: str[i] = '0';
				break;

			case 249: str[i] = 'u';
				break;

			case 250: str[i] = 'u';
				break;

			case 251: str[i] = 'u';
				break;

			case 252: str[i] = 'u';
				break;

			case 253: str[i] = 'y';
				break;

			case 254: str[i] = 't';
				break;

			case 255: str[i] = 'y';
				break;

			default: str[i] = ' ';
				break;
		}
	}
}

void Latin12UTF8(char* latin, const int len)
{
	int i, j = 0;
	unsigned char tmp[512];
	unsigned char* str = (unsigned char*)latin;

	for (i = 0; i < len; ++i)
	{
		if (str[i] == 0)
		{
			tmp[j] = 0;
			break;
		}

		tmp[j] = str[i];

		if (str[i] < 32) { tmp[j] = '.'; }
		if ((str[i] > 126) && (str[i] < 160)) { tmp[j] = '.'; }
		if (str[i] > 159)
		{
			if ((len - j) < 2) { tmp[j] = ' '; }
			else
			{
				tmp[j] = 192 + (str[i] >> 6);
				j++;
				tmp[j] = 128 + (str[i] & 63);
			}
		}

		j++;

		if (j >= len) { break; }
	}

	for (i = 0; i < len; ++i) { str[i] = tmp[i]; }
}

int EdfopenFileWriteonly(const char* path, const int filetype, const int nSignals)
{
	int i;

	if ((filetype != EDFLIB_FILETYPE_EDFPLUS) && (filetype != EDFLIB_FILETYPE_BDFPLUS)) { return EDFLIB_FILETYPE_ERROR; }

	if (files_open >= EDFLIB_MAXFILES) { return EDFLIB_MAXFILES_REACHED; }

	for (i = 0; i < EDFLIB_MAXFILES; ++i) { if (hdrlist[i] != NULL) { if (!(strcmp(path, hdrlist[i]->path))) { return EDFLIB_FILE_ALREADY_OPENED; } } }

	if (nSignals < 0) { return EDFLIB_NUMBER_OF_SIGNALS_INVALID; }
	if (nSignals > EDFLIB_MAXSIGNALS) { return EDFLIB_NUMBER_OF_SIGNALS_INVALID; }

	struct edfhdrblock* hdr = (struct edfhdrblock*)calloc(1, sizeof(struct edfhdrblock));
	if (hdr == NULL) { return EDFLIB_MALLOC_ERROR; }

	hdr->edfparam = (struct edfparamblock*)calloc(1, sizeof(struct edfparamblock) * nSignals);
	if (hdr->edfparam == NULL)
	{
		free(hdr);
		return EDFLIB_MALLOC_ERROR;
	}

	hdr->writemode = 1;

	hdr->edfsignals = nSignals;

	FILE* file = fopeno(path, "wb");
	if (file == NULL)
	{
		free(hdr->edfparam);
		free(hdr);
		return EDFLIB_NO_SUCH_FILE_OR_DIRECTORY;
	}

	hdr->file_hdl = file;

	int handle = -1;

	for (i = 0; i < EDFLIB_MAXFILES; ++i)
	{
		if (hdrlist[i] == NULL)
		{
			hdrlist[i] = hdr;
			handle     = i;
			break;
		}
	}

	if (handle < 0) { return EDFLIB_MAXFILES_REACHED; }

	write_annotationslist[handle] = NULL;

	strcpy(hdr->path, path);

	files_open++;

	if (filetype == EDFLIB_FILETYPE_EDFPLUS)
	{
		hdr->edf     = 1;
		hdr->edfplus = 1;
	}

	if (filetype == EDFLIB_FILETYPE_BDFPLUS)
	{
		hdr->bdf     = 1;
		hdr->bdfplus = 1;
	}

	hdr->long_data_record_duration = EDFLIB_TIME_DIMENSION;
	hdr->data_record_duration      = 1.0;

	return handle;
}

int EdfSetSampling(const int handle, const int edfsignal, const int sampling)
{
	if (handle < 0) { return -1; }
	if (handle >= EDFLIB_MAXFILES) { return -1; }
	if (hdrlist[handle] == NULL) { return -1; }
	if (!(hdrlist[handle]->writemode)) { return -1; }
	if (edfsignal < 0) { return -1; }
	if (edfsignal >= hdrlist[handle]->edfsignals) { return -1; }
	if (sampling < 1) { return -1; }
	if (hdrlist[handle]->datarecords) { return -1; }

	hdrlist[handle]->edfparam[edfsignal].smp_per_record = sampling;
	return 0;
}

int EdfSetDatarecordDuration(const int handle, const int duration)
{
	if (handle < 0) { return -1; }
	if (handle >= EDFLIB_MAXFILES) { return -1; }
	if (hdrlist[handle] == NULL) { return -1; }
	if (!(hdrlist[handle]->writemode)) { return -1; }
	if (hdrlist[handle]->datarecords) { return -1; }
	if ((duration < 2500) || (duration > 2000000)) { return -1; }

	hdrlist[handle]->long_data_record_duration = (long long)duration * 100LL;

	if (hdrlist[handle]->long_data_record_duration < (EDFLIB_TIME_DIMENSION * 10LL))
	{
		hdrlist[handle]->long_data_record_duration /= 10LL;
		hdrlist[handle]->long_data_record_duration *= 10LL;
	}
	else
	{
		hdrlist[handle]->long_data_record_duration /= 100LL;
		hdrlist[handle]->long_data_record_duration *= 100LL;
	}

	hdrlist[handle]->data_record_duration = ((double)(hdrlist[handle]->long_data_record_duration)) / EDFLIB_TIME_DIMENSION;

	return 0;
}

int EdfwriteDigitalSamples(const int handle, const int* buf)
{
	if (handle < 0) { return -1; }
	if (handle >= EDFLIB_MAXFILES) { return -1; }
	if (hdrlist[handle] == NULL) { return -1; }
	if (!(hdrlist[handle]->writemode)) { return -1; }
	if (hdrlist[handle]->edfsignals == 0) { return -1; }

	struct edfhdrblock* hdr = hdrlist[handle];

	FILE* file = hdr->file_hdl;

	const int edfsignal = hdr->signal_write_sequence_pos;

	if (!hdr->datarecords)
	{
		if (!edfsignal)
		{
			const int error = WriteEdfHeader(hdr);
			if (error) { return error; }
		}
	}

	const int sf     = hdr->edfparam[edfsignal].smp_per_record;
	const int digmax = hdr->edfparam[edfsignal].dig_max;
	const int digmin = hdr->edfparam[edfsignal].dig_min;

	for (int i = 0; i < sf; ++i)
	{
		int value = buf[i];

		if (value > digmax) { value = digmax; }
		if (value < digmin) { value = digmin; }

		fputc((value) & 0xff, file);

		if (fputc((value >> 8) & 0xff, file) == EOF) { return -1; }
		if (hdr->bdf) { fputc((value >> 16) & 0xff, file); }
	}

	hdr->signal_write_sequence_pos++;

	if (hdr->signal_write_sequence_pos == hdr->edfsignals)
	{
		hdr->signal_write_sequence_pos = 0;

		int p = FprintLLNumberNonlocalized(file, (hdr->datarecords * hdr->long_data_record_duration) / EDFLIB_TIME_DIMENSION, 0, 1);
		if (hdr->long_data_record_duration % EDFLIB_TIME_DIMENSION)
		{
			fputc('.', file);
			p++;
			p += FprintLLNumberNonlocalized(file, (hdr->datarecords * hdr->long_data_record_duration) % EDFLIB_TIME_DIMENSION, 7, 0);
		}
		fputc(20, file);
		fputc(20, file);
		p += 2;
		for (; p < EDFLIB_ANNOTATION_BYTES; ++p) { fputc(0, file); }

		hdr->datarecords++;

		fflush(file);
	}

	return 0;
}

int EdfBlockwriteDigitalSamples(const int handle, const int* buf)
{
	if (handle < 0) { return -1; }
	if (handle >= EDFLIB_MAXFILES) { return -1; }
	if (hdrlist[handle] == NULL) { return -1; }
	if (!(hdrlist[handle]->writemode)) { return -1; }
	if (hdrlist[handle]->signal_write_sequence_pos) { return -1; }
	if (hdrlist[handle]->edfsignals == 0) { return -1; }

	struct edfhdrblock* hdr = hdrlist[handle];

	FILE* file = hdr->file_hdl;

	const int edfsignals = hdr->edfsignals;

	if (!hdr->datarecords)
	{
		const int error = WriteEdfHeader(hdr);
		if (error) { return error; }
	}

	int offset = 0;

	for (int j = 0; j < edfsignals; ++j)
	{
		const int sf     = hdr->edfparam[j].smp_per_record;
		const int digmax = hdr->edfparam[j].dig_max;
		const int digmin = hdr->edfparam[j].dig_min;

		for (int i = 0; i < sf; ++i)
		{
			int value = buf[i + offset];
			if (value > digmax) { value = digmax; }
			if (value < digmin) { value = digmin; }
			fputc(value & 0xff, file);
			if (fputc((value >> 8) & 0xff, file) == EOF) { return -1; }
			if (hdr->bdf) { fputc((value >> 16) & 0xff, file); }
		}

		offset += sf;
	}

	int p = FprintLLNumberNonlocalized(file, (hdr->datarecords * hdr->long_data_record_duration) / EDFLIB_TIME_DIMENSION, 0, 1);
	if (hdr->long_data_record_duration % EDFLIB_TIME_DIMENSION)
	{
		fputc('.', file);
		p++;
		p += FprintLLNumberNonlocalized(file, (hdr->datarecords * hdr->long_data_record_duration) % EDFLIB_TIME_DIMENSION, 7, 0);
	}
	fputc(20, file);
	fputc(20, file);
	p += 2;
	for (; p < EDFLIB_ANNOTATION_BYTES; ++p) { fputc(0, file); }

	hdr->datarecords++;

	fflush(file);

	return 0;
}

int EdfwritePhysicalSamples(const int handle, const double* buf)
{
	if (handle < 0) { return -1; }
	if (handle >= EDFLIB_MAXFILES) { return -1; }
	if (hdrlist[handle] == NULL) { return -1; }
	if (!(hdrlist[handle]->writemode)) { return -1; }
	if (hdrlist[handle]->edfsignals == 0) { return -1; }

	struct edfhdrblock* hdr = hdrlist[handle];
	FILE* file              = hdr->file_hdl;
	const int edfsignal     = hdr->signal_write_sequence_pos;

	if (!hdr->datarecords)
	{
		if (!edfsignal)
		{
			const int error = WriteEdfHeader(hdr);
			if (error) { return error; }
		}
	}

	const int sf          = hdr->edfparam[edfsignal].smp_per_record;
	const int digmax      = hdr->edfparam[edfsignal].dig_max;
	const int digmin      = hdr->edfparam[edfsignal].dig_min;
	const double bitvalue = hdr->edfparam[edfsignal].bitvalue;
	const int offset      = hdr->edfparam[edfsignal].offset;

	for (int i = 0; i < sf; ++i)
	{
		int value = (int)(buf[i] / bitvalue);
		value -= offset;
		if (value > digmax) { value = digmax; }
		if (value < digmin) { value = digmin; }

		fputc(value & 0xff, file);

		if (fputc((value >> 8) & 0xff, file) == EOF) { return -1; }
		if (hdr->bdf) { fputc((value >> 16) & 0xff, file); }
	}

	hdr->signal_write_sequence_pos++;

	if (hdr->signal_write_sequence_pos == hdr->edfsignals)
	{
		hdr->signal_write_sequence_pos = 0;

		int p = FprintLLNumberNonlocalized(file, (hdr->datarecords * hdr->long_data_record_duration) / EDFLIB_TIME_DIMENSION, 0, 1);
		if (hdr->long_data_record_duration % EDFLIB_TIME_DIMENSION)
		{
			fputc('.', file);
			p++;
			p += FprintLLNumberNonlocalized(file, (hdr->datarecords * hdr->long_data_record_duration) % EDFLIB_TIME_DIMENSION, 7, 0);
		}
		fputc(20, file);
		fputc(20, file);
		p += 2;
		for (; p < EDFLIB_ANNOTATION_BYTES; ++p) { fputc(0, file); }

		hdr->datarecords++;

		fflush(file);
	}

	return 0;
}

int EdfBlockwritePhysicalSamples(const int handle, const double* buf)
{
	if (handle < 0) { return -1; }
	if (handle >= EDFLIB_MAXFILES) { return -1; }
	if (hdrlist[handle] == NULL) { return -1; }
	if (!(hdrlist[handle]->writemode)) { return -1; }
	if (hdrlist[handle]->signal_write_sequence_pos) { return -1; }
	if (hdrlist[handle]->edfsignals == 0) { return -1; }

	struct edfhdrblock* hdr = hdrlist[handle];
	FILE* file              = hdr->file_hdl;
	const int edfsignals    = hdr->edfsignals;

	if (!hdr->datarecords)
	{
		const int error = WriteEdfHeader(hdr);
		if (error) { return error; }
	}

	int bufOffset = 0;

	for (int j = 0; j < edfsignals; ++j)
	{
		const int sf          = hdr->edfparam[j].smp_per_record;
		const int digmax      = hdr->edfparam[j].dig_max;
		const int digmin      = hdr->edfparam[j].dig_min;
		const double bitvalue = hdr->edfparam[j].bitvalue;
		const int offset      = hdr->edfparam[j].offset;

		for (int i = 0; i < sf; ++i)
		{
			int value = (int)(buf[i + bufOffset] / bitvalue);
			value -= offset;
			if (value > digmax) { value = digmax; }
			if (value < digmin) { value = digmin; }

			fputc(value & 0xff, file);

			if (fputc((value >> 8) & 0xff, file) == EOF) { return -1; }
			if (hdr->bdf) { fputc((value >> 16) & 0xff, file); }
		}

		bufOffset += sf;
	}

	int p = FprintLLNumberNonlocalized(file, (hdr->datarecords * hdr->long_data_record_duration) / EDFLIB_TIME_DIMENSION, 0, 1);
	if (hdr->long_data_record_duration % EDFLIB_TIME_DIMENSION)
	{
		fputc('.', file);
		p++;
		p += FprintLLNumberNonlocalized(file, (hdr->datarecords * hdr->long_data_record_duration) % EDFLIB_TIME_DIMENSION, 7, 0);
	}
	fputc(20, file);
	fputc(20, file);
	p += 2;
	for (; p < EDFLIB_ANNOTATION_BYTES; ++p) { fputc(0, file); }

	hdr->datarecords++;

	fflush(file);

	return 0;
}

int WriteEdfHeader(struct edfhdrblock* hdr)
{
	int i, j, p, q, len, rest, edfsignals;

	char str[128];
	struct tm* dateTime;
	time_t elapsedTime;
	FILE* file;

	file = hdr->file_hdl;

	edfsignals = hdr->edfsignals;

	if (edfsignals < 0) { return -20; }
	if (edfsignals > EDFLIB_MAXSIGNALS) { return -21; }

	for (i = 0; i < edfsignals; ++i)
	{
		if (hdr->edfparam[i].smp_per_record < 1) { return -22; }
		if (hdr->edfparam[i].dig_max == hdr->edfparam[i].dig_min) { return -23; }
		if (hdr->edfparam[i].dig_max < hdr->edfparam[i].dig_min) { return -24; }
		if (hdr->edfparam[i].phys_max == hdr->edfparam[i].phys_min) { return -25; }
	}

	for (i = 0; i < edfsignals; ++i)
	{
		hdr->edfparam[i].bitvalue = (hdr->edfparam[i].phys_max - hdr->edfparam[i].phys_min) / (hdr->edfparam[i].dig_max - hdr->edfparam[i].dig_min);
		hdr->edfparam[i].offset   = (int)(hdr->edfparam[i].phys_max / hdr->edfparam[i].bitvalue - hdr->edfparam[i].dig_max);
	}

	rewind(file);

	if (hdr->edf) { fprintf(file, "0       "); }
	else
	{
		fputc(255, file);
		fprintf(file, "BIOSEMI");
	}

	p = 0;

	if (hdr->plus_birthdate[0] == 0) { rest = 72; }
	else { rest = 62; }

	len = (int)(strlen(hdr->plus_patientcode));
	if (len && rest)
	{
		if (len > rest)
		{
			len  = rest;
			rest = 0;
		}
		else { rest -= len; }
		strcpy(str, hdr->plus_patientcode);
		Latin1ToASCII(str, len);
		str[len] = 0;
		for (i = 0; i < len; ++i) { if (str[i] == ' ') { str[i] = '_'; } }
		p += fprintf(file, "%s ", str);
	}
	else { p += fprintf(file, "X "); }

	if (hdr->plus_gender[0] == 'M') { fputc('M', file); }
	else
	{
		if (hdr->plus_gender[0] == 'F') { fputc('F', file); }
		else { fputc('X', file); }
	}
	fputc(' ', file);
	p += 2;

	if (hdr->plus_birthdate[0] == 0)
	{
		fputc('X', file);
		fputc(' ', file);

		p += 2;
	}
	else
	{
		fputc(hdr->plus_birthdate[0], file);
		fputc(hdr->plus_birthdate[1], file);
		fputc('-', file);
		q = (int)AtofNonlocalized(&(hdr->plus_birthdate[3]));
		switch (q)
		{
			case 1: fprintf(file, "JAN");
				break;
			case 2: fprintf(file, "FEB");
				break;
			case 3: fprintf(file, "MAR");
				break;
			case 4: fprintf(file, "APR");
				break;
			case 5: fprintf(file, "MAY");
				break;
			case 6: fprintf(file, "JUN");
				break;
			case 7: fprintf(file, "JUL");
				break;
			case 8: fprintf(file, "AUG");
				break;
			case 9: fprintf(file, "SEP");
				break;
			case 10: fprintf(file, "OCT");
				break;
			case 11: fprintf(file, "NOV");
				break;
			case 12: fprintf(file, "DEC");
				break;
			default: fprintf(file, "UND");
				break;
		}
		fputc('-', file);
		fputc(hdr->plus_birthdate[6], file);
		fputc(hdr->plus_birthdate[7], file);
		fputc(hdr->plus_birthdate[8], file);
		fputc(hdr->plus_birthdate[9], file);
		fputc(' ', file);

		p += 12;
	}

	len = (int)strlen(hdr->plus_patient_name);
	if (len && rest)
	{
		if (len > rest)
		{
			len  = rest;
			rest = 0;
		}
		else { rest -= len; }
		strcpy(str, hdr->plus_patient_name);
		Latin1ToASCII(str, len);
		str[len] = 0;
		for (i = 0; i < len; ++i) { if (str[i] == ' ') { str[i] = '_'; } }
		p += fprintf(file, "%s ", str);
	}
	else
	{
		fputc('X', file);
		p++;
	}

	len = (int)strlen(hdr->plus_patient_additional);
	if (len && rest)
	{
		if (len > rest) { len = rest; }
		strcpy(str, hdr->plus_patient_additional);
		Latin1ToASCII(str, len);
		str[len] = 0;
		p += fprintf(file, "%s", str);
	}

	for (; p < 80; ++p) { fputc(' ', file); }

	if (!hdr->startdate_year)
	{
		elapsedTime = time(NULL);
		dateTime    = localtime(&elapsedTime);

		hdr->startdate_year   = dateTime->tm_year + 1900;
		hdr->startdate_month  = dateTime->tm_mon + 1;
		hdr->startdate_day    = dateTime->tm_mday;
		hdr->starttime_hour   = dateTime->tm_hour;
		hdr->starttime_minute = dateTime->tm_min;
		hdr->starttime_second = dateTime->tm_sec % 60;
	}

	p = 0;

	p += fprintf(file, "Startdate %02u-", hdr->startdate_day);
	switch (hdr->startdate_month)
	{
		case 1: fprintf(file, "JAN");
			break;
		case 2: fprintf(file, "FEB");
			break;
		case 3: fprintf(file, "MAR");
			break;
		case 4: fprintf(file, "APR");
			break;
		case 5: fprintf(file, "MAY");
			break;
		case 6: fprintf(file, "JUN");
			break;
		case 7: fprintf(file, "JUL");
			break;
		case 8: fprintf(file, "AUG");
			break;
		case 9: fprintf(file, "SEP");
			break;
		case 10: fprintf(file, "OCT");
			break;
		case 11: fprintf(file, "NOV");
			break;
		case 12: fprintf(file, "DEC");
			break;
		default: fprintf(file, "UND");
			break;
	}
	p += 3;
	fputc('-', file);
	p++;
	p += FprintIntNumberNonlocalized(file, hdr->startdate_year, 4, 0);
	fputc(' ', file);
	p++;

	rest = 42;

	len = (int)strlen(hdr->plus_admincode);
	if (len && rest)
	{
		if (len > rest)
		{
			len  = rest;
			rest = 0;
		}
		else { rest -= len; }
		strcpy(str, hdr->plus_admincode);
		Latin1ToASCII(str, len);
		str[len] = 0;
		for (i = 0; i < len; ++i) { if (str[i] == ' ') { str[i] = '_'; } }
		p += fprintf(file, "%s ", str);
	}
	else { p += fprintf(file, "X "); }

	len = (int)strlen(hdr->plus_technician);
	if (len && rest)
	{
		if (len > rest)
		{
			len  = rest;
			rest = 0;
		}
		else { rest -= len; }
		strcpy(str, hdr->plus_technician);
		Latin1ToASCII(str, len);
		str[len] = 0;
		for (i = 0; i < len; ++i) { if (str[i] == ' ') { str[i] = '_'; } }
		p += fprintf(file, "%s ", str);
	}
	else { p += fprintf(file, "X "); }

	len = (int)strlen(hdr->plus_equipment);
	if (len && rest)
	{
		if (len > rest)
		{
			len  = rest;
			rest = 0;
		}
		else { rest -= len; }
		strcpy(str, hdr->plus_equipment);
		Latin1ToASCII(str, len);
		str[len] = 0;
		for (i = 0; i < len; ++i) { if (str[i] == ' ') { str[i] = '_'; } }
		p += fprintf(file, "%s ", str);
	}
	else { p += fprintf(file, "X "); }

	len = (int)strlen(hdr->plus_recording_additional);
	if (len && rest)
	{
		if (len > rest) { len = rest; }
		strcpy(str, hdr->plus_recording_additional);
		Latin1ToASCII(str, len);
		str[len] = 0;
		p += fprintf(file, "%s", str);
	}

	for (; p < 80; ++p) { fputc(' ', file); }

	fprintf(file, "%02u.%02u.%02u", hdr->startdate_day, hdr->startdate_month, (hdr->startdate_year % 100));
	fprintf(file, "%02u.%02u.%02u", hdr->starttime_hour, hdr->starttime_minute, hdr->starttime_second);
	p = FprintIntNumberNonlocalized(file, edfsignals * 256 + 512, 0, 0);
	for (; p < 8; ++p) { fputc(' ', file); }
	if (hdr->edf) { fprintf(file, "EDF+C"); }
	else { fprintf(file, "BDF+C"); }
	for (i = 0; i < 39; ++i) { fputc(' ', file); }
	fprintf(file, "-1      ");
	if (hdr->long_data_record_duration == EDFLIB_TIME_DIMENSION) { fprintf(file, "1       "); }
	else
	{
		SprintNumberNonlocalized(str, hdr->data_record_duration);
		strcat(str, "        ");
		str[8] = 0;
		fprintf(file, "%s", str);
	}
	p = FprintIntNumberNonlocalized(file, edfsignals + 1, 0, 0);
	for (; p < 4; ++p) { fputc(' ', file); }

	for (i = 0; i < edfsignals; ++i)
	{
		len = (int)strlen(hdr->edfparam[i].label);
		Latin1ToASCII(hdr->edfparam[i].label, len);
		for (j = 0; j < len; ++j) { fputc(hdr->edfparam[i].label[j], file); }
		for (; j < 16; ++j) { fputc(' ', file); }
	}
	if (hdr->edf) { fprintf(file, "EDF Annotations "); }
	else { fprintf(file, "BDF Annotations "); }
	for (i = 0; i < edfsignals; ++i)
	{
		len = (int)strlen(hdr->edfparam[i].transducer);
		Latin1ToASCII(hdr->edfparam[i].transducer, len);
		for (j = 0; j < len; ++j) { fputc(hdr->edfparam[i].transducer[j], file); }
		for (; j < 80; ++j) { fputc(' ', file); }
	}
	for (i = 0; i < 80; ++i) { fputc(' ', file); }
	for (i = 0; i < edfsignals; ++i)
	{
		len = (int)strlen(hdr->edfparam[i].physdimension);
		Latin1ToASCII(hdr->edfparam[i].physdimension, len);
		for (j = 0; j < len; ++j) { fputc(hdr->edfparam[i].physdimension[j], file); }
		for (; j < 8; ++j) { fputc(' ', file); }
	}
	fprintf(file, "        ");
	for (i = 0; i < edfsignals; ++i)
	{
		p = SprintNumberNonlocalized(str, hdr->edfparam[i].phys_min);
		for (; p < 8; ++p) { str[p] = ' '; }
		str[8] = 0;
		fprintf(file, "%s", str);
	}
	fprintf(file, "-1      ");
	for (i = 0; i < edfsignals; ++i)
	{
		p = SprintNumberNonlocalized(str, hdr->edfparam[i].phys_max);
		for (; p < 8; ++p) { str[p] = ' '; }
		str[8] = 0;
		fprintf(file, "%s", str);
	}
	fprintf(file, "1       ");
	for (i = 0; i < edfsignals; ++i)
	{
		p = FprintIntNumberNonlocalized(file, hdr->edfparam[i].dig_min, 0, 0);
		for (; p < 8; ++p) { fputc(' ', file); }
	}
	if (hdr->edf) { fprintf(file, "-32768  "); }
	else { fprintf(file, "-8388608"); }
	for (i = 0; i < edfsignals; ++i)
	{
		p = FprintIntNumberNonlocalized(file, hdr->edfparam[i].dig_max, 0, 0);
		for (; p < 8; ++p) { fputc(' ', file); }
	}
	if (hdr->edf) { fprintf(file, "32767   "); }
	else { fprintf(file, "8388607 "); }
	for (i = 0; i < edfsignals; ++i)
	{
		len = (int)strlen(hdr->edfparam[i].prefilter);
		Latin1ToASCII(hdr->edfparam[i].prefilter, len);
		for (j = 0; j < len; ++j) { fputc(hdr->edfparam[i].prefilter[j], file); }
		for (; j < 80; ++j) { fputc(' ', file); }
	}
	for (j = 0; j < 80; ++j) { fputc(' ', file); }
	for (i = 0; i < edfsignals; ++i)
	{
		p = FprintIntNumberNonlocalized(file, hdr->edfparam[i].smp_per_record, 0, 0);
		for (; p < 8; ++p) { fputc(' ', file); }
	}
	if (hdr->edf)
	{
		p = FprintIntNumberNonlocalized(file, EDFLIB_ANNOTATION_BYTES / 2, 0, 0);
		for (; p < 8; ++p) { fputc(' ', file); }
	}
	else
	{
		p = FprintIntNumberNonlocalized(file, EDFLIB_ANNOTATION_BYTES / 3, 0, 0);
		for (; p < 8; ++p) { fputc(' ', file); }
	}
	for (i = 0; i < (edfsignals * 32 + 32); ++i) { fputc(' ', file); }

	return 0;
}

int EdfSetLabel(const int handle, const int edfsignal, const char* label)
{
	if (handle < 0) { return -1; }
	if (handle >= EDFLIB_MAXFILES) { return -1; }
	if (hdrlist[handle] == NULL) { return -1; }
	if (!(hdrlist[handle]->writemode)) { return -1; }
	if (edfsignal < 0) { return -1; }
	if (edfsignal >= hdrlist[handle]->edfsignals) { return -1; }
	if (hdrlist[handle]->datarecords) { return -1; }
	strncpy(hdrlist[handle]->edfparam[edfsignal].label, label, 16);
	hdrlist[handle]->edfparam[edfsignal].label[16] = 0;
	RemovePaddingTrailingSpaces(hdrlist[handle]->edfparam[edfsignal].label);
	return 0;
}

int EdfSetPhysicalDimension(const int handle, const int edfsignal, const char* dim)
{
	if (handle < 0) { return -1; }
	if (handle >= EDFLIB_MAXFILES) { return -1; }
	if (hdrlist[handle] == NULL) { return -1; }
	if (!(hdrlist[handle]->writemode)) { return -1; }
	if (edfsignal < 0) { return -1; }
	if (edfsignal >= hdrlist[handle]->edfsignals) { return -1; }
	if (hdrlist[handle]->datarecords) { return -1; }
	strncpy(hdrlist[handle]->edfparam[edfsignal].physdimension, dim, 8);
	hdrlist[handle]->edfparam[edfsignal].physdimension[8] = 0;
	RemovePaddingTrailingSpaces(hdrlist[handle]->edfparam[edfsignal].physdimension);
	return 0;
}

int EdfSetPhysicalMaximum(const int handle, const int edfsignal, const double max)
{
	if (handle < 0) { return -1; }
	if (handle >= EDFLIB_MAXFILES) { return -1; }
	if (hdrlist[handle] == NULL) { return -1; }
	if (!(hdrlist[handle]->writemode)) { return -1; }
	if (edfsignal < 0) { return -1; }
	if (edfsignal >= hdrlist[handle]->edfsignals) { return -1; }
	if (hdrlist[handle]->datarecords) { return -1; }
	hdrlist[handle]->edfparam[edfsignal].phys_max = max;
	return 0;
}

int EdfSetPhysicalMinimum(const int handle, const int edfsignal, const double min)
{
	if (handle < 0) { return -1; }
	if (handle >= EDFLIB_MAXFILES) { return -1; }
	if (hdrlist[handle] == NULL) { return -1; }
	if (!(hdrlist[handle]->writemode)) { return -1; }
	if (edfsignal < 0) { return -1; }
	if (edfsignal >= hdrlist[handle]->edfsignals) { return -1; }
	if (hdrlist[handle]->datarecords) { return -1; }
	hdrlist[handle]->edfparam[edfsignal].phys_min = min;
	return 0;
}

int EdfSetDigitalMaximum(const int handle, const int edfsignal, const int max)
{
	if (handle < 0) { return -1; }
	if (handle >= EDFLIB_MAXFILES) { return -1; }
	if (hdrlist[handle] == NULL) { return -1; }
	if (!(hdrlist[handle]->writemode)) { return -1; }
	if (edfsignal < 0) { return -1; }
	if (edfsignal >= hdrlist[handle]->edfsignals) { return -1; }
	if (hdrlist[handle]->edf) { if (max > 32767) { return -1; } }
	else { if (max > 8388607) { return -1; } }
	if (hdrlist[handle]->datarecords) { return -1; }
	hdrlist[handle]->edfparam[edfsignal].dig_max = max;
	return 0;
}

int EdfSetDigitalMinimum(const int handle, const int edfsignal, const int min)
{
	if (handle < 0) { return -1; }
	if (handle >= EDFLIB_MAXFILES) { return -1; }
	if (hdrlist[handle] == NULL) { return -1; }
	if (!(hdrlist[handle]->writemode)) { return -1; }
	if (edfsignal < 0) { return -1; }
	if (edfsignal >= hdrlist[handle]->edfsignals) { return -1; }
	if (hdrlist[handle]->edf) { if (min < (-32768)) { return -1; } }
	else { if (min < (-8388608)) { return -1; } }
	if (hdrlist[handle]->datarecords) { return -1; }
	hdrlist[handle]->edfparam[edfsignal].dig_min = min;
	return 0;
}

int EdfSetPatientname(const int handle, const char* patientname)
{
	if (handle < 0) { return -1; }
	if (handle >= EDFLIB_MAXFILES) { return -1; }
	if (hdrlist[handle] == NULL) { return -1; }
	if (!(hdrlist[handle]->writemode)) { return -1; }
	if (hdrlist[handle]->datarecords) { return -1; }
	strncpy(hdrlist[handle]->plus_patient_name, patientname, 80);
	hdrlist[handle]->plus_patient_name[80] = 0;
	RemovePaddingTrailingSpaces(hdrlist[handle]->plus_patient_name);
	return 0;
}

int EdfSetPatientcode(const int handle, const char* patientcode)
{
	if (handle < 0) { return -1; }
	if (handle >= EDFLIB_MAXFILES) { return -1; }
	if (hdrlist[handle] == NULL) { return -1; }
	if (!(hdrlist[handle]->writemode)) { return -1; }
	if (hdrlist[handle]->datarecords) { return -1; }
	strncpy(hdrlist[handle]->plus_patientcode, patientcode, 80);
	hdrlist[handle]->plus_patientcode[80] = 0;
	RemovePaddingTrailingSpaces(hdrlist[handle]->plus_patientcode);
	return 0;
}

int EdfSetGender(const int handle, const int gender)
{
	if (handle < 0) { return -1; }
	if (handle >= EDFLIB_MAXFILES) { return -1; }
	if (hdrlist[handle] == NULL) { return -1; }
	if (!(hdrlist[handle]->writemode)) { return -1; }
	if (hdrlist[handle]->datarecords) { return -1; }
	if ((gender < 0) || (gender > 1)) { return -1; }
	if (gender) { hdrlist[handle]->plus_gender[0] = 'M'; }
	else { hdrlist[handle]->plus_gender[0] = 'F'; }
	hdrlist[handle]->plus_gender[1] = 0;
	return 0;
}

int EdfSetBirthdate(const int handle, const int year, const int month, const int day)
{
	if (handle < 0) { return -1; }
	if (handle >= EDFLIB_MAXFILES) { return -1; }
	if (hdrlist[handle] == NULL) { return -1; }
	if (!(hdrlist[handle]->writemode)) { return -1; }
	if (hdrlist[handle]->datarecords) { return -1; }
	if ((year < 1800) || (year > 3000) || (month < 1) || (month > 12) || (day < 1) || (day > 31)) { return -1; }
	sprintf(hdrlist[handle]->plus_birthdate, "%02i.%02i.%02i%02i", day, month, year / 100, year % 100);
	hdrlist[handle]->plus_birthdate[10] = 0;
	return 0;
}

int EdfSetPatientAdditional(const int handle, const char* additional)
{
	if (handle < 0) { return -1; }
	if (handle >= EDFLIB_MAXFILES) { return -1; }
	if (hdrlist[handle] == NULL) { return -1; }
	if (!(hdrlist[handle]->writemode)) { return -1; }
	if (hdrlist[handle]->datarecords) { return -1; }
	strncpy(hdrlist[handle]->plus_patient_additional, additional, 80);
	hdrlist[handle]->plus_patient_additional[80] = 0;
	RemovePaddingTrailingSpaces(hdrlist[handle]->plus_patient_additional);
	return 0;
}

int EdfSetAdmincode(const int handle, const char* admincode)
{
	if (handle < 0) { return -1; }
	if (handle >= EDFLIB_MAXFILES) { return -1; }
	if (hdrlist[handle] == NULL) { return -1; }
	if (!(hdrlist[handle]->writemode)) { return -1; }
	if (hdrlist[handle]->datarecords) { return -1; }
	strncpy(hdrlist[handle]->plus_admincode, admincode, 80);
	hdrlist[handle]->plus_admincode[80] = 0;
	RemovePaddingTrailingSpaces(hdrlist[handle]->plus_admincode);
	return 0;
}

int EdfSetTechnician(const int handle, const char* technician)
{
	if (handle < 0) { return -1; }
	if (handle >= EDFLIB_MAXFILES) { return -1; }
	if (hdrlist[handle] == NULL) { return -1; }
	if (!(hdrlist[handle]->writemode)) { return -1; }
	if (hdrlist[handle]->datarecords) { return -1; }
	strncpy(hdrlist[handle]->plus_technician, technician, 80);
	hdrlist[handle]->plus_technician[80] = 0;
	RemovePaddingTrailingSpaces(hdrlist[handle]->plus_technician);
	return 0;
}

int EdfSetEquipment(const int handle, const char* equipment)
{
	if (handle < 0) { return -1; }
	if (handle >= EDFLIB_MAXFILES) { return -1; }
	if (hdrlist[handle] == NULL) { return -1; }
	if (!(hdrlist[handle]->writemode)) { return -1; }
	if (hdrlist[handle]->datarecords) { return -1; }
	strncpy(hdrlist[handle]->plus_equipment, equipment, 80);
	hdrlist[handle]->plus_equipment[80] = 0;
	RemovePaddingTrailingSpaces(hdrlist[handle]->plus_equipment);
	return 0;
}

int EdfSetRecordingAdditional(const int handle, const char* additional)
{
	if (handle < 0) { return -1; }
	if (handle >= EDFLIB_MAXFILES) { return -1; }
	if (hdrlist[handle] == NULL) { return -1; }
	if (!(hdrlist[handle]->writemode)) { return -1; }
	if (hdrlist[handle]->datarecords) { return -1; }
	strncpy(hdrlist[handle]->plus_recording_additional, additional, 80);
	hdrlist[handle]->plus_recording_additional[80] = 0;
	RemovePaddingTrailingSpaces(hdrlist[handle]->plus_recording_additional);
	return 0;
}

int EdfSetStartdatetime(const int handle, const int year, const int month, const int day, const int hour, const int minute, const int second)
{
	if (handle < 0) { return -1; }
	if (handle >= EDFLIB_MAXFILES) { return -1; }
	if (hdrlist[handle] == NULL) { return -1; }
	if (!(hdrlist[handle]->writemode)) { return -1; }
	if (hdrlist[handle]->datarecords) { return -1; }
	if ((year < 1970) || (year > 3000) || (month < 1) || (month > 12) ||
		(day < 1) || (day > 31) || (hour < 0) || (hour > 23) ||
		(minute < 0) || (minute > 59) || (second < 0) || (second > 59)) { return -1; }

	hdrlist[handle]->startdate_year   = year;
	hdrlist[handle]->startdate_month  = month;
	hdrlist[handle]->startdate_day    = day;
	hdrlist[handle]->starttime_hour   = hour;
	hdrlist[handle]->starttime_minute = minute;
	hdrlist[handle]->starttime_second = second;
	return 0;
}

int EdfwriteAnnotationUTF8(const int handle, const long long onset, const long long duration, const char* description)
{
	if (handle < 0) { return -1; }
	if (handle >= EDFLIB_MAXFILES) { return -1; }
	if (hdrlist[handle] == NULL) { return -1; }
	if (!(hdrlist[handle]->writemode)) { return -1; }
	if (onset < 0LL) { return -1; }

	struct edf_write_annotationblock* listAnnot = (struct edf_write_annotationblock*)calloc(1, sizeof(struct edf_write_annotationblock));
	if (listAnnot == NULL) { return -1; }

	listAnnot->onset    = onset;
	listAnnot->duration = duration;
	strncpy(listAnnot->annotation, description, EDFLIB_WRITE_MAX_ANNOTATION_LEN);
	listAnnot->annotation[EDFLIB_WRITE_MAX_ANNOTATION_LEN] = 0;
	listAnnot->next_annotation                             = NULL;
	listAnnot->former_annotation                           = NULL;

	for (int i = 0; ; ++i)
	{
		if (listAnnot->annotation[i] == 0) { break; }
		if (listAnnot->annotation[i] < 32) { listAnnot->annotation[i] = '.'; }
	}

	if (write_annotationslist[handle] == NULL) { write_annotationslist[handle] = listAnnot; }
	else
	{
		struct edf_write_annotationblock* tmpAnnot = write_annotationslist[handle];
		while (tmpAnnot->next_annotation != NULL) { tmpAnnot = tmpAnnot->next_annotation; }
		tmpAnnot->next_annotation = listAnnot;
	}

	return 0;
}

int EdfwriteAnnotationLatin1(const int handle, const long long onset, const long long duration, const char* description)
{
	char str[EDFLIB_WRITE_MAX_ANNOTATION_LEN + 1];

	if (handle < 0) { return -1; }
	if (handle >= EDFLIB_MAXFILES) { return -1; }
	if (hdrlist[handle] == NULL) { return -1; }
	if (!(hdrlist[handle]->writemode)) { return -1; }
	if (onset < 0LL) { return -1; }

	struct edf_write_annotationblock* listAnnot = (struct edf_write_annotationblock*)calloc(1, sizeof(struct edf_write_annotationblock));
	if (listAnnot == NULL) { return -1; }

	listAnnot->onset    = onset;
	listAnnot->duration = duration;
	strncpy(str, description, EDFLIB_WRITE_MAX_ANNOTATION_LEN);
	str[EDFLIB_WRITE_MAX_ANNOTATION_LEN] = 0;
	Latin12UTF8(str, (int)strlen(str));
	strncpy(listAnnot->annotation, str, EDFLIB_WRITE_MAX_ANNOTATION_LEN);
	listAnnot->annotation[EDFLIB_WRITE_MAX_ANNOTATION_LEN] = 0;
	listAnnot->next_annotation                             = NULL;
	listAnnot->former_annotation                           = NULL;

	if (write_annotationslist[handle] == NULL) { write_annotationslist[handle] = listAnnot; }
	else
	{
		struct edf_write_annotationblock* tmpAnnot = write_annotationslist[handle];
		while (tmpAnnot->next_annotation != NULL) { tmpAnnot = tmpAnnot->next_annotation; }
		tmpAnnot->next_annotation = listAnnot;
	}

	return 0;
}

void RemovePaddingTrailingSpaces(char* str)
{
	int i;

	while (str[0] == ' ')
	{
		for (i = 0; ; ++i)
		{
			if (str[i] == 0) { break; }
			str[i] = str[i + 1];
		}
	}

	for (i = (int)strlen(str); i > 0; i--)
	{
		if (str[i - 1] == ' ') { str[i - 1] = 0; }
		else { break; }
	}
}

int EdfSetPrefilter(const int handle, const int edfsignal, const char* prefilter)
{
	if (handle < 0) { return -1; }
	if (handle >= EDFLIB_MAXFILES) { return -1; }
	if (hdrlist[handle] == NULL) { return -1; }
	if (!(hdrlist[handle]->writemode)) { return -1; }
	if (edfsignal < 0) { return -1; }
	if (edfsignal >= hdrlist[handle]->edfsignals) { return -1; }
	if (hdrlist[handle]->datarecords) { return -1; }
	strncpy(hdrlist[handle]->edfparam[edfsignal].prefilter, prefilter, 80);
	hdrlist[handle]->edfparam[edfsignal].prefilter[80] = 0;
	RemovePaddingTrailingSpaces(hdrlist[handle]->edfparam[edfsignal].prefilter);
	return 0;
}

int EdfSetTransducer(const int handle, const int edfsignal, const char* transducer)
{
	if (handle < 0) { return -1; }
	if (handle >= EDFLIB_MAXFILES) { return -1; }
	if (hdrlist[handle] == NULL) { return -1; }
	if (!(hdrlist[handle]->writemode)) { return -1; }
	if (edfsignal < 0) { return -1; }
	if (edfsignal >= hdrlist[handle]->edfsignals) { return -1; }
	if (hdrlist[handle]->datarecords) { return -1; }
	strncpy(hdrlist[handle]->edfparam[edfsignal].transducer, transducer, 80);
	hdrlist[handle]->edfparam[edfsignal].transducer[80] = 0;
	RemovePaddingTrailingSpaces(hdrlist[handle]->edfparam[edfsignal].transducer);
	return 0;
}

/* minimum is the minimum digits that will be printed (minus sign not included), leading zero's will be added if necessary */
/* if sign is zero, only negative numbers will have the sign '-' character */
/* if sign is one, the sign '+' or '-' character will always be printed */
/* returns the amount of characters printed */
int FprintIntNumberNonlocalized(FILE* file, int q, int minimum, const int sign)
{
	int flag = 0, j = 0, base = 1000000000;
	if (minimum < 0) { minimum = 0; }
	if (minimum > 9) { flag = 1; }

	if (q < 0)
	{
		fputc('-', file);
		j++;
		q = -q;
	}
	else
	{
		if (sign)
		{
			fputc('+', file);
			j++;
		}
	}

	for (int i = 10; i; i--)
	{
		if (minimum == i) { flag = 1; }

		const int z = q / base;
		q %= base;

		if (z || flag)
		{
			fputc('0' + z, file);
			j++;
			flag = 1;
		}
		base /= 10;
	}

	if (!flag)
	{
		fputc('0', file);
		j++;
	}

	return j;
}

/* minimum is the minimum digits that will be printed (minus sign not included), leading zero's will be added if necessary */
/* if sign is zero, only negative numbers will have the sign '-' character */
/* if sign is one, the sign '+' or '-' character will always be printed */
/* returns the amount of characters printed */
int FprintLLNumberNonlocalized(FILE* file, long long q, int minimum, const int sign)
{
	int flag       = 0, j = 0;
	long long base = 1000000000000000000LL;
	if (minimum < 0) { minimum = 0; }
	if (minimum > 18) { flag = 1; }

	if (q < 0LL)
	{
		fputc('-', file);
		j++;
		q = -q;
	}
	else
	{
		if (sign)
		{
			fputc('+', file);
			j++;
		}
	}

	for (int i = 19; i; i--)
	{
		if (minimum == i) { flag = 1; }

		const int z = (int)(q / base);
		q %= base;

		if (z || flag)
		{
			fputc('0' + z, file);
			j++;
			flag = 1;
		}
		base /= 10LL;
	}

	if (!flag)
	{
		fputc('0', file);
		j++;
	}

	return j;
}

/* minimum is the minimum digits that will be printed (minus sign not included), leading zero's will be added if necessary */
/* if sign is zero, only negative numbers will have the sign '-' character */
/* if sign is one, the sign '+' or '-' character will always be printed */
/* returns the amount of characters printed */
int SprintIntNumberNonlocalized(char* str, int q, int minimum, const int sign)
{
	int flag = 0, j = 0, base = 1000000000;

	if (minimum < 0) { minimum = 0; }
	if (minimum > 9) { flag = 1; }

	if (q < 0)
	{
		str[j++] = '-';
		q        = -q;
	}
	else { if (sign) { str[j++] = '+'; } }

	for (int i = 10; i; i--)
	{
		if (minimum == i) { flag = 1; }

		const int z = q / base;
		q %= base;

		if (z || flag)
		{
			str[j++] = '0' + z;
			flag     = 1;
		}
		base /= 10;
	}

	if (!flag) { str[j++] = '0'; }
	str[j] = 0;
	return j;
}

/* minimum is the minimum digits that will be printed (minus sign not included), leading zero's will be added if necessary */
/* if sign is zero, only negative numbers will have the sign '-' character */
/* if sign is one, the sign '+' or '-' character will always be printed */
/* returns the amount of characters printed */
int SprintLLNumberNonlocalized(char* str, long long q, int minimum, const int sign)
{
	int flag       = 0, j = 0;
	long long base = 1000000000000000000LL;

	if (minimum < 0) { minimum = 0; }
	if (minimum > 18) { flag = 1; }

	if (q < 0LL)
	{
		str[j++] = '-';
		q        = -q;
	}
	else { if (sign) { str[j++] = '+'; } }

	for (int i = 19; i; i--)
	{
		if (minimum == i) { flag = 1; }

		const int z = (int)(q / base);
		q %= base;

		if (z || flag)
		{
			str[j++] = '0' + z;
			flag     = 1;
		}
		base /= 10LL;
	}

	if (!flag) { str[j++] = '0'; }
	str[j] = 0;
	return j;
}

int SprintNumberNonlocalized(char* str, const double nr)
{
	int flag   = 0, z, i, j = 0, base = 1000000000;
	int q      = (int)nr;
	double var = nr - q;

	if (nr < 0.0)
	{
		str[j++] = '-';
		if (q < 0) { q = -q; }
	}

	for (i = 10; i; i--)
	{
		z = q / base;
		q %= base;

		if (z || flag)
		{
			str[j++] = '0' + z;
			flag     = 1;
		}
		base /= 10;
	}

	if (!flag) { str[j++] = '0'; }

	base = 100000000;
	var *= (base * 10);
	q = (int)var;

	if (q < 0) { q = -q; }

	if (!q)
	{
		str[j] = 0;
		return j;
	}

	str[j++] = '.';

	for (i = 9; i; i--)
	{
		z = q / base;
		q %= base;
		str[j++] = '0' + z;
		base /= 10;
	}

	str[j] = 0;
	j--;

	for (; j > 0; j--)
	{
		if (str[j] == '0') { str[j] = 0; }
		else
		{
			j++;
			break;
		}
	}
	return j;
}

double AtofNonlocalized(const char* str)
{
	int i              = 0, dotPos = -1, decimals = 0, sign = 1;
	double value2      = 0.0;
	const double value = AtoiNonlocalized(str);

	while (str[i] == ' ') { i++; }

	if ((str[i] == '+') || (str[i] == '-'))
	{
		if (str[i] == '-') { sign = -1; }
		i++;
	}

	for (; ; ++i)
	{
		if (str[i] == 0) { break; }
		if (((str[i] < '0') || (str[i] > '9')) && (str[i] != '.')) { break; }

		if (dotPos >= 0)
		{
			if ((str[i] >= '0') && (str[i] <= '9')) { decimals++; }
			else { break; }
		}

		if (str[i] == '.') { if (dotPos < 0) { dotPos = i; } }
	}

	if (decimals)
	{
		value2 = AtoiNonlocalized(str + dotPos + 1) * sign;
		i      = 1;
		while (decimals--) { i *= 10; }
		value2 /= i;
	}

	return (value + value2);
}

int AtoiNonlocalized(const char* str)
{
	int i = 0, value = 0, sign = 1;

	while (str[i] == ' ') { i++; }

	if ((str[i] == '+') || (str[i] == '-'))
	{
		if (str[i] == '-') { sign = -1; }
		i++;
	}

	for (; ; ++i)
	{
		if (str[i] == 0) { break; }
		if ((str[i] < '0') || (str[i] > '9')) { break; }

		value *= 10;
		value += (str[i] - '0');
	}
	return (value * sign);
}
