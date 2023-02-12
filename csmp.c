#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "types.h"

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define	U16B(x)			(x)
#define	U32B(x)			(x)
#define	U64B(x)			(x)
#define	U16L(x)			__builtin_bswap16(x)
#define	U32L(x)			__builtin_bswap32(x)
#define	U64L(x)			__builtin_bswap64(x)
#else
#define	U16B(x)			__builtin_bswap16(x)
#define	U32B(x)			__builtin_bswap32(x)
#define	U64B(x)			__builtin_bswap64(x)
#define	U16L(x)			(x)
#define	U32L(x)			(x)
#define	U64L(x)			(x)
#endif

typedef struct {
	char	magic[4];
	u64	data_size;
	u64	unk1;
	char	type[4];
	u32	unk2;
	u32	unk3;
} __attribute__((packed)) RFRMHeader;

typedef struct {
	u32	size;
	u32	id1;
	u32	id2;
} SampleInfo;

typedef struct {
	RFRMHeader rfrm;
	u32	name_length;
	char	name[0];
} CAUD;

typedef struct {
	u32	unk0[2];
	u32	type;
	u8	unk1[134];
} __attribute__((packed)) CAUDSegment2;

typedef struct {
	u32	magic;
	u32	unk1[5];
	u8	channels;
	u8	unk2;
	u8	unk3[3];
} __attribute__((packed)) CSMPFMTA;

typedef struct {
	u32	magic;
	u32	size;
	u32	unk[4];
	u8	unk2;
	u8	unk3;
	u8	data[3];
} __attribute__((packed)) CSMPCRMS;

typedef struct {
	u32	magic;
	u64	unk0;
	u64	unk1;
	u32	unk2;
	u32	block_size;
	u32	blocks;
	u32	block_samples;
	u32	first_block_pad;
	u32	last_block_samples;
	u32	loop_start_block;
	u32	loop_start_sample;
	u32	loop_end_block;
	u32	loop_end_sample;
} __attribute__((packed)) CSMPRAS3;

typedef struct {
	u32	num_samples;
	u32	num_adpcm_nibbles;
	u32	sample_rate;
	u16	loop_flag;
	u16	format;
	u32	sa;
	u32	ea;
	u32	ca;
	s16	coef[16];
	u16	gain; // never used anyway
	u16	ps,yn1,yn2;
	u16	lps,lyn1,lyn2;
	u32	pad[13];
} DSPHeader;

typedef struct {
	u32	magic;
	u32	data_length;
	u32	unk[4];
	DSPHeader dsp[0];
} __attribute__((packed)) CSMPDATA;

/* interleave is 0x10000 bytes */

static void readall(int fd, void* buf, size_t size)
{
	ssize_t rd;
	size_t sz = size;
	u8* p = (u8*) buf;
	while(sz != 0 && (rd = read(fd, p, sz)) != 0) {
		if(rd == -1) {
			perror("read");
			exit(1);
		}
		p += rd;
		sz -= rd;
	}
}

static void writeall(int fd, void* buf, size_t size)
{
	ssize_t wr;
	size_t sz = size;
	u8* p = (u8*) buf;
	while(sz != 0 && (wr = write(fd, p, sz)) != 0) {
		if(wr == -1) {
			perror("write");
			exit(1);
		}
		p += wr;
		sz -= wr;
	}
}

int caud_get_offset(int type)
{
	switch(type) {
		case 0x0C:
			return 47;
		case 0x1C:
			return 51;
		case 0x1D:
			return 51;
		case 0x28:
			return 44;
		case 0x3C:
			return 52;
		case 0x48:
			return 43;
		case 0x49:
			return 43;
		case 0x4C:
			return 47;
		case 0x4D:
			return 47;
		case 0x4E:
			return 47;
		case 0x4F:
			return 47;
		case 0x58:
			return 47;
		case 0x5C:
			return 51;
		case 0x5D:
			return 51;
		case 0x5E:
			return 51;
		case 0x5F:
			return 51;
		case 0x6C:
			return 48;
		case 0x7C:
			return 52;
		case 0xDC:
			return 51;
		default:
			return 0;
	}
}

int get_csmp_name(char* csmpname, CAUDSegment2* info)
{
	char filename[64];

	int offset = caud_get_offset(info->type);
	u8* id = (u8*) ((uintptr_t) info + offset);
	sprintf(filename, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x.CSMP",
			id[15], id[14], id[13], id[12], id[11], id[10], id[9],
			id[8], id[7], id[6], id[5], id[4], id[3], id[2], id[1],
			id[0]);

	filename[41] = 0;

	/* check for file existence */
	sprintf(csmpname, "files/%s", filename);
	return access(csmpname, R_OK) == 0;
}

