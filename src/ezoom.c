/**
 * Compiz Enhanced zoom
 *
 * Copyright (c) 2007 Kristian Lyngstøl <kristian@compiz-project.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *
 * If you are going to modify this plugin, add your name, mail and what
 * you did to this header. 
 *
 *
 * This will be an enhanced zoom plugin.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrender.h>
#include <compiz.h>

#include "ezoom_options.h"

#define GET_EZ_DISPLAY(d)                            \
	((ezoomDisplay *) (d)->privates[displayPrivateIndex].ptr)
#define EZ_DISPLAY(d)                                \
    ezoomDisplay *ad = GET_EZ_DISPLAY (d)
#define GET_EZ_SCREEN(s, ad)                         \
	((ezoomScreen *) (s)->privates[(ad)->screenPrivateIndex].ptr)
#define EZ_SCREEN(s)                                 \
	ezoomScreen *as = GET_EZ_SCREEN (s, GET_EZ_DISPLAY (s->display))
#define GET_EZ_WINDOW(w, as) \
	((ezoomWindow *) (w)->privates[ (as)->windowPrivateIndex].ptr)
#define EZ_WINDOW(w) \
	ezoomWindow *aw = GET_EZ_WINDOW (w, GET_EZ_SCREEN  (w->screen, GET_EZ_DISPLAY (w->screen->display)))

static int displayPrivateIndex = 0;

typedef struct _ezoomDisplay
{
	int screenPrivateIndex;
	HandleEventProc handleEvent;
} ezoomDisplay;

typedef struct _ezoomScreen
{
	int windowPrivateIndex;
} ezoomScreen;

/* Configuration, initialization, boring stuff. ----------------------- */

static Bool ezoomInitScreen(CompPlugin * p, CompScreen * s)
{
	EZ_DISPLAY(s->display);
	ezoomScreen *as = (ezoomScreen*) malloc(sizeof(ezoomScreen));
	as->windowPrivateIndex = allocateWindowPrivateIndex(s);
	if (as->windowPrivateIndex < 0) {
		free(as);
		return FALSE;
	}
	s->privates[ad->screenPrivateIndex].ptr = as;
	return TRUE;
}

static void ezoomFiniScreen(CompPlugin * p, CompScreen * s)
{
	EZ_SCREEN(s);
	free(as);
}

static Bool ezoomInitDisplay(CompPlugin * p, CompDisplay * d)
{
	ezoomDisplay *ad = (ezoomDisplay *) malloc(sizeof(ezoomDisplay));
	ad->screenPrivateIndex = allocateScreenPrivateIndex(d);
	if (ad->screenPrivateIndex < 0) {
		free(ad);
		return FALSE;
	}
	d->privates[displayPrivateIndex].ptr = ad;
	return TRUE;
}

static void ezoomFiniDisplay(CompPlugin * p, CompDisplay * d)
{
	EZ_DISPLAY(d);
	freeScreenPrivateIndex(d, ad->screenPrivateIndex);
	free(ad);
}

static Bool ezoomInit(CompPlugin * p)
{
	displayPrivateIndex = allocateDisplayPrivateIndex();
	if (displayPrivateIndex < 0)
		return FALSE;
	return TRUE;
}

static void ezoomFini(CompPlugin * p)
{
	if (displayPrivateIndex >= 0)
		freeDisplayPrivateIndex(displayPrivateIndex);
}

static int ezoomGetVersion(CompPlugin *p, int version)
{
	return ABIVERSION;
}

CompPluginVTable ezoomVTable = {
	"ezoom",
	N_("ezoom"),
	N_("Enhanced zoom plugin"),
	ezoomGetVersion,
	0,
	ezoomInit,
	ezoomFini,
	ezoomInitDisplay,
	ezoomFiniDisplay,
	ezoomInitScreen,
	ezoomFiniScreen,
	0,
	0,
	0, // ezoomGetDisplayOptions
	0, // ezoomSetDisplayOptions
	0, // ezoomGetScreenOptions,
	0, // ezoomSetScreenOptions,
	0,
	0,
	0,
	0
};

CompPluginVTable *getCompPluginInfo(void)
{
	return &ezoomVTable;
}
