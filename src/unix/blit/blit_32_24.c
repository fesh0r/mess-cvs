#include "blit.h"

/* normal blitting pack the pixels while blitting */
#define FUNC_NAME(name) name##_32_24_direct
#define SRC_DEPTH    32
#define DEST_DEPTH   24
#define RENDER_DEPTH 32
#include "blit_defs.h"
#include "blit_normal_24.h"
#include "blit_undefs.h"

/* effects render to an intermediate 32 bpp sparse buffer and then this buffer
   is packed and blitted to the final destination by this function */
INLINE void blit_line_32_24(unsigned int *src, unsigned int *end, unsigned int *dst)
{
  for(;src<end;dst+=3,src+=4)
  {
    *(dst  ) = (*(src  )    ) | (*(src+1)<<24);
    *(dst+1) = (*(src+1)>> 8) | (*(src+2)<<16);
    *(dst+2) = (*(src+2)>>16) | (*(src+3)<< 8);
  }
}

#define FUNC_NAME(name) name##_32_24_direct
#define SRC_DEPTH    32
#define DEST_DEPTH   24
#define RENDER_DEPTH 32
#define BLIT_LINE_FUNC blit_line_32_24
#include "blit_defs.h"
#include "blit_effect.h"
#include "advance/scale2x.h"
#include "advance/hq2x.h"
#include "blit_undefs.h"
