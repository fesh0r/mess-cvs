//#include "mamalleg.h"
#include "driver.h"
#include "unzip.h"
#include "rc.h"
#include <unistd.h>
#include <signal.h>

#ifndef _MSC_VER
#include <sys/stat.h>
#endif

/* Verbose outputs to error.log ? */
#define VERBOSE 	0

/* Use the file cache ? */
#define FILE_CACHE	1

#if VERBOSE
#define LOG(x)	logerror x
#else
#define LOG(x)	/* x */
#endif

char *roms = NULL;
char **rompathv = NULL;
int rompathc = 0;

char *samples = NULL;
char **samplepathv = NULL;
int samplepathc = 0;

static const char *rompath;
static const char *samplepath;
static const char *cfgdir, *nvdir, *hidir, *inpdir, *stadir;
static const char *memcarddir, *artworkdir, *screenshotdir, *cheatdir;
/* from datafile.c */
extern const char *history_filename;
extern const char *mameinfo_filename;
/* from cheat.c */
extern char *cheatfile;

/* MESS specifc */
static char *soft = NULL;
static char **softpathv = NULL;
static int softpathc = 0;

struct rc_option fileio_opts[] =
{
	/* name, shortname, type, dest, deflt, min, max, func, help */
	{ "Windows path and directory options", NULL, rc_seperator, NULL, NULL, 0, 0, NULL, NULL },
	{ "rompath", "rp", rc_string, &rompath, "roms", 0, 0, request_decompose_rompath, "path to romsets" },
	{ "samplepath", "sp", rc_string, &samplepath, "samples", 0, 0, request_decompose_samplepath, "path to samplesets" },
	{ "cfg_directory", NULL, rc_string, &cfgdir, "cfg", 0, 0, NULL, "directory to save configurations" },
	{ "nvram_directory", NULL, rc_string, &nvdir, "nvram", 0, 0, NULL, "directory to save nvram contents" },
	{ "memcard_directory", NULL, rc_string, &memcarddir, "memcard", 0, 0, NULL, "directory to save memory card contents" },
	{ "input_directory", NULL, rc_string, &inpdir, "inp", 0, 0, NULL, "directory to save input device logs" },
	{ "hiscore_directory", NULL, rc_string, &hidir, "hi", 0, 0, NULL, "directory to save hiscores" },
	{ "state_directory", NULL, rc_string, &stadir, "sta", 0, 0, NULL, "directory to save states" },
	{ "artwork_directory", NULL, rc_string, &artworkdir, "artwork", 0, 0, NULL, "directory for Artwork (Overlays etc.)" },
	{ "snapshot_directory", NULL, rc_string, &screenshotdir, "snap", 0, 0, NULL, "directory for screenshots (.png format)" },
	{ "cheat_file", NULL, rc_string, &cheatfile, "cheat.dat", 0, 0, NULL, "cheat filename" },
	{ "history_file", NULL, rc_string, &history_filename, "history.dat", 0, 0, NULL, NULL },
	{ "mameinfo_file", NULL, rc_string, &mameinfo_filename, "mameinfo.dat", 0, 0, NULL, NULL },
	{ NULL,	NULL, rc_end, NULL, NULL, 0, 0,	NULL, NULL }
};


char *alternate_name;				   /* for "-romdir" */

typedef enum
{
	kPlainFile,
	kRAMFile,
	kZippedFile
}	eFileType;

typedef struct
{
	FILE *file;
	unsigned char *data;
	unsigned int offset;
	unsigned int length;
	eFileType type;
	unsigned int crc;
	int		eof;	// for kRamFiles only
}	FakeFileHandle;


extern unsigned int crc32 (unsigned int crc, const unsigned char *buf, unsigned int len);
static int checksum_file (const char *file, unsigned char **p, unsigned int *size, unsigned int *crc);

/*
 * File stat cache LRU (Last Recently Used)
 */

#if FILE_CACHE
struct file_cache_entry
{
	struct stat stat_buffer;
	int result;
	char *file;
};

/* File cache buffer */
static struct file_cache_entry **file_cache_map = 0;

/* File cache size */
static unsigned int file_cache_max = 0;

/* AM 980919 */
static int cache_stat (const char *path, struct stat *statbuf)
{
	if( file_cache_max )
	{
		unsigned i;
		struct file_cache_entry *entry;

		/* search in the cache */
		for( i = 0; i < file_cache_max; ++i )
		{
			if( file_cache_map[i]->file && strcmp (file_cache_map[i]->file, path) == 0 )
			{	/* found */
				unsigned j;

//				LOG(("File cache HIT  for %s\n", path));
                /* store */
				entry = file_cache_map[i];

				/* shift */
				for( j = i; j > 0; --j )
					file_cache_map[j] = file_cache_map[j - 1];

				/* set the first entry */
				file_cache_map[0] = entry;

				if( entry->result == 0 )
					memcpy (statbuf, &entry->stat_buffer, sizeof (struct stat));

				return entry->result;
			}
		}
//		LOG(("File cache FAIL for %s\n", path));

		/* oldest entry */
		entry = file_cache_map[file_cache_max - 1];
		free (entry->file);

		/* shift */
		for( i = file_cache_max - 1; i > 0; --i )
			file_cache_map[i] = file_cache_map[i - 1];

		/* set the first entry */
		file_cache_map[0] = entry;

		/* file */
		entry->file = (char *) malloc (strlen (path) + 1);
		strcpy (entry->file, path);

		/* result and stat */
		entry->result = stat (path, &entry->stat_buffer);

		if( entry->result == 0 )
			memcpy (statbuf, &entry->stat_buffer, sizeof (struct stat));

		return entry->result;
	}
	else
	{
		return stat (path, statbuf);
	}
}

