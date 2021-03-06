/**
 * PLATOTermAmiga - A PLATO Terminal for the Commodore Amiga
 * Based on Steve Peltz's PAD
 *
 * Author: Thomas Cherryhomes <thom.cherryhomes at gmail dot com>
 *
 * screen.h - Display output functions
 */

#include <proto/intuition.h>
#include <proto/dos.h>
#include <intuition/screens.h>
#include <proto/graphics.h>
#include <proto/layers.h>
#include <proto/exec.h>
#include <exec/memory.h>
#include "screen.h"
#include "protocol.h"
#include "scale.h"
#include "font.h"

unsigned char CharWide=8;
unsigned char CharHigh=16;
unsigned char screen_mode;
extern padBool FastText; /* protocol.c */
padPt TTYLoc;
unsigned char FONT_SIZE_X;
unsigned char FONT_SIZE_Y;
unsigned short width;
unsigned short height;
unsigned long current_foreground=1;
unsigned long current_background=0;
padRGB current_foreground_rgb={255,255,255};
padRGB current_background_rgb={0,0,0};
unsigned char fontm23[2048];
unsigned char is_mono=0;

padRGB palette[16];

#define TEXT_BITMAP_W 640
#define TEXT_BITMAP_H 32
struct BitMap* bmText = NULL; /* temporary bitmap for text output. */
struct BitMap* bmGlyph = NULL; /* bitmap for glyph. */

extern void done(void);

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;

struct NewScreen Screen1 = {
  0,0,640,400,4,             /* Screen of 640 x 400 of depth 8 (2^4 = 16 colours)    */
  DETAILPEN, BLOCKPEN,
  HIRES|LACE,                     /* see graphics/view.h for view modes */
  PUBLICSCREEN,              /* Screen types */
  NULL,                      /* Text attributes (use defaults) */
  "PLATOTerm",
  NULL,
  NULL
};
struct Screen *myScreen;

struct Window *myWindow;
struct NewWindow winlayout = {
  0, 0,
  639, 399,
  0,1,
  IDCMP_CLOSEWINDOW|IDCMP_MENUPICK|IDCMP_ACTIVEWINDOW|IDCMP_VANILLAKEY|IDCMP_MOUSEMOVE,
  WFLG_ACTIVATE|WFLG_BACKDROP|WFLG_BORDERLESS|WFLG_GIMMEZEROZERO,
  NULL, NULL,
  "PLATOTerm",
  NULL,NULL,
  640,400,
  640,400,
  CUSTOMSCREEN
};

/**
 * screen_init() - Set up the screen
 */
void screen_init(void)
{
  unsigned char *bp;
  IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 34);

  if (!IntuitionBase)
    done();

  GfxBase = (struct GfxBase *)OpenLibrary("graphics.library",34);

  if (!GfxBase)
    done();

  myScreen = OpenScreen(&Screen1);
  if (!myScreen)
    done();
  winlayout.Screen=myScreen;

  myWindow = OpenWindow(&winlayout);

  if (!myWindow)
    done();

  /* Initialize text staging bitmap. */
  bmText=AllocMem(sizeof(struct BitMap), MEMF_PUBLIC|MEMF_CLEAR);
  if (!bmText)
    done();
  InitBitMap(bmText,1,TEXT_BITMAP_W,TEXT_BITMAP_H);
  bp=AllocRaster(TEXT_BITMAP_W,TEXT_BITMAP_H);
  if (!bp)
    done();
  bmText->Planes[0]=bp;

  /* Initialize glyph bitmap */
  bmGlyph=AllocMem(sizeof(struct BitMap), MEMF_PUBLIC|MEMF_CLEAR);
  if (!bmGlyph)
    done();
  InitBitMap(bmGlyph,1,8,12);
}

/**
 * screen_update_colors() - Set the terminal colors
 */
void screen_update_colors(void)
{
  long i=0;
  for (i=0;i<16;i++)
    {
      unsigned long r=palette[i].red   >> 4;
      unsigned long g=palette[i].green >> 4;
      unsigned long b=palette[i].blue  >> 4;
      SetRGB4(&myScreen->ViewPort,i,r,g,b);
    }
}

/**
 * screen_wait(void) - Sleep for approx 16.67ms
 */
void screen_wait(void)
{
}

/**
 * screen_beep(void) - Beep the terminal
 */
void screen_beep(void)
{
}

/**
 * screen_clear - Clear the screen
 */
