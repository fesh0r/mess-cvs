#include <stdlib.h>
#include <stdio.h>
#include "formats.h"
#include "utils.h"

/**************************************************************************/

struct bdf_file
{
	void *file;
	const struct bdf_procs *procs;
	UINT8 tracks, heads, sectors;
	UINT8 tracks_base, heads_base, sectors_base;
	UINT16 bytes_per_sector;
	int offset;
};

static int find_geometry_options(const struct InternalBdFormatDriver *drv, int filesize,
	 UINT8 *tracks, UINT8 *heads, UINT8 *sectors)
{
	int expected_size;
	int tracks_opt, heads_opt, sectors_opt;

	for(tracks_opt = 0; drv->tracks_options[tracks_opt]; tracks_opt++)
	{
		*tracks = drv->tracks_options[tracks_opt];
		for(heads_opt = 0; drv->heads_options[heads_opt]; heads_opt++)
		{
			*heads = drv->heads_options[heads_opt];
			for(sectors_opt = 0; drv->sectors_options[sectors_opt]; sectors_opt++)
			{
				*sectors = drv->sectors_options[sectors_opt];

				expected_size = *heads * *tracks * *sectors * drv->bytes_per_sector + drv->header_size;
				if (expected_size == filesize)
					return 0;

				if ((drv->flags & BDFD_ROUNDUP_TRACKS) && (expected_size > filesize)
						&& (((expected_size - filesize) % (*heads * *sectors * drv->bytes_per_sector)) == 0))
					return 0;
			}
		}
	}
	return 1;	/* failure */
}

int try_format_driver(const struct InternalBdFormatDriver *drv, const struct bdf_procs *procs,
	const char *extension, void *file, int filesize,
	int *success, UINT8 *tracks, UINT8 *heads, UINT8 *sectors, UINT16 *bytes_per_sector, int *offset)
{
	void *header;

	/* match the extension; if either the formatdriver or the caller do not
	 * specify the extension, count that as a match
	 */
	if (extension && drv->extension && strcmpi(extension, drv->extension))
		return 0;

	/* if there is a header, try to read it */
	if (drv->header_size > 0)
	{
		if (drv->header_size > filesize)
			return 0;

		header = malloc(drv->header_size);
		if (!header)
			return BLOCKDEVICE_ERROR_OUTOFMEMORY;

		/* read the header */
		procs->seekproc(file, 0, SEEK_SET);
		procs->readproc(file, header, drv->header_size);

		/* try to decode the header */
		if (!drv->header_decode(header, tracks, heads, sectors, bytes_per_sector, offset))			
			*success = 1;	/* success!!! */

		free(header);
	}
	else
	{
		*bytes_per_sector = 0;
		*offset = 0;

		if (!find_geometry_options(drv, filesize, tracks, heads, sectors))
			*success = 1;	/* success!!! */
	}
	return 0;
}

int bdf_create(const struct bdf_procs *procs, formatdriver_ctor format,
	void *file, const char *extension, UINT8 tracks, UINT8 heads, UINT8 sectors, void **outbdf)
{
	char buffer[1024];
	void *header = NULL;
	struct InternalBdFormatDriver drv;
	int bytes_to_write, len, err;
	formatdriver_ctor formats[2];

	format(&drv);

	if (drv.header_size)
	{
		header = malloc(drv.header_size);
		if (!header)
			goto outofmemory;

		err = drv.header_encode(header, tracks, heads, sectors, drv.bytes_per_sector);
		if (err)
			goto error;

		procs->writeproc(file, header, drv.header_size);
		free(header);
		header = NULL;
	}

	bytes_to_write = ((int) drv.bytes_per_sector) * tracks * heads * sectors;
	memset(buffer, drv.filler_byte, sizeof(buffer));

	while(bytes_to_write > 0)
	{
		len = (bytes_to_write > sizeof(buffer)) ? sizeof(buffer) : bytes_to_write;
		procs->writeproc(file, buffer, len);
		bytes_to_write -= len;
	}

	if (outbdf)
	{
		formats[0] = format;
		formats[1] = NULL;
		err = bdf_open(procs, formats, file, extension, outbdf);
		if (err)
			goto error;
	}
	return 0;

outofmemory:
	err = BLOCKDEVICE_ERROR_OUTOFMEMORY;
error:
	if (header)
		free(header);
	return err;
}