/* AM 980919 */
static void cache_allocate (unsigned entries)
{
	if( entries )
	{
		unsigned i;

		file_cache_max = entries;
		file_cache_map = (struct file_cache_entry **) malloc (file_cache_max * sizeof (struct file_cache_entry *));

		for( i = 0; i < file_cache_max; ++i )
		{
			file_cache_map[i] = (struct file_cache_entry *) malloc (sizeof (struct file_cache_entry));
			memset (file_cache_map[i], 0, sizeof (struct file_cache_entry));
		}
		LOG(("File cache allocated for %d entries\n", file_cache_max));
	}
}
#else

#define cache_stat(a,b) stat(a,b)

#endif

/* Lee Ward 1 June 2001 Merged Kev Thacker's seperated path decompose stuff */
/* KT - seperated it out - the same code was used over and over again, and added some error checking */
void	decompose(char *dest_path, int *pathc, char *new_path, char ***ppathv)
{
	char *path;
	int path_count;
	char *token;
	char **pathv = *ppathv;

	path_count = 0;

	/* if path passed in is invalid, make it valid! */
	if (new_path==0)
	{
		/* no path specified, use current directory for path */
		path = ".";
		LOG(("using default path\n"));
	}
	else
	{
		LOG(("path specified: %s\n",new_path));
		path = new_path;
	}

	if (dest_path)
	{
		dest_path = realloc(dest_path, strlen(path)+1);
	}
	else
	{
		dest_path = malloc(strlen(path)+1);
	}

	if (!dest_path)
	{
		logerror("decompose: failed to malloc!\n");
		*pathc = 0;
		raise(SIGABRT);
	}

	strcpy(dest_path, path);

	token = strtok (dest_path, ";");
	while( token )
	{
		if( path_count )
			pathv = realloc (pathv, (path_count + 1) * sizeof(char *));
		else
			pathv = malloc (sizeof(char *));
		if( !pathv )
			break;
		pathv[path_count++] = token;
		token = strtok (NULL, ";");
	}

	*pathc = path_count;
	*ppathv = pathv;
	LOG(("path count: %d\n",path_count));
}
/* This function can be called several times with different parameters,
 * for example by "mame -verifyroms *". */
void decompose_rom_sample_path (char *rompath, char *samplepath)
{
		decompose(roms, &rompathc, rompath, &rompathv);
		decompose(samples, &samplepathc, samplepath, &samplepathv);

#if FILE_CACHE
    /* AM 980919 */
    if( file_cache_max == 0 )
    {
        /* (rom path directories + 1 buffer)==rompathc+1 */
        /* (dir + .zip + .zif)==3 */
        /* (clone+parent)==2 */
        cache_allocate ((rompathc + 1) * 3 * 2);
    }
#endif

}
void decompose_software_path(char *softwarepath)
{
	decompose(soft, &softpathc, softwarepath, &softpathv);

	LOG(("Number of software paths is %d\n", softpathc));
}
/*
 * file handling routines
 *
 * gamename holds the driver name, filename is only used for ROMs and samples.
 * if 'write' is not 0, the file is opened for write. Otherwise it is opened
 * for read.
 */

/*
 * check if roms/samples for a game exist at all
 * return index+1 of the path vector component on success, otherwise 0
 */
 /* Lee Ward 1 June 2001 Fixed for MESS to handle Software Directory */