void extract_csmp(const char* outfilename, CSMPDATA* data, size_t size, int channels, int interleave, int padding, unsigned int loop, unsigned int sa, unsigned int ea)
{
	printf("extracting %d channels (%s interleave)...\n", channels, interleave == 0 ? "without" : "with");
	unsigned int bytes = (data->dsp[0].num_adpcm_nibbles + 1) / 2;
	/* align short block to ADPCM frames */
	bytes = ((bytes + 7) / 8) * 8;

	size_t blocks = 0;
	if(interleave) {
		size_t filesize = data->data_length - channels * sizeof(DSPHeader);
		blocks = filesize / channels / interleave;
		printf("blocks: %ld\n", blocks);
		if(blocks == 0) {
			printf("short block detected\n");
		}
	}

	/* compute dsp data offset */
	u8* dspdata = (u8*) &data->dsp[channels];
	unsigned int last_block = bytes;
	if(interleave) {
		last_block %= interleave;
	}
	printf("last_block: %d\n", last_block);

	if(!interleave) {
		/* no interleave: channels are just concatenated one after another */
		size_t filesize = data->data_length - channels * sizeof(DSPHeader);
		bytes = (filesize + channels - 1) / channels;
	}

	/* dump channel by channel */
	for(unsigned int ch = 0; ch < channels; ch++) {
		char filename[256];
		if(channels == 1) {
			sprintf(filename, "%s.dsp", outfilename);
		} else if(channels == 2) {
			sprintf(filename, "%s%c.dsp", outfilename, ch == 0 ? 'L' : 'R');
		} else {
			sprintf(filename, "%s-%d.dsp", outfilename, ch);
		}
		printf("processing channel %d [%s]...\n", ch, filename);

		int fd = open(filename, O_WRONLY | O_CREAT, 0644);
		if(fd == -1) {
			perror("open");
		}

		/* endianess swap DSP header */
		DSPHeader hdr = data->dsp[ch];
		hdr.num_samples = U32B(hdr.num_samples);
		hdr.num_adpcm_nibbles = U32B(hdr.num_adpcm_nibbles);
		hdr.sample_rate = U32B(hdr.sample_rate);
		hdr.loop_flag = U16B(hdr.loop_flag);
		hdr.format = U16B(hdr.format);
		hdr.sa = U32B(hdr.sa);
		hdr.ea = U32B(hdr.ea);
		hdr.ca = U32B(hdr.ca);
		hdr.gain = U16B(hdr.gain);
		hdr.ps = U16B(hdr.ps);
		hdr.yn1 = U16B(hdr.yn1);
		hdr.yn2 = U16B(hdr.yn2);
		hdr.lps = U16B(hdr.lps);
		hdr.lyn1 = U16B(hdr.lyn1);
		hdr.lyn2 = U16B(hdr.lyn2);

		if(loop) {
			hdr.loop_flag = U16B(1);
			hdr.sa = U32B(sa);
			hdr.ea = U32B(ea);
			hdr.ca = U32B(2);
		}

		/* update initial PS / loop PS */
		if(blocks == 0) {
			printf("data start: %p\n", (uintptr_t) &dspdata[ch * bytes] - (uintptr_t) data);
			if(data->dsp[ch].ps != dspdata[ch * bytes]) {
				printf("WARNING: ps mismatch found on channel %d: 0x%02X vs 0x%02X (short)\n", ch, dspdata[ch * bytes], data->dsp[ch].ps);
			}
			hdr.ps = U16B(dspdata[ch * bytes]);

			if(loop) {
				u32 sa_ps = sa / 16 * 8;
				hdr.lps = U16B(dspdata[ch * bytes + sa_ps]);
			}
		} else {
			u8* ch_start = &dspdata[ch * interleave + padding];
			printf("data start: %p\n", (uintptr_t) ch_start - (uintptr_t) data);
			if(data->dsp[ch].ps != *ch_start) {
				printf("WARNING: ps mismatch found on channel %d: 0x%02X vs 0x%02X\n", ch, *ch_start, data->dsp[ch].ps);
			}
			hdr.ps = U16B(*ch_start);

			if(loop) {
				u32 sa_ps = sa / 16 * 8;
				u32 sa_block = sa_ps / interleave;
				u32 sa_off = sa_ps % interleave;
				hdr.lps = U16B(dspdata[(sa_block * channels + ch) * interleave + sa_off]);
			}
		}

		for(unsigned int i = 0; i < 16; i++) {
			hdr.coef[i] = U16B(hdr.coef[i]);
		}

		printf("CHANNEL %d\n", ch);
		printf("num_samples: %d\n", U32B(hdr.num_samples));
		printf("num_adpcm_nibbles: %d\n", U32B(hdr.num_adpcm_nibbles));
		printf("sample_rate: %d\n", U32B(hdr.sample_rate));
		printf("loop_flag: %d\n", U16B(hdr.loop_flag));
		printf("format: %d\n", U16B(hdr.format));
		printf("sa: %d, ea: %d, ca: %d\n", U32B(hdr.sa), U32B(hdr.ea), U32B(hdr.ca));
		printf("gain: %d, ps: %d, yn1: %d, yn2: %d\n", U16B(hdr.gain), U16B(hdr.ps), U16B(hdr.yn1), U16B(hdr.yn2));
		printf("lps: %d, lyn1: %d, lyn2: %d\n", U16B(hdr.lps), U16B(hdr.lyn1), U16B(hdr.lyn2));

		/* write DSP header */
		write(fd, &hdr, 96);

		/* write audio data */
		if(blocks == 0 || channels == 1) {
			/* short block handling */
			writeall(fd, &dspdata[ch * bytes], bytes);
		} else {
			/* first block */
			if(padding == 0) {
				writeall(fd, &dspdata[ch * interleave], interleave);
			} else {
				writeall(fd, &dspdata[ch * interleave + padding], interleave - padding);
			}

			/* remaining blocks */
			for(size_t i = 1; i < blocks; i++) {
				writeall(fd, &dspdata[(i * channels + ch) * interleave], interleave);
			}
		}

		close(fd);
	}
}

