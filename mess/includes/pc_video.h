/*********************************************************************

	pc_video.h

	Refactoring of code common to PC video implementations

*********************************************************************/

#ifndef PC_VIDEO_H
#define PC_VIDEO_H

#include "includes/crtc6845.h"

typedef void (*pc_video_update_proc)(struct mame_bitmap *bitmap,
	struct crtc6845 *crtc);

struct crtc6845 *pc_video_start(const struct crtc6845_config *config,
	pc_video_update_proc (*choosevideomode)(int *width, int *height, struct crtc6845 *crtc),
	size_t vramsize);

VIDEO_UPDATE( pc_video );

WRITE_HANDLER( pc_video_videoram_w );
WRITE32_HANDLER( pc_video_videoram32_w );

#endif /* PC_VIDEO_H */