int osd_faccess (const char *newfilename, int filetype)
{
	static int indx;
	static const char *filename;
    char name[256];
    char **pathv;
    int pathc;
	char *dir_name;

	/* if filename == NULL, continue the search */
	if( newfilename != NULL )
	{
		indx = 0;
		filename = newfilename;
	}
	else
		indx++;

	if( filetype == OSD_FILETYPE_ROM )
	{
		pathv = rompathv;
		pathc = rompathc;
	}
	else
	if( filetype == OSD_FILETYPE_SAMPLE )
	{
		pathv = samplepathv;
		pathc = samplepathc;
	}
	else
#ifdef MESS
	if (filetype == OSD_FILETYPE_IMAGE_R ||
		filetype == OSD_FILETYPE_IMAGE_RW)
	{
		pathv = softpathv;
		pathc = softpathc;
	}
	else
#endif
	if( filetype == OSD_FILETYPE_SCREENSHOT )
	{
		void *f;

		sprintf (name, "%s/%s.png", screenshotdir, newfilename);
		f = fopen (name, "rb");
		if( f )
		{
			fclose (f);
			return 1;
		}
		else
			return 0;
	}
	else
		return 0;

	for( ; indx < pathc; indx++ )
	{
		struct stat stat_buffer;

		dir_name = pathv[indx];

		/* does such a directory (or file) exist? */
		sprintf (name, "%s/%s", dir_name, filename);
		if( cache_stat (name, &stat_buffer) == 0 )
			return indx + 1;

		/* try again with a .zip extension */
		sprintf (name, "%s/%s.zip", dir_name, filename);
		if( cache_stat (name, &stat_buffer) == 0 )
			return indx + 1;

		/* try again with a .zif extension */
		sprintf (name, "%s/%s.zif", dir_name, filename);
		if( cache_stat (name, &stat_buffer) == 0 )
			return indx + 1;
	}

	/* no match */
	return 0;
}
/* Lee Ward 01 June 2001 Support for multiple directories */
int try_fopen(FakeFileHandle *f, const char *path1, const char *path2, const char *path3, const char *file, const char *img, int do_checksum, int try_zip, int openforwrite)
{
	static char *write_modes[] = {"rb", "wb", "r+b", "r+b", "w+b"};
	struct stat stat_buffer;
	char name[260] = "";
	int found = 0;

	if (path1)
		strcpy(name, path1);
	if (path2)
	{
		if (strlen(name))
			strcat(name, "/");
		strcat(name, path2);
	}
	if (path3)
	{
		if (strlen(name))
			strcat(name, "/");
		strcat(name, path3);
	}
	if (!strlen(name))
		return 0;

	if (cache_stat(name, &stat_buffer) != 0 || (stat_buffer.st_mode & S_IFDIR) == 0)
		return 0;

	LOG(("  try directory %s\n", name));
	if (try_zip)
	{
		char *ext;
		strcat(name, "/");
		strcat(name, file);
		ext = strrchr(name, '.');
		if (ext && strchr(ext, '/') == NULL && strchr(ext, '\\') == NULL)
			strcpy(ext, ".zip");
		else
			strcat(name, ".zip");
		LOG(("       filename %s\n", name));
		if (cache_stat(name, &stat_buffer) == 0)
		{
			if (load_zipped_file(name, img, &f->data, &f->length) == 0)
			{
				f->type = kZippedFile;
				f->offset = 0;
				f->crc = crc32(0L, f->data, f->length);
				found = 1;
			}
		}
	}
	else
	{
		strcat(name, "/");
		strcat(name, file);
		if (do_checksum)
		{
			LOG(("  checksum_file %s\n", name));
			if (checksum_file(name, &f->data, &f->length, &f->crc) == 0)
			{
				f->type = kRAMFile;
				f->offset = 0;
				found = 1;
			}
		}
		else
		{
			if (cache_stat(name, &stat_buffer) == 0)
			{
				LOG(("  open filename %s mode %s\n", name, write_modes[openforwrite]));
				f->type = kPlainFile;
				f->file = fopen(name, write_modes[openforwrite]);
				found = f->file != 0;
			}
			if (!found && openforwrite == 3)
			{
				LOG(("    create file %s mode %s\n", name, write_modes[4]));
				f->file = fopen(name, write_modes[4]);
				found = f->file != 0;
			}
		}
	}

	if (found)
		LOG(("FOUND %s in %s!\n", img, name));

	return found;
}
/* Lee Ward 01 June 2001 Added Support for MESS Filetypes */
/* JB 980920 update */
/* AM 980919 update */
void *osd_fopen(const char *game, const char *filename, int filetype, int openforwrite)
{
	char name[256];
	char file[256];
	char extd[32];
	char *extension;
	char *gamename;
	int found = 0;
	int indx;
	struct stat stat_buffer;
	FakeFileHandle *f;
	int pathc;
	char **pathv;


	f = (FakeFileHandle *) malloc(sizeof (FakeFileHandle));
	if (!f)
	{
		logerror("osd_fopen: failed to malloc FakeFileHandle!\n");
		return 0;
	}
	memset(f, 0, sizeof (FakeFileHandle));

	gamename = (char *) game;

	/* Support "-romdir" yuck. */
	if (alternate_name)
	{
				/* check for DEFAULT.CFG file request */
		if( filetype == OSD_FILETYPE_CONFIG && gamename == "default" )
		{
			LOG(("osd_fopen: default input configuration file requested; -romdir switch not applied\n"));
		} else {
			LOG(("osd_fopen: -romdir overrides '%s' by '%s'\n", gamename, alternate_name));
        		gamename = alternate_name;
        	}

	}

	switch (filetype)
	{
	case OSD_FILETYPE_ROM:
	case OSD_FILETYPE_SAMPLE:

		/* only for reading */
		if (openforwrite)
		{
			LOG(("osd_fopen: type %02x write not supported\n", filetype));
			break;
		}

		if (filetype == OSD_FILETYPE_SAMPLE)
		{
			LOG(("Open SAMPLE '%s' for '%s'\n", filename, game));
			pathc = samplepathc;
			pathv = samplepathv;
		}
		else
		{
			LOG(("Open ROM '%s' for '%s'\n", filename, game));
			pathc = rompathc;
			pathv = rompathv;
		}

		for (indx = 0; indx < pathc && !found; indx++)
		{
			int chksum = filetype == OSD_FILETYPE_ROM;

			if (!found)
				found = try_fopen(f, pathv[indx], NULL, NULL, file, filename, chksum, 0, 0);
			if (!found)
				found = try_fopen(f, pathv[indx], NULL, NULL, file, filename, chksum, 1, 0);
			if (!found)
				found = try_fopen(f, pathv[indx], game, NULL, file, filename, chksum, 0, 0);
			if (!found)
				found = try_fopen(f, pathv[indx], game, NULL, file, filename, chksum, 1, 0);
			if (!found)
				found = try_fopen(f, pathv[indx], NULL, NULL, game, filename, chksum, 0, 0);
			if (!found)
				found = try_fopen(f, pathv[indx], NULL, NULL, game, filename, chksum, 1, 0);
			if (!found)
				found = try_fopen(f, pathv[indx], game, NULL, game, filename, chksum, 0, 0);
			if (!found)
				found = try_fopen(f, pathv[indx], game, NULL, game, filename, chksum, 1, 0);
			if (!found)
			{
				/* try with a .zip directory (if ZipMagic is installed) */
				sprintf(name, "%s.zip", game);
				found = try_fopen(f, pathv[indx], name, NULL, file, filename, chksum, 0, 0);
			}
			extension = strrchr(filename, '.');
			if (extension &&
				strlen(extension) < sizeof(extd) &&
				strchr(extension, '/') == NULL &&
				strchr(extension,'\\') == NULL)
			{
				/* copy extension */
				strcpy(extd, extension+1);
				if (!found)
					found = try_fopen(f, pathv[indx], extd, NULL, file, filename, chksum, 0, 0);
				if (!found)
					found = try_fopen(f, pathv[indx], extd, NULL, file, filename, chksum, 1, 0);
				if (!found)
					found = try_fopen(f, pathv[indx], extd, game, file, filename, chksum, 0, 0);
				if (!found)
					found = try_fopen(f, pathv[indx], extd, game, file, filename, chksum, 1, 0);
				if (!found)
					found = try_fopen(f, pathv[indx], extd, NULL, game, filename, chksum, 0, 0);
				if (!found)
					found = try_fopen(f, pathv[indx], extd, NULL, game, filename, chksum, 1, 0);
				if (!found)
					found = try_fopen(f, pathv[indx], extd, game, game, filename, chksum, 0, 0);
				if (!found)
					found = try_fopen(f, pathv[indx], extd, game, game, filename, chksum, 1, 0);
				if (!found)
				{
					/* try with a .zip directory (if ZipMagic is installed) */
					sprintf(name, "%s.zip", game);
					found = try_fopen(f, pathv[indx], extd, name, file, filename, chksum, 0, 0);
				}
			}
		}
		if (!found)
			LOG(("ROM/SAMPLE '%s' NOT FOUND!\n", filename));
		break;
#ifdef MESS
	case OSD_FILETYPE_IMAGE_R:
		/* only for reading */
		if (openforwrite)
		{
			LOG(("osd_fopen: type %02x write not supported\n", filetype));
			break;
		}

		strcpy(file, filename);

		/* NPW 26-Aug-2001 - Adding support for absolute paths */
		if ((filename[0] == '\\') || (filename[1] == ':'))
		{
			char *s;
			char *ext;
			int is_zip;
			ZIP *z;
			struct zipent *ent;

			s = strrchr(file, '\\');
			if (s)
				*(s++) = '\0';
			else
				s = file;

			ext = strrchr(s, '.');
			is_zip = (ext) ? !stricmp(ext, ".zip") : 0;
			if (is_zip)
			{
				if (cache_stat(filename, &stat_buffer) == 0)
				{
					z = openzip(filename);
					if (z)
					{
						ent = readzip(z);
						if (ent)
						{
							if (load_zipped_file(filename, ent->name, &f->data, &f->length) == 0)
							{
								f->type = kZippedFile;
								f->offset = 0;
								f->crc = crc32(0L, f->data, f->length);
								found = 1;
							}
						}
					}
				}
			}
			else
			{
				if (!found)
					found = try_fopen(f, file, NULL, NULL, s, s, 1, 0, 0);
				if (!found)
					found = try_fopen(f, file, NULL, NULL, s, s, 1, 1, 0);
				if (!found)
					found = try_fopen(f, file, NULL, NULL, s, s, 0, 0, 0);
				if (!found)
					found = try_fopen(f, file, NULL, NULL, s, s, 0, 1, 0);
			}
		}
		else
		{
			pathc = softpathc;
			pathv = softpathv;
			LOG(("osd_fopen: using softwarepath (%d directories)\n", pathc));

			LOG(("Open IMAGE_R '%s' for '%s'\n", filename, game));
			do
			{
				for (indx = 0; indx < pathc && !found; indx++)
				{
					if (!found)
						found = try_fopen(f, pathv[indx], game, NULL, filename, filename, 1, 0, 0);
					if (!found)
						found = try_fopen(f, pathv[indx], game, NULL, filename, filename, 1, 1, 0);
					if (!found)
						found = try_fopen(f, pathv[indx], NULL, NULL, filename, filename, 1, 0, 0);
					if (!found)
						found = try_fopen(f, pathv[indx], NULL, NULL, filename, filename, 1, 1, 0);
					if (!found)
						found = try_fopen(f, pathv[indx], game, NULL, filename, filename, 0, 0, 0);
					if (!found)
						found = try_fopen(f, pathv[indx], game, NULL, filename, filename, 0, 1, 0);
					if (!found)
						found = try_fopen(f, pathv[indx], NULL, NULL, filename, filename, 0, 0, 0);
					if (!found)
						found = try_fopen(f, pathv[indx], NULL, NULL, filename, filename, 0, 1, 0);

					extension = strrchr(filename, '.');
					if (extension &&
						strlen(extension) < sizeof(extd) &&
						strchr(extension, '/') == NULL &&
						strchr(extension,'\\') == NULL)
					{
						/* copy extension */
						strcpy(extd, extension+1);
						if (!found)
							found = try_fopen(f, pathv[indx], extd, game, filename, filename, 1, 0, 0);
						if (!found)
							found = try_fopen(f, pathv[indx], extd, game, filename, filename, 1, 1, 0);
						if (!found)
							found = try_fopen(f, pathv[indx], extd, NULL, filename, filename, 1, 0, 0);
						if (!found)
							found = try_fopen(f, pathv[indx], extd, NULL, filename, filename, 1, 1, 0);
						if (!found)
							found = try_fopen(f, pathv[indx], extd, game, filename, filename, 0, 0, 0);
						if (!found)
							found = try_fopen(f, pathv[indx], extd, game, filename, filename, 0, 1, 0);
						if (!found)
							found = try_fopen(f, pathv[indx], extd, NULL, filename, filename, 0, 0, 0);
						if (!found)
							found = try_fopen(f, pathv[indx], extd, NULL, filename, filename, 0, 1, 0);
					}
				}

				extension = strrchr(file, '.');
				if (extension)
					*extension = '\0';

			} while (!found && extension);
		}

		if (!found)
			LOG(("IMAGE_R '%s' NOT FOUND!\n", filename));
		break;

	case OSD_FILETYPE_IMAGE_RW:
		LOG(("Open IMAGE_RW '%s' for '%s' mode %d\n", filename, game, openforwrite));
		strcpy(file, filename);

		pathc = softpathc;
		pathv = softpathv;
		LOG(("osd_fopen: using softwarepath (%d directories)\n", pathc));

		do
		{
			for (indx = 0; indx < pathc && !found; indx++)
			{
				if (!found)
					found = try_fopen(f, pathv[indx], game, NULL, file, filename, 0, 0, openforwrite);
				if (!found && !openforwrite)
					found = try_fopen(f, pathv[indx], game, NULL, file, filename, 0, 1, 0);
				if (!found)
					found = try_fopen(f, pathv[indx], NULL, NULL, file, filename, 0, 0, openforwrite);
				if (!found && !openforwrite)
					found = try_fopen(f, pathv[indx], NULL, NULL, file, filename, 0, 1, 0);

				if (!found && !openforwrite)
				{
					/* try with a .zip extension */
					sprintf(name, "%s.zip", gamename);
					if (!found)
						found = try_fopen(f, pathv[indx], name, NULL, file, filename, 0, 0, openforwrite);
				}

				extension = strrchr(filename, '.');
				if (extension &&
					strlen(extension) < sizeof(extd) &&
					strchr(extension, '/') == NULL &&
					strchr(extension,'\\') == NULL)
				{
					/* copy extension */
					strcpy(extd, extension+1);
					if (!found)
						found = try_fopen(f, pathv[indx], game, extd, file, filename, 0, 0, openforwrite);
					if (!found && !openforwrite)
						found = try_fopen(f, pathv[indx], game, extd, file, filename, 0, 1, 0);
					if (!found)
						found = try_fopen(f, pathv[indx], extd, NULL, file, filename, 0, 0, openforwrite);
					if (!found && !openforwrite)
						found = try_fopen(f, pathv[indx], extd, NULL, file, filename, 0, 1, 0);

					if (!found && !openforwrite)
					{
						/* try with a .zip extension */
						sprintf(name, "%s.zip", gamename);
						if (!found)
							found = try_fopen(f, pathv[indx], game, extd, file, filename, 0, 0, openforwrite);
					}
				}
			}

			extension = strrchr(file, '.');
			if (extension)
				*extension = '\0';
		} while (!found && extension);
		if (!found)
			LOG(("IMAGE_RW '%s' NOT FOUND!\n", filename));
		break;
#endif			/* MESS */
	case OSD_FILETYPE_NVRAM:
		if (!found)
		{
			sprintf(name, "%s/%s.nv", nvdir, gamename);
			f->type = kPlainFile;
			f->file = fopen(name, openforwrite ? "wb" : "rb");
			found = f->file != 0;
		}

		if (!found)
		{
			/* try with a .zip directory (if ZipMagic is installed) */
			sprintf(name, "%s.zip/%s.nv", nvdir, gamename);
			f->type = kPlainFile;
			f->file = fopen(name, openforwrite ? "wb" : "rb");
			found = f->file != 0;
		}

		if (!found)
		{
			/* try with a .zif directory (if ZipFolders is installed) */
			sprintf(name, "%s.zif/%s.nv", nvdir, gamename);
			f->type = kPlainFile;
			f->file = fopen(name, openforwrite ? "wb" : "rb");
			found = f->file != 0;
		}
		break;

	case OSD_FILETYPE_HIGHSCORE:
		if (mame_highscore_enabled())
		{
			if (!found)
			{
				sprintf(name, "%s/%s.hi", hidir, gamename);
				f->type = kPlainFile;
				f->file = fopen(name, openforwrite ? "wb" : "rb");
				found = f->file != 0;
			}

			if (!found)
			{
				/* try with a .zip directory (if ZipMagic is installed) */
				sprintf(name, "%s.zip/%s.hi", hidir, gamename);
				f->type = kPlainFile;
				f->file = fopen(name, openforwrite ? "wb" : "rb");
				found = f->file != 0;
			}

			if (!found)
			{
				/* try with a .zif directory (if ZipFolders is installed) */
				sprintf(name, "%s.zif/%s.hi", hidir, gamename);
				f->type = kPlainFile;
				f->file = fopen(name, openforwrite ? "wb" : "rb");
				found = f->file != 0;
			}
		}
		break;

	case OSD_FILETYPE_CONFIG:
		sprintf(name, "%s/%s.cfg", cfgdir, gamename);
		f->type = kPlainFile;
		f->file = fopen(name, openforwrite ? "wb" : "rb");
		found = f->file != 0;

		if (!found)
		{
			/* try with a .zip directory (if ZipMagic is installed) */
			sprintf(name, "%s.zip/%s.cfg", cfgdir, gamename);
			f->type = kPlainFile;
			f->file = fopen(name, openforwrite ? "wb" : "rb");
			found = f->file != 0;
		}

		if (!found)
		{
			/* try with a .zif directory (if ZipFolders is installed) */
			sprintf(name, "%s.zif/%s.cfg", cfgdir, gamename);
			f->type = kPlainFile;
			f->file = fopen(name, openforwrite ? "wb" : "rb");
			found = f->file != 0;
		}
		break;

	case OSD_FILETYPE_INPUTLOG:
		sprintf(name, "%s/%s.inp", inpdir, gamename);
		f->type = kPlainFile;
		f->file = fopen(name, openforwrite ? "wb" : "rb");
		found = f->file != 0;

		if (!found)
		{
			/* try with a .zip directory (if ZipMagic is installed) */
			sprintf(name, "%s.zip/%s.cfg", inpdir, gamename);
			f->type = kPlainFile;
			f->file = fopen(name, openforwrite ? "wb" : "rb");
			found = f->file != 0;
		}

		if (!found)
		{
			/* try with a .zif directory (if ZipFolders is installed) */
			sprintf(name, "%s.zif/%s.cfg", inpdir, gamename);
			f->type = kPlainFile;
			f->file = fopen(name, openforwrite ? "wb" : "rb");
			found = f->file != 0;
		}

		if (!openforwrite)
		{
			sprintf(file, "%s.inp", gamename);
			sprintf(name, "%s/%s.zip", inpdir, gamename);
			LOG(("Trying '%s' in '%s'\n", file, name));
			if (cache_stat(name, &stat_buffer) == 0)
			{
				if (load_zipped_file(name, file, &f->data, &f->length) == 0)
				{
					LOG(("Using (osd_fopen) zip file %s for %s\n", name, file));
					f->type = kZippedFile;
					f->offset = 0;
					found = 1;
				}
			}
		}

		break;

	case OSD_FILETYPE_STATE:
		sprintf(name, "%s/%s.sta", stadir, gamename);
		f->file = fopen(name, openforwrite ? "wb" : "rb");
		found = !(f->file == 0);
		if (!found)
		{
			/* try with a .zip directory (if ZipMagic is installed) */
			sprintf(name, "%s.zip/%s.sta", stadir, gamename);
			f->file = fopen(name, openforwrite ? "wb" : "rb");
			found = !(f->file == 0);
		}
		if (!found)
		{
			/* try with a .zif directory (if ZipFolders is installed) */
			sprintf(name, "%s.zif/%s.sta", stadir, gamename);
			f->file = fopen(name, openforwrite ? "wb" : "rb");
			found = !(f->file == 0);
		}
		break;

	case OSD_FILETYPE_ARTWORK:
		/* only for reading */
		if (openforwrite)
		{
			LOG(("osd_fopen: type %02x write not supported\n", filetype));
			break;
		}
		sprintf(name, "%s/%s", artworkdir, filename);
		f->type = kPlainFile;
		f->file = fopen(name, openforwrite ? "wb" : "rb");
		found = f->file != 0;
		if (!found)
		{
			/* try with a .zip directory (if ZipMagic is installed) */
			sprintf(name, "%s.zip/%s.png", artworkdir, filename);
			f->type = kPlainFile;
			f->file = fopen(name, openforwrite ? "wb" : "rb");
			found = f->file != 0;
		}

		if (!found)
		{
			/* try with a .zif directory (if ZipFolders is installed) */
			sprintf(name, "%s.zif/%s.png", artworkdir, filename);
			f->type = kPlainFile;
			f->file = fopen(name, openforwrite ? "wb" : "rb");
			found = f->file != 0;
		}

		if (!found)
		{
			sprintf(file, "%s", filename);
			sprintf(name, "%s/%s", artworkdir, filename);
			extension = strrchr(name, '.');
			if (extension)
				strcpy(extension, ".zip");
			else
				strcat(name, ".zip");
			LOG(("Trying %s in %s\n", file, name));
			if (cache_stat(name, &stat_buffer) == 0)
			{
				if (load_zipped_file(name, file, &f->data, &f->length) == 0)
				{
					LOG(("Using (osd_fopen) zip file %s\n", name));
					f->type = kZippedFile;
					f->offset = 0;
					found = 1;
				}
			}
			if (!found)
			{
				sprintf(name, "%s/%s.zip", artworkdir, game);
				LOG(("Trying %s in %s\n", file, name));
				if (cache_stat(name, &stat_buffer) == 0)
				{
					if (load_zipped_file(name, file, &f->data, &f->length) == 0)
					{
						LOG(("Using (osd_fopen) zip file %s\n", name));
						f->type = kZippedFile;
						f->offset = 0;
						found = 1;
					}
				}
			}
		}
		break;

	case OSD_FILETYPE_MEMCARD:
		sprintf(name, "%s/%s", memcarddir, filename);
		f->type = kPlainFile;
		f->file = fopen(name, openforwrite ? "wb" : "rb");
		found = f->file != 0;
		break;

	case OSD_FILETYPE_SCREENSHOT:
		/* only for writing */
		if (!openforwrite)
		{
			LOG(("osd_fopen: type %02x read not supported\n", filetype));
			break;
		}

		sprintf(name, "%s/%s.png", screenshotdir, filename);
		f->type = kPlainFile;
		f->file = fopen(name, openforwrite ? "wb" : "rb");
		found = f->file != 0;
		break;

	case OSD_FILETYPE_HIGHSCORE_DB:
	case OSD_FILETYPE_HISTORY:
		/* only for reading */
		if (openforwrite)
		{
			LOG(("osd_fopen: type %02x write not supported\n", filetype));
			break;
		}
		f->type = kPlainFile;
		/* open as BINARY file to avoid platform problems */
		f->file = fopen(filename, "rb");
		found = f->file != 0;
		break;

		/* Steph */
	case OSD_FILETYPE_CHEAT:
		sprintf(name, "%s/%s", cheatdir, filename);
		f->type = kPlainFile;
		/* open as ASCII files, not binary like the others */
		f->file = fopen(filename, openforwrite ? "a" : "r");
		found = f->file != 0;
		break;

	case OSD_FILETYPE_LANGUAGE:
		/* only for reading */
		if (openforwrite)
		{
			LOG(("osd_fopen: type %02x write not supported\n", filetype));
			break;
		}
		sprintf(name, "%s.lng", filename);
		f->type = kPlainFile;
		/* open as ASCII files, not binary like the others */
		f->file = fopen(name, openforwrite ? "w" : "r");
		found = f->file != 0;
		LOG(("fopen %s = %08x\n", name, (int) f->file));
		break;

	default:
		LOG(("osd_fopen(): unknown filetype %02x\n", filetype));
	}

	if (!found)
	{
		free(f);
		return 0;
	}

	return f;
}