int bdf_open(const struct bdf_procs *procs, const formatdriver_ctor *formats,
	void *file, const char *extension, void **outbdf)
{
	int err, success;
	struct bdf_file *bdffile;
	int filesize;
	void *header;
	struct InternalBdFormatDriver drv;

	assert(formats);
	assert(file);
	assert(procs);
	assert(procs->filesizeproc);
	assert(procs->seekproc);
	assert(procs->readproc);
	assert(procs->writeproc);

	header = NULL;

	/* allocate the bdffile */
	bdffile = malloc(sizeof(struct bdf_file));
	if (!bdffile)
	{
		err = BLOCKDEVICE_ERROR_OUTOFMEMORY;
		goto done;
	}

	/* gather some initial information about the file */
	filesize = procs->filesizeproc(file);

	/* the first task is to locate an appropriate format driver */
	success = 0;
	while(!success && *formats)
	{
		/* build the format driver */
		(*formats)(&drv);

		err = try_format_driver(&drv, procs, extension, file, filesize, &success,
			&bdffile->tracks, &bdffile->heads, &bdffile->sectors, &bdffile->bytes_per_sector, &bdffile->offset);
		if (err)
			goto done;

		if (!success)
			formats++;
	}

	/* did we find an appropriate format driver? */
	if (!(*formats))
	{
		err = BLOCKDEVICE_ERROR_CANTDECODEFORMAT;
		goto done;
	}

	bdffile->file = file;
	bdffile->procs = procs;
	bdffile->tracks_base = drv.tracks_base;
	bdffile->heads_base = drv.heads_base;
	bdffile->sectors_base = drv.sectors_base;
	bdffile->bytes_per_sector = drv.bytes_per_sector;

	err = BLOCKDEVICE_ERROR_SUCCESS;

done:
	if (err)
	{
		if (bdffile)
			free(bdffile);
		bdffile = NULL;
	}
	*outbdf = (void *) bdffile;
	return err;
}

void bdf_close(void *bdf)
{
	struct bdf_file *bdffile = (struct bdf_file *) bdf;
	if (bdffile->procs->closeproc)
		bdffile->procs->closeproc(bdffile->file);
	free(bdffile);
}

static void bdf_seek(struct bdf_file *bdffile, UINT8 track, UINT8 head, UINT8 sector, int offset)
{
	int pos;

	assert(track >= bdffile->tracks_base);
	assert(track < bdffile->tracks);
	assert(head >= bdffile->heads_base);
	assert(head < bdffile->heads);
	assert(sector >= bdffile->sectors_base);
	assert(sector < bdffile->sectors);

	pos = track - bdffile->tracks_base;
	pos *= bdffile->heads;
	pos += head - bdffile->heads_base;
	pos *= bdffile->sectors;
	pos += sector - bdffile->sectors_base;
	pos *= bdffile->bytes_per_sector;
	pos += bdffile->offset;
	pos += offset;
	bdffile->procs->seekproc(bdffile->file, pos, SEEK_SET);
}

int bdf_read_sector(void *bdf, UINT8 track, UINT8 head, UINT8 sector, int offset, void *buffer, int length)
{
	struct bdf_file *bdffile = (struct bdf_file *) bdf;
	bdf_seek(bdffile, track, head, sector, offset);
	bdffile->procs->readproc(bdffile->file, buffer, length);
	return 0;
}

int bdf_write_sector(void *bdf, UINT8 track, UINT8 head, UINT8 sector, int offset, const void *buffer, int length)
{
	struct bdf_file *bdffile = (struct bdf_file *) bdf;
	bdf_seek(bdffile, track, head, sector, offset);
	bdffile->procs->writeproc(bdffile->file, buffer, length);
	return 0;
}

void bdf_get_geometry(void *bdf, UINT8 *tracks, UINT8 *heads, UINT8 *sectors)
{
	struct bdf_file *bdffile = (struct bdf_file *) bdf;	
	if (tracks)
		*tracks = bdffile->tracks;
	if (heads)
		*heads = bdffile->heads;
	if (sectors)
		*sectors = bdffile->sectors;
}

int bdf_is_readonly(void *bdf)
{
	struct bdf_file *bdffile = (struct bdf_file *) bdf;	
	return bdffile->procs->isreadonlyproc(bdffile->file);
}

#ifdef MAME_DEBUG
void validate_construct_formatdriver(struct InternalBdFormatDriver *drv, int tracks_optnum, int heads_optnum, int sectors_optnum)
{
	assert(heads_optnum && tracks_optnum && sectors_optnum);
	assert(tracks_optnum < sizeof(drv->tracks_options) / sizeof(drv->tracks_options[0]));
	assert(heads_optnum < sizeof(drv->heads_options) / sizeof(drv->heads_options[0]));
	assert(sectors_optnum < sizeof(drv->sectors_options) / sizeof(drv->sectors_options[0]));
	assert(drv->header_decode || drv->bytes_per_sector);
}
#endif