void load_csmp(const char* filename, const char* outfilename)
{
	struct stat buf;
	if(stat(filename, &buf) == -1) {
		perror("stat");
		return;
	}

	int fd = open(filename, O_RDONLY);
	if(fd == -1) {
		perror("open");
		return;
	}

	void* csmp = malloc(buf.st_size);
	if(csmp == NULL) {
		perror("malloc");
		return;
	}
	readall(fd, csmp, buf.st_size);
	close(fd);

	RFRMHeader* rfrm = (RFRMHeader*) csmp;

	if(memcmp(rfrm->magic, "RFRM", 4)) {
		printf("invalid magic: expected RFRM, got %c%c%c%c\n", rfrm->magic[0], rfrm->magic[1], rfrm->magic[2], rfrm->magic[3]);
		free(csmp);
		return;
	}

	if(memcmp(rfrm->type, "CSMP", 4)) {
		printf("invalid magic: expected CSMP, got %c%c%c%c\n", rfrm->type[0], rfrm->type[1], rfrm->type[2], rfrm->type[3]);
		free(csmp);
		return;
	}

	char csmpfilename[256];
	sprintf(csmpfilename, "csmp/%s.csmp", outfilename);
	fd = open(csmpfilename, O_WRONLY | O_CREAT, 0644);
	if(fd != -1) {
		writeall(fd, csmp, buf.st_size);
		close(fd);
	}

	unsigned int channels = -1;
	unsigned int interleave = 0;
	unsigned int pad = 0;
	unsigned int sa = 0;
	unsigned int ea = 0;
	unsigned int loop = 0;
	u8* ptr = (u8*) &rfrm[1];
	for(unsigned int i = 0; i < 4; i++) {
		switch(*((u32*) ptr)) {
			case 0x41544D46: { /* FMTA */
				printf("CHUNK: FMTA\n");
				CSMPFMTA* fmta = (CSMPFMTA*) ptr;
				channels = fmta->channels;
				printf("channels: %d\n", channels);
				ptr += sizeof(CSMPFMTA);
				break;
			}

			case 0x534D5243: { /* CRMS */
				printf("CHUNK: CRMS\n");
				CSMPCRMS* crms = (CSMPCRMS*) ptr;
				ptr += sizeof(CSMPCRMS);
				u32* len = (u32*) ptr;
				ptr += *len + 4;
				break;
			}

			case 0x33534152: { /* RAS3 */
				printf("CHUNK: RAS3\n");
				CSMPRAS3* ras3 = (CSMPRAS3*) ptr;
				ptr += sizeof(CSMPRAS3);
				interleave = ras3->block_size / channels;
				pad = (ras3->first_block_pad + 13) / 14 * 8;
				sa = ras3->loop_start_block * interleave * 2 + ras3->loop_start_sample * 16 / 14 - (pad * 2);
				ea = ras3->loop_end_block * interleave * 2 + ras3->loop_end_sample * 16 / 14 - (pad * 2);
				if((sa % 16) == 0) {
					sa += 2;
				} else if((sa % 16) == 1) {
					sa++;
				}
				loop = (sa != 0 || ea != 0) && (sa < ea);
				printf("interleave: 0x%x\n", interleave);
				printf("blocks: %d\n", ras3->blocks);
				printf("samples per block: %d\n", ras3->block_samples);
				printf("initial padding: %d (%d bytes)\n", ras3->first_block_pad, pad);
				printf("last block (samples): %d\n", ras3->last_block_samples);
				printf("loop start: %d:%d => sa=%d\n", ras3->loop_start_block, ras3->loop_start_sample, sa);
				printf("loop end: %d:%d => ea=%d\n", ras3->loop_end_block, ras3->loop_end_sample, ea);
				printf("loop: %d\n", loop);
				printf("unk0 = 0x%016lx (%lu)\n", ras3->unk0, ras3->unk0);
				printf("unk1 = 0x%016lx (%lu)\n", ras3->unk1, ras3->unk1);
				printf("unk2 = 0x%08x (%u)\n", ras3->unk2, ras3->unk2);
				break;
			}

			case 0x41544144: { /* DATA */
				printf("CHUNK: DATA\n");
				CSMPDATA* data = (CSMPDATA*) ptr;
				printf("sample rate: %d / %d\n", data->dsp[0].sample_rate, data->dsp[1].sample_rate);
				printf("sample count: %d / %d\n", data->dsp[0].num_samples, data->dsp[1].num_samples);
				printf("nibble count: %d / %d\n", data->dsp[0].num_adpcm_nibbles, data->dsp[1].num_adpcm_nibbles);
				printf("format: %d / %d\n", data->dsp[0].format, data->dsp[1].format);
				printf("loop flag: %d / %d\n", data->dsp[0].loop_flag, data->dsp[1].loop_flag);
				printf("loop start: %d / %d\n", data->dsp[0].sa, data->dsp[1].sa);
				printf("loop end: %d / %d\n", data->dsp[0].ea, data->dsp[1].ea);
				printf("ca: %d / %d\n", data->dsp[0].ca, data->dsp[1].ca);
				printf("data starts at: %p\n", (uintptr_t) &data->dsp[2] - (uintptr_t) csmp);
				size_t size = buf.st_size - ((uintptr_t) &data->dsp[2] - (uintptr_t) csmp);
				printf("data size: %ld\n", size);
				printf("data size (including DSP headers): %d\n", data->data_length);
				extract_csmp(outfilename, data, size, channels, interleave, pad, loop, sa, ea);
				free(csmp);
				return;
			}

			default:
				printf("unknown chunk: %c%c%c%c\n", ptr[0], ptr[1], ptr[2], ptr[3]);
				free(csmp);
				return;
		}
	}

	printf("Failed to locate DATA chunk\n");
	free(csmp);
}