/* JB 980920 update */
int osd_fread (void *file, void *buffer, int length)
{
	FakeFileHandle *f = (FakeFileHandle *) file;

	switch( f->type )
	{
	case kPlainFile:
		return fread (buffer, 1, length, f->file);
		break;
	case kZippedFile:
	case kRAMFile:
		/* reading from the RAM image of a file */
		if( f->data )
		{
			if ((f->eof=(length + f->offset > f->length)) )
				length = f->length - f->offset;
			memcpy (buffer, f->offset + f->data, length);
			f->offset += length;
			return length;
		}
		break;
	}

	return 0;
}

int osd_fread_swap (void *file, void *buffer, int length)
{
	int i;
	unsigned char *buf;
	unsigned char temp;
	int res;


	res = osd_fread (file, buffer, length);

	buf = buffer;
	for( i = 0; i < length; i += 2 )
	{
		temp = buf[i];
		buf[i] = buf[i + 1];
		buf[i + 1] = temp;
	}

	return res;
}


/* AM 980919 update */
int osd_fwrite (void *file, const void *buffer, int length)
{
	FakeFileHandle *f = (FakeFileHandle *) file;

	switch( f->type )
	{
	case kPlainFile:
		return fwrite (buffer, 1, length, ((FakeFileHandle *) file)->file);
	default:
		return 0;
	}
}