void screen_clear(void)
{
  SetRast(myWindow->RPort,current_background);

  palette[0].red=current_background_rgb.red;
  palette[0].green=current_background_rgb.green;
  palette[0].blue=current_background_rgb.blue;

  if ((current_background_rgb.red   != current_foreground_rgb.red) &&
      (current_background_rgb.green != current_foreground_rgb.green) &&
      (current_background_rgb.blue  != current_foreground_rgb.blue))
    {
      palette[1].red=current_foreground_rgb.red;
      palette[1].green=current_foreground_rgb.green;
      palette[1].blue=current_foreground_rgb.blue;
    }

  screen_update_colors();
}

/**
 * screen_set_pen_mode(void) - Set the pen mode
 */
void screen_set_pen_mode(void)
{
  if ((CurMode==ModeErase)||(CurMode==ModeInverse))
    SetAPen(myWindow->RPort,current_background);
  else
    SetAPen(myWindow->RPort,current_foreground);
}

/**
 * screen_block_draw(Coord1, Coord2) - Perform a block fill from Coord1 to Coord2
 */
void screen_block_draw(padPt* Coord1, padPt* Coord2)
{
  screen_set_pen_mode();
  RectFill(myWindow->RPort,
	   scalex[Coord1->x],
	   scaley[Coord1->y],
	   scalex[Coord2->x],
	   scaley[Coord2->y]);
}

/**
 * screen_dot_draw(Coord) - Plot a mode 0 pixel
 */
void screen_dot_draw(padPt* Coord)
{
  screen_set_pen_mode();
  WritePixel(myWindow->RPort,scalex[Coord->x],scaley[Coord->y]);
}

/**
 * screen_line_draw(Coord1, Coord2) - Draw a mode 1 line
 */
void screen_line_draw(padPt* Coord1, padPt* Coord2)
{
  screen_set_pen_mode();
  Move(myWindow->RPort,scalex[Coord1->x],scaley[Coord1->y]);
  Draw(myWindow->RPort,scalex[Coord2->x],scaley[Coord2->y]);
}

/**
 * screen_char_draw(Coord, ch, count) - Output buffer from ch* of length count as PLATO characters
 */
void screen_char_draw(padPt* Coord, unsigned char* ch, unsigned char count)
{
  short offset;
  short x,y;
  unsigned char i=0;
  unsigned short choffset;

  switch(CurMem)
    {
    case M0:
      bmGlyph->Planes[0]=(unsigned char*)font;
      offset=-32;
      break;
    case M1:
      bmGlyph->Planes[0]=(unsigned char*)font;
      offset=64;
      break;
    case M2:
      bmGlyph->Planes[0]=(unsigned char*)fontm23;
      offset=-32;
      break;
    case M3:
      bmGlyph->Planes[0]=(unsigned char*)fontm23;
      offset=32;
      break;
    }

  switch(CurMode)
    {
    case ModeWrite:
      break;
    case ModeRewrite:
      break;
    case ModeErase:
      break;
    case ModeInverse:
      break;
    }

  x=scalex[Coord->x];
  y=scaley[Coord->y]-12;

  for (i=0;i<count;i++)
    {
      choffset=fontptr[ch[i]+offset];
      bmGlyph->Planes[0]=(unsigned char*)font+choffset;
      /* BltBitMap(bmGlyph,0,0,myWindow->RPort->BitMap,x,y,8,12,0xC0,0xFF,NULL); */
      BltBitMapRastPort(bmGlyph,0,0,myWindow->RPort,x,y,8,12,0xC0);
      x+=8;
    }

}

/**
 * screen_tty_char - Called to plot chars when in tty mode
 */
void screen_tty_char(padByte theChar)
{
}

/**
 * screen_foreground - set foreground color
 */
void screen_foreground(padRGB* theColor)
{
}

/**
 * screen_background - set background_color
 */
void screen_background(padRGB* theColor)
{
}

/**
 * screen_paint - Paint the screen
 * Calling convention is different due to this needing to be recursive.
 */
void screen_paint(padPt* Coord)
{
}

/**
 * screen_done()
 * Close down TGI
 */
void screen_done(void)
{
  /* Free the text staging bitmap */
  FreeRaster(bmText->Planes[0],TEXT_BITMAP_W,TEXT_BITMAP_H);
  FreeMem(bmText,sizeof(struct BitMap));
  FreeMem(bmGlyph,sizeof(struct BitMap));
  /* since bmGlyph points to an already in memory raster that wasn't allocated, it is not freed. */
  bmText=NULL;
  bmGlyph=NULL;

  if (myWindow)
    CloseWindow(myWindow);
  if (myScreen)
    CloseScreen(myScreen);

  if (IntuitionBase)
    CloseLibrary((struct Library*)IntuitionBase);
  if (GfxBase)
    CloseLibrary((struct Library*)GfxBase);
}