int main(int argc, char** argv)
{
	const char* input = NULL;

	if(argc == 2) {
		input = argv[1];
	} else {
		printf("Usage: %s input.caud\n", *argv);
		return 1;
	}

	struct stat buf;

	if(stat(input, &buf) == -1) {
		perror("stat");
		return 1;
	}

	printf("total size = %lu\n", buf.st_size);

	CAUD* caud = (CAUD*) malloc(buf.st_size);
	if(caud == NULL) {
		perror("malloc");
		return 1;
	}
	int fd = open(input, O_RDONLY);
	if(fd == -1) {
		perror("open");
		return 1;
	}
	readall(fd, caud, buf.st_size);
	close(fd);

	char* name = (char*) malloc(caud->name_length + 1);
	if(name == NULL) {
		perror("malloc");
		return 1;
	}
	memcpy(name, caud->name, caud->name_length);
	name[caud->name_length] = 0;

	CAUDSegment2* info = (CAUDSegment2*) ((uintptr_t) caud->name + caud->name_length);

	u32 len = buf.st_size - ((uintptr_t) info - (uintptr_t) caud);

	if(memcmp(caud->rfrm.magic, "RFRM", 4)) {
		printf("invalid magic: expected RFRM, got %c%c%c%c\n", caud->rfrm.magic[0], caud->rfrm.magic[1], caud->rfrm.magic[2], caud->rfrm.magic[3]);
		return 1;
	}

	if(memcmp(caud->rfrm.type, "CAUD", 4)) {
		printf("invalid magic: expected CAUD, got %c%c%c%c\n", caud->rfrm.type[0], caud->rfrm.type[1], caud->rfrm.type[2], caud->rfrm.type[3]);
		return 1;
	}

	u32* id = (u32*) ((uintptr_t) info + caud_get_offset(info->type));
	printf("magic = %c%c%c%c\n", caud->rfrm.type[0], caud->rfrm.type[1], caud->rfrm.type[2], caud->rfrm.type[3]);
	printf("name = \"%s\"\n", name);
	printf("type = 0x%02X\n", info->type);
	printf("CSMP ID: %08X%08X%08X%08X\n", id[3], id[2], id[1], id[0]);

	char csmpname[256];
	if(!get_csmp_name(csmpname, info)) {
		printf("Failed to find CSMP file\n");
		return 1;
	}

	load_csmp(csmpname, name);
}