int osd_fwrite_swap (void *file, const void *buffer, int length)
{
	int i;
	unsigned char *buf;
	unsigned char temp;
	int res;


	buf = (unsigned char *) buffer;
	for( i = 0; i < length; i += 2 )
	{
		temp = buf[i];
		buf[i] = buf[i + 1];
		buf[i + 1] = temp;
	}

	res = osd_fwrite (file, buffer, length);

	for( i = 0; i < length; i += 2 )
	{
		temp = buf[i];
		buf[i] = buf[i + 1];
		buf[i + 1] = temp;
	}

	return res;
}

int osd_fread_scatter (void *file, void *buffer, int length, int increment)
{
	unsigned char *buf = buffer;
	FakeFileHandle *f = (FakeFileHandle *) file;
	unsigned char tempbuf[4096];
	int totread, r, i;

	switch( f->type )
	{
	case kPlainFile:
		totread = 0;
		while (length)
		{
			r = length;
			if( r > 4096 )
				r = 4096;
			r = fread (tempbuf, 1, r, f->file);
			if( r == 0 )
				return totread;		   /* error */
			for( i = 0; i < r; i++ )
			{
				*buf = tempbuf[i];
				buf += increment;
			}
			totread += r;
			length -= r;
		}
		return totread;
		break;
	case kZippedFile:
	case kRAMFile:
		/* reading from the RAM image of a file */
		if( f->data )
		{
			if( length + f->offset > f->length )
				length = f->length - f->offset;
			for( i = 0; i < length; i++ )
			{
				*buf = f->data[f->offset + i];
				buf += increment;
			}
			f->offset += length;
			return length;
		}
		break;
	}

	return 0;
}


