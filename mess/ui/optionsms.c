#define WIN32_LEAN_AND_MEAN

static int default_mess_column_width[] = { 186, 230, 88, 84, 84, 68 };
static int default_mess_column_shown[] = {   1,   1,  1,  1,  1,  1 };
static int default_mess_column_order[] = {   0,   1,  2,  3,  4,  5 };

static void MessColumnEncodeString(void* data, char *str);
static void MessColumnDecodeString(const char* str, void* data);
static void MessColumnDecodeWidths(const char* str, void* data);

#include "ui/options.c"

static void MessColumnEncodeString(void* data, char *str)
{
	ColumnEncodeStringWithCount(data, str, MESS_COLUMN_MAX);
}

static void MessColumnDecodeString(const char* str, void* data)
{
	ColumnDecodeStringWithCount(str, data, MESS_COLUMN_MAX);
}

static void MessColumnDecodeWidths(const char* str, void* data)
{
	if (settings.view == VIEW_REPORT || settings.view == VIEW_GROUPED)
		MessColumnDecodeString(str, data);
}

void SetMessColumnWidths(int width[])
{
    int i;

	assert((sizeof(default_mess_column_width) / sizeof(default_mess_column_width[0])) == MESS_COLUMN_MAX);
	assert((sizeof(default_mess_column_shown) / sizeof(default_mess_column_shown[0])) == MESS_COLUMN_MAX);
	assert((sizeof(default_mess_column_order) / sizeof(default_mess_column_order[0])) == MESS_COLUMN_MAX);

    for (i=0; i < MESS_COLUMN_MAX; i++)
        settings.mess_column_width[i] = width[i];
}

void GetMessColumnWidths(int width[])
{
    int i;

    for (i=0; i < MESS_COLUMN_MAX; i++)
        width[i] = settings.mess_column_width[i];
}

void SetMessColumnOrder(int order[])
{
    int i;

    for (i = 0; i < MESS_COLUMN_MAX; i++)
        settings.mess_column_order[i] = order[i];
}

void GetMessColumnOrder(int order[])
{
    int i;

    for (i = 0; i < MESS_COLUMN_MAX; i++)
        order[i] = settings.mess_column_order[i];
}

void SetMessColumnShown(int shown[])
{
    int i;

    for (i = 0; i < MESS_COLUMN_MAX; i++)
        settings.mess_column_shown[i] = shown[i];
}

void GetMessColumnShown(int shown[])
{
    int i;

    for (i = 0; i < MESS_COLUMN_MAX; i++)
        shown[i] = settings.mess_column_shown[i];
}

const char* GetSoftwareDirs(void)
{
    return settings.softwaredirs;
}

void SetSoftwareDirs(const char* paths)
{
	FreeIfAllocated(&settings.softwaredirs);
    if (paths != NULL)
        settings.softwaredirs = strdup(paths);
}

const char *GetCrcDir(void)
{
	return settings.crcdir;
}

void SetCrcDir(const char *crcdir)
{
	FreeIfAllocated(&settings.crcdir);
    if (crcdir != NULL)
        settings.crcdir = strdup(crcdir);
}

BOOL GetUseNewUI(int num_game)
{
    assert(0 <= num_game && num_game < num_games);

    return game_options[num_game].use_new_ui;
}

void SetSelectedSoftware(int driver_index, int devtype, const char *software)
{
	char *newsoftware;
	newsoftware = strdup(software ? software : "");
	if (!newsoftware)
		return;
	FreeIfAllocated(&game_options[driver_index].software[devtype]);
	game_options[driver_index].software[devtype] = newsoftware;
}

const char *GetSelectedSoftware(int driver_index, int devtype)
{
	const char *software;
	software = game_options[driver_index].software[devtype];
	return software ? software : "";
}

void SetExtraSoftwarePaths(int driver_index, const char *extra_paths)
{
	char *new_extra_paths = NULL;

	assert(driver_index >= 0);
	assert(driver_index < num_games);

	if (extra_paths && *extra_paths)
	{
		new_extra_paths = strdup(extra_paths);
		if (!new_extra_paths)
			return;
	}
	FreeIfAllocated(&game_variables[driver_index].extra_software_paths);
	game_variables[driver_index].extra_software_paths = new_extra_paths;
}

const char *GetExtraSoftwarePaths(int driver_index)
{
	const char *paths;

	assert(driver_index >= 0);
	assert(driver_index < num_games);

	paths = game_variables[driver_index].extra_software_paths;
	return paths ? paths : "";
}