/* JB 980920 update */
int osd_fseek (void *file, int offset, int whence)
{
	FakeFileHandle *f = (FakeFileHandle *) file;
	int err = 0;

	switch( f->type )
	{
	case kPlainFile:
		return fseek (f->file, offset, whence);
		break;
	case kZippedFile:
	case kRAMFile:
		/* seeking within the RAM image of a file */
		switch( whence )
		{
		case SEEK_SET:
			f->offset = offset;
			break;
		case SEEK_CUR:
			f->offset += offset;
			break;
		case SEEK_END:
			f->offset = f->length + offset;
			break;
		}
		break;
	}

	return err;
}

/* JB 980920 update */
void osd_fclose (void *file)
{
	FakeFileHandle *f = (FakeFileHandle *) file;

	switch( f->type )
	{
	case kPlainFile:
		fclose (f->file);
		break;
	case kZippedFile:
	case kRAMFile:
		if( f->data )
			free (f->data);
		break;
	}
	free (f);
}

/* JB 980920 update */
/* AM 980919 */
static int checksum_file (const char *file, unsigned char **p, unsigned int *size, unsigned int *crc)
{
	int length;
	unsigned char *data;
	FILE *f;

	f = fopen (file, "rb");
	if( !f )
		return -1;

	/* determine length of file */
	if( fseek (f, 0L, SEEK_END) != 0 )
	{
		fclose (f);
		return -1;
	}

	length = ftell (f);
	if( length == -1L )
	{
		fclose (f);
		return -1;
	}

	/* allocate space for entire file */
	data = (unsigned char *) malloc (length);
	if( !data )
	{
		fclose (f);
		return -1;
	}

	/* read entire file into memory */
	if( fseek (f, 0L, SEEK_SET) != 0 )
	{
		free (data);
		fclose (f);
		return -1;
	}

	if( fread (data, sizeof (unsigned char), length, f) != length )
	{
		free (data);
		fclose (f);
		return -1;
	}

	*size = length;
	*crc = crc32 (0L, data, length);
	if( p )
		*p = data;
	else
		free (data);

	fclose (f);

	return 0;
}

/* JB 980920 updated */
/* AM 980919 updated */
int osd_fchecksum (const char *game, const char *filename, unsigned int *length, unsigned int *sum)
{
	char name[256];
	int indx;
	struct stat stat_buffer;
	int found = 0;
	const char *gamename = game;

	decompose_paths_if_needed();

	/* Support "-romdir" yuck. */
	if( alternate_name )
		gamename = alternate_name;

	for( indx = 0; indx < rompathc && !found; ++indx )
	{
		const char *dir_name = rompathv[indx];

		if( !found )
		{
			sprintf (name, "%s/%s", dir_name, gamename);
			if( cache_stat (name, &stat_buffer) == 0 && (stat_buffer.st_mode & S_IFDIR) )
			{
				sprintf (name, "%s/%s/%s", dir_name, gamename, filename);
				if( checksum_file (name, 0, length, sum) == 0 )
                {
					found = 1;
                }
			}
		}

		if( !found )
		{
			/* try with a .zip extension */
			sprintf (name, "%s/%s.zip", dir_name, gamename);
			if( cache_stat (name, &stat_buffer) == 0 )
			{
				if( checksum_zipped_file (name, filename, length, sum) == 0 )
				{
					LOG(("Using (osd_fchecksum) zip file for %s\n", filename));
					found = 1;
				}
			}
		}

		if( !found )
		{
			/* try with a .zif directory (if ZipFolders is installed) */
			sprintf (name, "%s/%s.zif", dir_name, gamename);
			if( cache_stat (name, &stat_buffer) == 0 )
			{
				sprintf (name, "%s/%s.zif/%s", dir_name, gamename, filename);
				if( checksum_file (name, 0, length, sum) == 0 )
				{
					found = 1;
				}
			}
		}
	}

	if( !found )
		return -1;

	return 0;
}

/* JB 980920 */
int osd_fsize (void *file)
{
	FakeFileHandle *f = (FakeFileHandle *) file;

	if( f->type == kRAMFile || f->type == kZippedFile )
		return f->length;

	if( f->file )
	{
		int size, offs;
		offs = ftell( f->file );
		fseek( f->file, 0, SEEK_END );
		size = ftell( f->file );
		fseek( f->file, offs, SEEK_SET );
		return size;
	}

    return 0;
}

/* JB 980920 */
unsigned int osd_fcrc (void *file)
{
	FakeFileHandle *f = (FakeFileHandle *) file;

	return f->crc;
}

int osd_fgetc(void *file)
{
	FakeFileHandle *f = (FakeFileHandle *) file;

	if (f->type == kPlainFile && f->file)
		return fgetc(f->file);
	else
		return EOF;
}

int osd_ungetc(int c, void *file)
{
	FakeFileHandle *f = (FakeFileHandle *) file;

	if (f->type == kPlainFile && f->file)
		return ungetc(c,f->file);
	else
		return EOF;
}

char *osd_fgets(char *s, int n, void *file)
{
	FakeFileHandle *f = (FakeFileHandle *) file;

	if (f->type == kPlainFile && f->file)
		return fgets(s,n,f->file);
	else
		return NULL;
}

int osd_feof(void *file)
{
	FakeFileHandle *f = (FakeFileHandle *) file;

	if (f->type == kPlainFile && f->file)
		return feof(f->file);
	else if (f->type == kRAMFile)
	{
		if (!f->data) return 1;
		return f->eof;
	}
	else
		return 1;
}

int osd_ftell(void *file)
{
	FakeFileHandle *f = (FakeFileHandle *) file;

	if (f->type == kPlainFile && f->file)
		return ftell(f->file);
	else
		return -1L;
}

char *osd_basename (char *filename)
{
	char *c;

	if (!filename)
		return NULL;

	c = filename + strlen(filename);

	while (c != filename)
	{
		c--;
		if (*c == '\\' || *c == '/' || *c == ':')
			return (c+1);
	}

	return filename;
}

char *osd_dirname (char *filename)
{
	char *dirname;
	char *c;
	int found = 0;

	if (!filename)
		return NULL;

	if ( !( dirname = malloc(strlen(filename)+1) ) )
	{
		fprintf(stderr, "error: malloc failed in osd_dirname\n");
		return 0;
	}

	strcpy (dirname, filename);

	c = dirname + strlen(dirname);
	while (c != dirname)
	{
		--c;
		if (*c == '\\' || *c == '/' || *c == ':')
		{
			*(c+1)=0;
			found = 1;
			break;
		}
	}

	/* did we find a path seperator? */
	if (!found)
		dirname[0]=0;

	return dirname;
}

char *osd_strip_extension (char *filename)
{
	char *newname;
	char *c;

	if (!filename)
		return NULL;

	if ( !( newname = malloc(strlen(filename)+1) ) )
	{
		fprintf(stderr, "error: malloc failed in osd_newname\n");
		return 0;
	}

	strcpy (newname, filename);

	c = newname + strlen(newname);
	while (c != newname)
	{
		--c;
		if (*c == '.')
			*c = 0;
		if (*c == '\\' || *c == '/' || *c == ':')
			break;
	}

	return newname;
}


/* called while loading ROMs. It is called a last time with name == 0 to signal */
/* that the ROM loading process is finished. */
/* return non-zero to abort loading */
int osd_display_loading_rom_message (const char *name, int current, int total)
{
	if( name )
		fprintf (stdout, "loading %-12s\r", name);
	else
		fprintf (stdout, "                    \r");
	fflush (stdout);

//	if( keyboard_pressed (KEYCODE_LCONTROL) && keyboard_pressed (KEYCODE_C) )
//		return 1;

	return 0;
}
