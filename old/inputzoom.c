/*
 * Beryl Input-Enabled-Zoom-Plugin
 *
 * Copyright : (C) 2006 by Dennis Kasprzyk
 * E-mail    : onestone@beryl-project.org
 *
 * Copyright : (C) 2006 Quinn Storm
 * E-mail    : quinnstorm@beryl-project.org
 *
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
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>

#include <beryl.h>

#define ZOOM_IN_BUTTON_DEFAULT    Button4
#define ZOOM_IN_MODIFIERS_DEFAULT CompSuperMask

#define ZOOM_OUT_BUTTON_DEFAULT    Button5
#define ZOOM_OUT_MODIFIERS_DEFAULT CompSuperMask

#define ZOOM_SPEED_DEFAULT   1.00f
#define ZOOM_SPEED_MIN       0.1f
#define ZOOM_SPEED_MAX       4.0f
#define ZOOM_SPEED_PRECISION 0.1f

#define ZOOM_STEP_DEFAULT   1.25f
#define ZOOM_STEP_MIN       1.05f
#define ZOOM_STEP_MAX       5.0f
#define ZOOM_STEP_PRECISION 0.05f

#define ZOOM_MAX_FACTOR_DEFAULT   16.0f
#define ZOOM_MAX_FACTOR_MIN       1.1f
#define ZOOM_MAX_FACTOR_MAX       64.0f
#define ZOOM_MAX_FACTOR_PRECISION 0.1f

#define ZOOM_TIMESTEP_DEFAULT   1.2f
#define ZOOM_TIMESTEP_MIN       0.1f
#define ZOOM_TIMESTEP_MAX       20.0f
#define ZOOM_TIMESTEP_PRECISION 0.1f

#define ZOOM_MOUSE_UPDATE_MIN     1
#define ZOOM_MOUSE_UPDATE_MAX     500
#define ZOOM_MOUSE_UPDATE_DEFAULT 10

#define ZOOM_FILTER_LINEAR_DEFAULT TRUE

#define ZOOM_HIDE_NORMAL_CURSOR_DEFAULT FALSE
#define ZOOM_SHOW_SCALED_CURSOR_DEFAULT FALSE

#define ZOOM_OUT_ON_CUBE_DEFAULT TRUE

static int displayPrivateIndex;

#define ZOOM_DISPLAY_OPTION_IN         0
#define ZOOM_DISPLAY_OPTION_OUT         1
#define ZOOM_DISPLAY_OPTION_NUM         2

typedef struct _ZoomDisplay
{
	int screenPrivateIndex;
	HandleEventProc handleEvent;

	Bool fixesSupported;
	int fixesEventBase;
	int fixesErrorBase;
	Bool canHideCursor;

	CompOption opt[ZOOM_DISPLAY_OPTION_NUM];
} ZoomDisplay;

#define ZOOM_SCREEN_OPTION_SPEED              0
#define ZOOM_SCREEN_OPTION_STEP               1
#define ZOOM_SCREEN_OPTION_MAX_FACTOR         2
#define ZOOM_SCREEN_OPTION_TIMESTEP           3
#define ZOOM_SCREEN_OPTION_FILTER_LINEAR      4
#define ZOOM_SCREEN_OPTION_MOUSE_UPDATE       5
#define ZOOM_SCREEN_OPTION_OUT_ON_CUBE        6
#define ZOOM_SCREEN_OPTION_NUM                7
//#define ZOOM_SCREEN_OPTION_HIDE_NORMAL_CURSOR 5
//#define ZOOM_SCREEN_OPTION_SHOW_SCALED_CURSOR 6


typedef struct _CursorTexture
{
	Bool isSet;
	GLuint texture;
	int width;
	int height;
	int hotX;
	int hotY;
} CursorTexture;

typedef struct _ZoomScreen
{
	PreparePaintScreenProc preparePaintScreen;
	DonePaintScreenProc donePaintScreen;
	PaintScreenProc paintScreen;
	PaintTransformedScreenProc paintTransformedScreen;
	SetScreenOptionForPluginProc setScreenOptionForPlugin;
	ApplyScreenTransformProc applyScreenTransform;
	SetClipPlanesProc setClipPlanes;

	CompTimeoutHandle mouseTimeout;

	CompOption opt[ZOOM_SCREEN_OPTION_NUM];

	float speed;
	float step;
	float timestep;
	float maxScale;

	GLfloat currentScale;
	GLfloat newScale;
	GLfloat toScale;

	GLfloat zVelocity;

	GLfloat xscale;
	GLfloat yscale;

	Bool active;

	CursorTexture cursor;
	int mouseX;
	int mouseY;
	Bool cursorHidden;
	Bool cursorInfoSelected;
	Bool showScaled;
	Bool hideNormal;
	Bool noTimerUpdate;

	float maxTranslate;
} ZoomScreen;

#define GET_ZOOM_DISPLAY(d)                      \
    ((ZoomDisplay *) (d)->privates[displayPrivateIndex].ptr)

#define ZOOM_DISPLAY(d)                   \
    ZoomDisplay *zd = GET_ZOOM_DISPLAY (d)

#define GET_ZOOM_SCREEN(s, zd)                         \
    ((ZoomScreen *) (s)->privates[(zd)->screenPrivateIndex].ptr)

#define ZOOM_SCREEN(s)                                \
    ZoomScreen *zs = GET_ZOOM_SCREEN (s, GET_ZOOM_DISPLAY (s->display))

#define NUM_OPTIONS(s) (sizeof ((s)->opt) / sizeof (CompOption))

static void zoomSetClipPlanes (CompScreen *s, int output)
{
	ZOOM_SCREEN(s);

	if (zs->active)
	{
		float ox, oy, ow, oh;

		Bool fullscreen = compDisplayGetRequestFlagForPlugin (s->display,
								"inputzoom","fullscreen");
		Bool singlescreen = compDisplayGetRequestFlagForPlugin (s->display,
								"inputzoom","singlescreen");
		
		if (fullscreen && !singlescreen)
		{
			ox = 0;
			oy = 0;
			ow = s->width;
			oh = s->height;
		}
		else
		{
			ox = s->outputDev[output].region.extents.x1;
			oy = s->outputDev[output].region.extents.y1;
			ow = s->outputDev[output].width;
			oh = s->outputDev[output].height;
		}
		// Prevent zoom from going below 1.0 ever -- fixes annoying bounce during zoom cancel
		if (zs->xscale < 1.0)
		{
			zs->zVelocity = 0.0;
			zs->xscale = 1.0;
		}
		if (zs->yscale < 1.0)
		{
			zs->zVelocity = 0.0;
			zs->yscale = 1.0;
		}

		float tx = zs->mouseX - (ox + (ow / 2.0));
		float ty = zs->mouseY - (oy + (oh / 2.0));

		tx /= ow;
		tx = -(tx * (zs->xscale - 1));
		ty /= oh;
		ty = ty * (zs->yscale - 1);

		
		glScalef(1.0/zs->xscale, 1.0/zs->yscale, 1.0f);
		glTranslatef(-tx, -ty, 0.0f);
	}
		
	UNWRAP(zs, s, setClipPlanes);
	(*s->setClipPlanes) (s, output);
	WRAP(zs, s, setClipPlanes, zoomSetClipPlanes);

}

static void updateCursor(Display * dpy, CursorTexture * cursor)
{
	glEnable(GL_TEXTURE_RECTANGLE_ARB);
	if (!cursor->isSet)
	{
		cursor->isSet = TRUE;
		glGenTextures(1, &cursor->texture);
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, cursor->texture);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,
						GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,
						GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,
						GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,
						GL_TEXTURE_WRAP_T, GL_CLAMP);
	}

	XFixesCursorImage *ci = XFixesGetCursorImage(dpy);

	cursor->width = ci->width;
	cursor->height = ci->height;
	cursor->hotX = ci->xhot;
	cursor->hotY = ci->yhot;

	unsigned char *pixels = malloc(ci->width * ci->height * 4);
	int i;

	for (i = 0; i < ci->width * ci->height; i++)
	{
		unsigned long pix = ci->pixels[i];

		pixels[i * 4] = pix & 0xff;
		pixels[(i * 4) + 1] = (pix >> 8) & 0xff;
		pixels[(i * 4) + 2] = (pix >> 16) & 0xff;
		pixels[(i * 4) + 3] = (pix >> 24) & 0xff;
	}

	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, cursor->texture);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, cursor->width,
				 cursor->height, 0, GL_BGRA, GL_UNSIGNED_BYTE, pixels);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
	glDisable(GL_TEXTURE_RECTANGLE_ARB);
	XFree(ci);
	free(pixels);
}

static void freeCursor(CursorTexture * cursor)
{
	if (!cursor->isSet)
		return;
	cursor->isSet = FALSE;
	glDeleteTextures(1, &cursor->texture);
	cursor->texture = 0;
}

static void zoomHandleEvent(CompDisplay * d, XEvent * event)
{
	CompScreen *s;

	ZOOM_DISPLAY(d);

	if (event->type == zd->fixesEventBase + XFixesCursorNotify)
	{
		XFixesCursorNotifyEvent *cev = (XFixesCursorNotifyEvent *) event;

		s = findScreenAtDisplay(d, cev->window);
		if (s)
		{
			ZOOM_SCREEN(s);
			if (zs->cursor.isSet)
				updateCursor(d->display, &zs->cursor);
		}
	}

	UNWRAP(zd, d, handleEvent);
	(*d->handleEvent) (d, event);
	WRAP(zd, d, handleEvent, zoomHandleEvent);
}

static Bool
zoomSetScreenOption(CompScreen * screen, char *name, CompOptionValue * value)
{
	CompOption *o;
	int index;

	ZOOM_SCREEN(screen);

	o = compFindOption(zs->opt, NUM_OPTIONS(zs), name, &index);
	if (!o)
		return FALSE;

	switch (index)
	{
	case ZOOM_SCREEN_OPTION_STEP:
		if (compSetFloatOption(o, value))
		{
			zs->step = o->value.f;
			return TRUE;
		}
		break;
	case ZOOM_SCREEN_OPTION_MAX_FACTOR:
		if (compSetFloatOption(o, value))
		{
			zs->maxScale = o->value.f;
			return TRUE;
		}
		break;
	case ZOOM_SCREEN_OPTION_SPEED:
		if (compSetFloatOption(o, value))
		{
			zs->speed = o->value.f;
			return TRUE;
		}
		break;
	case ZOOM_SCREEN_OPTION_TIMESTEP:
		if (compSetFloatOption(o, value))
		{
			zs->timestep = o->value.f;
			return TRUE;
		}
		break;
	case ZOOM_SCREEN_OPTION_FILTER_LINEAR:
		if (compSetBoolOption(o, value))
			return TRUE;
/*	case ZOOM_SCREEN_OPTION_HIDE_NORMAL_CURSOR:
		if (compSetBoolOption(o, value))
		{
			zs->hideNormal = o->value.b;
			return TRUE;
		}
		break;
	case ZOOM_SCREEN_OPTION_SHOW_SCALED_CURSOR:
		if (compSetBoolOption(o, value))
		{
			zs->showScaled = o->value.b;
			return TRUE;
		}
		break;*/
	case ZOOM_SCREEN_OPTION_MOUSE_UPDATE:
		if (compSetIntOption(o, value))
		{
			return TRUE;
		}
		break;
	case ZOOM_SCREEN_OPTION_OUT_ON_CUBE:
		if (compSetBoolOption(o, value))
		{
			return TRUE;
		}
		break;
	default:
		break;
	}

	return FALSE;
}

static void zoomScreenInitOptions(ZoomScreen * zs)
{
	CompOption *o;

	o = &zs->opt[ZOOM_SCREEN_OPTION_SPEED];
	o->advanced = False;
	o->name = "speed";
	o->group = N_("Misc. options");
	o->subGroup = N_("");
	o->displayHints = "";
	o->shortDesc = N_("Zoom Speed");
	o->longDesc = N_("Zoom Speed.");
	o->type = CompOptionTypeFloat;
	o->value.f = ZOOM_SPEED_DEFAULT;
	o->rest.f.min = ZOOM_SPEED_MIN;
	o->rest.f.max = ZOOM_SPEED_MAX;
	o->rest.f.precision = ZOOM_SPEED_PRECISION;

	o = &zs->opt[ZOOM_SCREEN_OPTION_STEP];
	o->advanced = False;
	o->name = "step";
	o->group = N_("Misc. options");
	o->subGroup = N_("");
	o->displayHints = "";
	o->shortDesc = N_("Step");
	o->longDesc =
			N_
			("Zoomfactor multiplier for amount of Zooming in each ZoomIn/ZoomOut.");
	o->type = CompOptionTypeFloat;
	o->value.f = ZOOM_STEP_DEFAULT;
	o->rest.f.min = ZOOM_STEP_MIN;
	o->rest.f.max = ZOOM_STEP_MAX;
	o->rest.f.precision = ZOOM_STEP_PRECISION;

	o = &zs->opt[ZOOM_SCREEN_OPTION_MAX_FACTOR];
	o->advanced = False;
	o->name = "max";
	o->group = N_("Misc. options");
	o->subGroup = N_("");
	o->displayHints = "";
	o->shortDesc = N_("Maximum Zoom");
	o->longDesc = N_("Maximum Zoom factor.");
	o->type = CompOptionTypeFloat;
	o->value.f = ZOOM_MAX_FACTOR_DEFAULT;
	o->rest.f.min = ZOOM_MAX_FACTOR_MIN;
	o->rest.f.max = ZOOM_MAX_FACTOR_MAX;
	o->rest.f.precision = ZOOM_MAX_FACTOR_PRECISION;

	o = &zs->opt[ZOOM_SCREEN_OPTION_TIMESTEP];
	o->advanced = False;
	o->name = "timestep";
	o->group = N_("Misc. options");
	o->subGroup = N_("");
	o->displayHints = "";
	o->shortDesc = N_("Zoom Timestep");
	o->longDesc = N_("Zoom Timestep.");
	o->type = CompOptionTypeFloat;
	o->value.f = ZOOM_TIMESTEP_DEFAULT;
	o->rest.f.min = ZOOM_TIMESTEP_MIN;
	o->rest.f.max = ZOOM_TIMESTEP_MAX;
	o->rest.f.precision = ZOOM_TIMESTEP_PRECISION;

	o = &zs->opt[ZOOM_SCREEN_OPTION_FILTER_LINEAR];
	o->advanced = False;
	o->name = "filter_linear";
	o->group = N_("Misc. options");
	o->subGroup = N_("Visual quality");
	o->displayHints = "";
	o->shortDesc = N_("Linear Filtering");
	o->longDesc = N_("Use Linear Filter when Zoomed in.");
	o->type = CompOptionTypeBool;
	o->value.b = ZOOM_FILTER_LINEAR_DEFAULT;

/*	o = &zs->opt[ZOOM_SCREEN_OPTION_HIDE_NORMAL_CURSOR];
	o->advanced = False;
	o->name = "hide_normal";
	o->group = N_("Misc. options");
	o->subGroup = N_("Cursor options");
	o->displayHints = "";
	o->shortDesc = N_("Hide Normal Cursor (May cause\nissues when switching window managers).");
	o->longDesc = N_("Hide Normal Cursor during Zoom.");
	o->type = CompOptionTypeBool;
	o->value.b = ZOOM_HIDE_NORMAL_CURSOR_DEFAULT;

	o = &zs->opt[ZOOM_SCREEN_OPTION_SHOW_SCALED_CURSOR];
	o->advanced = False;
	o->name = "show_scaled";
	o->group = N_("Misc. options");
	o->subGroup = N_("Cursor options");
	o->displayHints = "";
	o->shortDesc = N_("Show Scaled Cursor");
	o->longDesc = N_("Show Scaled Cursor during Zoom.");
	o->type = CompOptionTypeBool;
	o->value.b = ZOOM_SHOW_SCALED_CURSOR_DEFAULT;*/

	o = &zs->opt[ZOOM_SCREEN_OPTION_MOUSE_UPDATE];
	o->advanced = True;
	o->name = "mouse_update";
	o->group = N_("Misc. options");
	o->subGroup = N_("");
	o->displayHints = "";
	o->shortDesc = N_("Mouse Update Interval");
	o->longDesc = N_("Mouse position Update Interval.");
	o->type = CompOptionTypeInt;
	o->value.i = ZOOM_MOUSE_UPDATE_DEFAULT;
	o->rest.i.min = ZOOM_MOUSE_UPDATE_MIN;
	o->rest.i.max = ZOOM_MOUSE_UPDATE_MAX;

	o = &zs->opt[ZOOM_SCREEN_OPTION_OUT_ON_CUBE];
	o->advanced = False;
	o->name = "zoomout_on_cube";
	o->group = N_("Misc. options");
	o->subGroup = N_("");
	o->displayHints = "";
	o->shortDesc = N_("Zoom Out on Cube");
	o->longDesc = N_("Zoom Out if the Cube gets activated.");
	o->type = CompOptionTypeBool;
	o->value.b = ZOOM_OUT_ON_CUBE_DEFAULT;
}
static CompOption *zoomGetScreenOptions(CompScreen * screen, int *count)
{
	if (screen)
	{
		ZOOM_SCREEN(screen);

		*count = NUM_OPTIONS(zs);
		return zs->opt;
	}
	else
	{
		ZoomScreen *zs = malloc(sizeof(ZoomScreen));

		zoomScreenInitOptions(zs);
		*count = NUM_OPTIONS(zs);
		return zs->opt;
	}
}


static int adjustZoomVelocity(ZoomScreen * zs)
{
	float d, adjust, amount;

	d = (zs->toScale - zs->currentScale) * 75.0f;

	adjust = d * 0.002f;
	amount = fabs(d);
	if (amount < 1.0f)
		amount = 1.0f;
	else if (amount > 5.0f)
		amount = 5.0f;

	zs->zVelocity = (amount * zs->zVelocity + adjust) / (amount + 1.0f);

	return (fabs(d) < 0.1f && fabs(zs->zVelocity) < 0.005f);
}

static void
zoomApplyScreenTransform(CompScreen * s,
						 const ScreenPaintAttrib * sAttrib, int output,
						 CompTransform *transform)
{
	ZOOM_SCREEN(s);

	UNWRAP(zs, s, applyScreenTransform);
	(*s->applyScreenTransform) (s, sAttrib, output, transform);
	WRAP(zs, s, applyScreenTransform, zoomApplyScreenTransform);

	Bool *pCaps = (Bool *) IPCS_GetVPtrND(IPCS_OBJECT(s),
										  "CUBE_PAINTING_CAPS_BOOL_PTR",
										  NULL);

	if (zs->active && (!pCaps || !(*pCaps)))
	{
		float ox, oy, ow, oh;

		Bool fullscreen = compDisplayGetRequestFlagForPlugin (s->display,
								"inputzoom","fullscreen");
		Bool singlescreen = compDisplayGetRequestFlagForPlugin (s->display,
								"inputzoom","singlescreen");
		
		if (fullscreen && !singlescreen)
		{
			ox = 0;
			oy = 0;
			ow = s->width;
			oh = s->height;
		}
		else
		{
			ox = s->outputDev[output].region.extents.x1;
			oy = s->outputDev[output].region.extents.y1;
			ow = s->outputDev[output].width;
			oh = s->outputDev[output].height;
		}
		// Prevent zoom from going below 1.0 ever -- fixes annoying bounce during zoom cancel
		if (zs->xscale < 1.0)
		{
			zs->zVelocity = 0.0;
			zs->xscale = 1.0;
		}
		if (zs->yscale < 1.0)
		{
			zs->zVelocity = 0.0;
			zs->yscale = 1.0;
		}

		float tx = zs->mouseX - (ox + (ow / 2.0));
		float ty = zs->mouseY - (oy + (oh / 2.0));

		tx /= ow;
		tx = -(tx * (zs->xscale - 1));
		ty /= oh;
		ty = ty * (zs->yscale - 1);

		matrixTranslate(transform, tx, ty, 0.0f);
		matrixScale(transform, zs->xscale, zs->yscale, 1.0f);
	}
}

static Bool zoomUpdateMouse(void *vs)
{
	CompScreen *s = (CompScreen *) vs;

	ZOOM_SCREEN(s);

	if (zs->active &&
		!otherScreenGrabExist(s, "scale", "switcher", "move", "resize", 0))
	{
		int winX, winY;
		int rootX, rootY;
		unsigned int mask_return;
		Window root_return;
		Window child_return;

		XQueryPointer(s->display->display, s->root,
					  &root_return, &child_return,
					  &rootX, &rootY, &winX, &winY, &mask_return);

		if (rootX != zs->mouseX || rootY != zs->mouseY)
		{
			zs->mouseX = rootX;
			zs->mouseY = rootY;
			damageScreen(s);
		}
	}

	if (!zs->noTimerUpdate)
		zs->mouseTimeout =
			compAddTimeout(zs->opt[ZOOM_SCREEN_OPTION_MOUSE_UPDATE].value.i,
						   zoomUpdateMouse, s);
	return FALSE;
}

static void zoomPreparePaintScreen(CompScreen * s, int msSinceLastPaint)
{
	ZOOM_SCREEN(s);
	ZOOM_DISPLAY(s->display);

	if (screenGrabExist(s, "rotate", "cube", 0) &&
		zs->opt[ZOOM_SCREEN_OPTION_OUT_ON_CUBE].value.b)
		zs->toScale = 1.0f;
	else
		zs->toScale = zs->newScale;

	if (zs->active)
	{
		int steps;
		float amount, chunk;

		amount = msSinceLastPaint * 0.05f * zs->speed;
		steps = amount / (0.5f * zs->timestep);
		if (!steps)
			steps = 1;
		chunk = amount / (float)steps;

		while (steps--)
		{

			if (adjustZoomVelocity(zs))
			{
				zs->currentScale = zs->toScale;
				zs->zVelocity = 0.0f;
			}
			else
			{
				zs->currentScale +=
						(zs->zVelocity * msSinceLastPaint) / s->redrawTime;
			}

			zs->xscale = zs->currentScale;
			zs->yscale = zs->currentScale;

			if (zs->currentScale == 1.0f && zs->zVelocity == 0.0f &&
				zs->newScale == 1.0)
			{
				zs->active = FALSE;
				if (zs->mouseTimeout)
				{
					compRemoveTimeout(zs->mouseTimeout);
					zs->mouseTimeout = 0;
				}
				break;
			}

		}

		if (zs->currentScale == 1.0f && zs->zVelocity == 0.0f &&
			zs->newScale == 1.0)
		{
			zs->active = FALSE;
			if (zs->mouseTimeout)
			{
				compRemoveTimeout(zs->mouseTimeout);
				zs->mouseTimeout = 0;
			}
		}
	}

	if (!zs->active && zd->fixesSupported && zs->cursorInfoSelected)
	{
		zs->cursorInfoSelected = FALSE;
		XFixesSelectCursorInput(s->display->display, s->root, 0);
	}
	if (!zs->active && zs->cursor.isSet)
	{
		freeCursor(&zs->cursor);
	}
	if (zd->canHideCursor && !zs->active && zs->cursorHidden)
	{
		zs->cursorHidden = FALSE;
		XFixesShowCursor(s->display->display, s->root);
	}

	UNWRAP(zs, s, preparePaintScreen);
	(*s->preparePaintScreen) (s, msSinceLastPaint);
	WRAP(zs, s, preparePaintScreen, zoomPreparePaintScreen);
}

static void zoomDonePaintScreen(CompScreen * s)
{
	ZOOM_SCREEN(s);

	if (zs->active && zs->zVelocity != 0.0f)
	{
		damageScreen(s);
	}
	zs->noTimerUpdate = TRUE;
	zoomUpdateMouse(s);
	zs->noTimerUpdate = FALSE;

	UNWRAP(zs, s, donePaintScreen);
	(*s->donePaintScreen) (s);
	WRAP(zs, s, donePaintScreen, zoomDonePaintScreen);
}

static Bool
zoomPaintScreen(CompScreen * s,
				const ScreenPaintAttrib * sAttrib,
				const CompTransform *transform,
				Region region, int output, unsigned int mask)
{
	Bool status;

	ZOOM_SCREEN(s);

	if (zs->active)
	{
		mask &= ~PAINT_SCREEN_REGION_MASK;
		mask |= PAINT_SCREEN_TRANSFORMED_MASK;// | PAINT_SCREEN_CLEAR_MASK;
		
		int saveFilter = s->filter[SCREEN_TRANS_FILTER];

		if (zs->opt[ZOOM_SCREEN_OPTION_FILTER_LINEAR].value.b ||
			zs->zVelocity != 0.0f)
			s->filter[SCREEN_TRANS_FILTER] = COMP_TEXTURE_FILTER_GOOD;
		else
			s->filter[SCREEN_TRANS_FILTER] = COMP_TEXTURE_FILTER_FAST;

		UNWRAP(zs, s, paintScreen);
		status = (*s->paintScreen) (s, sAttrib, transform, region, output, mask);
		WRAP(zs, s, paintScreen, zoomPaintScreen);

		s->filter[SCREEN_TRANS_FILTER] = saveFilter;
	}
	else
	{
		UNWRAP(zs, s, paintScreen);
		status = (*s->paintScreen) (s, sAttrib, transform, region, output, mask);
		WRAP(zs, s, paintScreen, zoomPaintScreen);
	}

	return status;
}

static void
zoomPaintTransformedScreen(CompScreen * s, const ScreenPaintAttrib * sa,
						   const CompTransform *transform,
						   Region region, int output, unsigned int mask)
{
	ZOOM_SCREEN(s);
	if (zs->active)
	{
		REGION r;

		r.rects = &r.extents;
		r.numRects = 1;

		Bool fullscreen = compDisplayGetRequestFlagForPlugin (s->display,
								"inputzoom","fullscreen");
		Bool singlescreen = compDisplayGetRequestFlagForPlugin (s->display,
								"inputzoom","singlescreen");

		if (fullscreen && !singlescreen)
		{
			r.extents.x1 = zs->mouseX - (zs->mouseX / zs->xscale);
			r.extents.y1 = zs->mouseY - (zs->mouseY / zs->yscale);
			r.extents.x2 = r.extents.x1 + ceil(s->width / zs->xscale) + 1;
			r.extents.y2 = r.extents.y1 + ceil(s->height / zs->yscale) + 1;
		}
		else
		{
			r.extents.x1 = zs->mouseX - (zs->mouseX / zs->xscale) +
					(s->outputDev[output].region.extents.x1 / zs->xscale);
			r.extents.y1 = zs->mouseY - (zs->mouseY / zs->yscale) +
					(s->outputDev[output].region.extents.y1 / zs->yscale);
			r.extents.x2 = r.extents.x1 +
					ceil(s->outputDev[output].width / zs->xscale) + 1;
			r.extents.y2 = r.extents.y1 +
					ceil(s->outputDev[output].height / zs->yscale) + 1;
		}

		r.extents.x1 = MAX(0, r.extents.x1);
		r.extents.y1 = MAX(0, r.extents.y1);
		r.extents.x2 = MIN(s->width, r.extents.x2);
		r.extents.y2 = MIN(s->height, r.extents.y2);

		UNWRAP(zs, s, paintTransformedScreen);
		(*s->paintTransformedScreen) (s, sa, transform, &r, output, mask);
		WRAP(zs, s, paintTransformedScreen, zoomPaintTransformedScreen);

		if (zs->cursor.isSet)
		{

			CompTransform     sTransform = *transform;
			transformToScreenSpace (s, output, -DEFAULT_Z_CAMERA, &sTransform);

    		glPushMatrix ();
    		glLoadMatrixf (sTransform.m);
			glTranslatef(zs->mouseX, zs->mouseY, 0.0);
			glScalef(zs->xscale, zs->yscale, 1.0f);
			int x = -zs->cursor.hotX;
			int y = -zs->cursor.hotY;

			glEnable(GL_BLEND);
			glBindTexture(GL_TEXTURE_RECTANGLE_ARB, zs->cursor.texture);
			glEnable(GL_TEXTURE_RECTANGLE_ARB);

			glBegin(GL_QUADS);
			glTexCoord2d(0, 0);
			glVertex2f(x, y);
			glTexCoord2d(0, zs->cursor.height);
			glVertex2f(x, y + zs->cursor.height);
			glTexCoord2d(zs->cursor.width, zs->cursor.height);
			glVertex2f(x + zs->cursor.width, y + zs->cursor.height);
			glTexCoord2d(zs->cursor.width, 0);
			glVertex2f(x + zs->cursor.width, y);
			glEnd();

			glDisable(GL_BLEND);

			glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
			glDisable(GL_TEXTURE_RECTANGLE_ARB);
			glPopMatrix();
		}
	}
	else
	{
		UNWRAP(zs, s, paintTransformedScreen);
		(*s->paintTransformedScreen) (s, sa, transform, region, output, mask);
		WRAP(zs, s, paintTransformedScreen, zoomPaintTransformedScreen);
	}
}

static Bool
zoomIn(CompDisplay * d,
	   CompAction * action,
	   CompActionState state, CompOption * option, int nOption)
{
	CompScreen *s;
	Window xid;

	ZOOM_DISPLAY(d);

	xid = getIntOptionNamed(option, nOption, "root", 0);

	s = findScreenAtDisplay(d, xid);
	if (s)
	{
		ZOOM_SCREEN(s);

		zs->active = TRUE;

		if (!zs->mouseTimeout)
			zs->mouseTimeout =
					compAddTimeout(zs->opt[ZOOM_SCREEN_OPTION_MOUSE_UPDATE].
								   value.i, zoomUpdateMouse, s);

		if (zd->fixesSupported && !zs->cursorInfoSelected && zs->showScaled)
		{
			zs->cursorInfoSelected = TRUE;
			XFixesSelectCursorInput(s->display->display,
									s->root, XFixesDisplayCursorNotifyMask);
			updateCursor(d->display, &zs->cursor);
		}
		if (zd->canHideCursor && !zs->cursorHidden && zs->hideNormal)
		{
			zs->cursorHidden = TRUE;
			XFixesHideCursor(d->display, s->root);
		}

		zs->newScale *= zs->step;
		if (zs->newScale > zs->maxScale)
			zs->newScale = zs->maxScale;

		damageScreen(s);
	}

	return TRUE;
}

static Bool
zoomOut(CompDisplay * d,
		CompAction * action,
		CompActionState state, CompOption * option, int nOption)
{
	CompScreen *s;
	Window xid;

	ZOOM_DISPLAY(d);

	xid = getIntOptionNamed(option, nOption, "root", 0);

	s = findScreenAtDisplay(d, xid);
	if (s)
	{
		ZOOM_SCREEN(s);

		zs->active = TRUE;

		if (!zs->mouseTimeout)
			zs->mouseTimeout =
					compAddTimeout(zs->opt[ZOOM_SCREEN_OPTION_MOUSE_UPDATE].
								   value.i, zoomUpdateMouse, s);

		if (zd->fixesSupported && !zs->cursorInfoSelected && zs->showScaled)
		{
			zs->cursorInfoSelected = TRUE;
			XFixesSelectCursorInput(s->display->display,
									s->root, XFixesDisplayCursorNotifyMask);
			updateCursor(d->display, &zs->cursor);
		}
		if (zd->canHideCursor && !zs->cursorHidden && zs->hideNormal)
		{
			zs->cursorHidden = TRUE;
			XFixesHideCursor(d->display, s->root);
		}

		zs->newScale /= zs->step;
		if (zs->newScale < 1.0)
			zs->newScale = 1.0;

		damageScreen(s);
	}

	return TRUE;
}

static void zoomUpdateCubeOptions(CompScreen * s)
{
	CompPlugin *p;

	ZOOM_SCREEN(s);

	p = findActivePlugin("cube");
	if (p && p->vTable->getScreenOptions)
	{
		CompOption *options, *option;
		int nOptions;

		options = (*p->vTable->getScreenOptions) (s, &nOptions);
		option = compFindOption(options, nOptions, "in", 0);
		if (option)
			zs->maxTranslate = option->value.b ? 0.85f : 1.5f;
	}
	else
	{
		zs->maxTranslate = 1.5f;
	}
}

static Bool
zoomSetScreenOptionForPlugin(CompScreen * s,
							 char *plugin,
							 char *name, CompOptionValue * value)
{
	Bool status;

	ZOOM_SCREEN(s);

	UNWRAP(zs, s, setScreenOptionForPlugin);
	status = (*s->setScreenOptionForPlugin) (s, plugin, name, value);
	WRAP(zs, s, setScreenOptionForPlugin, zoomSetScreenOptionForPlugin);

	if (status && strcmp(plugin, "cube") == 0)
		zoomUpdateCubeOptions(s);

	return status;
}

static Bool
zoomSetDisplayOption(CompDisplay * display,
					 char *name, CompOptionValue * value)
{
	CompOption *o;
	int index;

	ZOOM_DISPLAY(display);

	o = compFindOption(zd->opt, NUM_OPTIONS(zd), name, &index);
	if (!o)
		return FALSE;

	switch (index)
	{
	case ZOOM_DISPLAY_OPTION_IN:
	case ZOOM_DISPLAY_OPTION_OUT:
		if (setDisplayAction(display, o, value))
			return TRUE;
	default:
		break;
	}

	return FALSE;
}

static void zoomDisplayInitOptions(ZoomDisplay * zd)
{
	CompOption *o;

	o = &zd->opt[ZOOM_DISPLAY_OPTION_IN];
	o->advanced = False;
	o->name = "zoom_in";
	o->group = N_("Bindings");
	o->subGroup = N_("Zoom In");
	o->displayHints = "";
	o->shortDesc = N_("Zoom In");
	o->longDesc = N_("Zoom In.");
	o->type = CompOptionTypeAction;
	o->value.action.initiate = zoomIn;
	o->value.action.terminate = 0;
	o->value.action.bell = FALSE;
	o->value.action.edgeMask = 0;
	o->value.action.state = CompActionStateInitKey;
	o->value.action.state |= CompActionStateInitButton;
	o->value.action.type = CompBindingTypeButton;
	o->value.action.button.modifiers = ZOOM_IN_MODIFIERS_DEFAULT;
	o->value.action.button.button = ZOOM_IN_BUTTON_DEFAULT;

	o = &zd->opt[ZOOM_DISPLAY_OPTION_OUT];
	o->advanced = False;
	o->name = "zoom_out";
	o->group = N_("Bindings");
	o->subGroup = N_("Zoom Out");
	o->displayHints = "";
	o->shortDesc = N_("Zoom Out");
	o->longDesc = N_("Zoom Out.");
	o->type = CompOptionTypeAction;
	o->value.action.initiate = zoomOut;
	o->value.action.terminate = 0;
	o->value.action.bell = FALSE;
	o->value.action.edgeMask = 0;
	o->value.action.state = CompActionStateInitKey;
	o->value.action.state |= CompActionStateInitButton;
	o->value.action.type = CompBindingTypeButton;
	o->value.action.button.modifiers = ZOOM_OUT_MODIFIERS_DEFAULT;
	o->value.action.button.button = ZOOM_OUT_BUTTON_DEFAULT;
}

static CompOption *zoomGetDisplayOptions(CompDisplay * display, int *count)
{
	if (display)
	{
		ZOOM_DISPLAY(display);

		*count = NUM_OPTIONS(zd);
		return zd->opt;
	}
	else
	{
		ZoomDisplay *zd = malloc(sizeof(ZoomDisplay));

		zoomDisplayInitOptions(zd);
		*count = NUM_OPTIONS(zd);
		return zd->opt;
	}
}

static Bool zoomInitDisplay(CompPlugin * p, CompDisplay * d)
{
	ZoomDisplay *zd;

	zd = malloc(sizeof(ZoomDisplay));
	if (!zd)
		return FALSE;

	zd->screenPrivateIndex = allocateScreenPrivateIndex(d);
	if (zd->screenPrivateIndex < 0)
	{
		free(zd);
		return FALSE;
	}

	zoomDisplayInitOptions(zd);

	WRAP(zd, d, handleEvent, zoomHandleEvent);

	d->privates[displayPrivateIndex].ptr = zd;


	zd->fixesSupported =
			XFixesQueryExtension(d->display, &zd->fixesEventBase,
								 &zd->fixesErrorBase);

	int minor, major;

	XFixesQueryVersion(d->display, &major, &minor);

	if (major >= 4)
		zd->canHideCursor = TRUE;
	else
		zd->canHideCursor = FALSE;

	return TRUE;
}

static void zoomFiniDisplay(CompPlugin * p, CompDisplay * d)
{
	ZOOM_DISPLAY(d);

	UNWRAP(zd, d, handleEvent);
	freeScreenPrivateIndex(d, zd->screenPrivateIndex);

	free(zd);
}

static Bool zoomInitScreen(CompPlugin * p, CompScreen * s)
{
	ZoomScreen *zs;

	ZOOM_DISPLAY(s->display);

	zs = malloc(sizeof(ZoomScreen));
	if (!zs)
		return FALSE;

	zs->active = FALSE;

	zs->mouseTimeout = 0;

	zs->currentScale = 1.0f;
	zs->newScale = 1.0f;

	zs->zVelocity = 0.0f;

	zs->maxTranslate = 0.85f;

	zs->speed = ZOOM_SPEED_DEFAULT;
	zs->step = ZOOM_STEP_DEFAULT;
	zs->maxScale = ZOOM_MAX_FACTOR_DEFAULT;
	zs->timestep = ZOOM_TIMESTEP_DEFAULT;

	zoomScreenInitOptions(zs);

	addScreenAction(s, &zd->opt[ZOOM_DISPLAY_OPTION_IN].value.action);
	addScreenAction(s, &zd->opt[ZOOM_DISPLAY_OPTION_OUT].value.action);

	WRAP(zs, s, preparePaintScreen, zoomPreparePaintScreen);
	WRAP(zs, s, donePaintScreen, zoomDonePaintScreen);
	WRAP(zs, s, paintScreen, zoomPaintScreen);
	WRAP(zs, s, paintTransformedScreen, zoomPaintTransformedScreen);
	WRAP(zs, s, setScreenOptionForPlugin, zoomSetScreenOptionForPlugin);
	WRAP(zs, s, applyScreenTransform, zoomApplyScreenTransform);
	WRAP(zs, s, setClipPlanes, zoomSetClipPlanes);

	s->privates[zd->screenPrivateIndex].ptr = zs;

	zoomUpdateCubeOptions(s);

	zs->cursor.isSet = FALSE;
	zs->cursorHidden = FALSE;
	zs->cursorInfoSelected = FALSE;

	zs->showScaled = ZOOM_SHOW_SCALED_CURSOR_DEFAULT;
	zs->hideNormal = ZOOM_HIDE_NORMAL_CURSOR_DEFAULT;

	zs->noTimerUpdate = FALSE;

	return TRUE;
}

static void zoomFiniScreen(CompPlugin * p, CompScreen * s)
{
	ZOOM_SCREEN(s);
	ZOOM_DISPLAY(s->display);

	if (zs->mouseTimeout)
	{
		compRemoveTimeout(zs->mouseTimeout);
		zs->mouseTimeout = 0;
	}

	UNWRAP(zs, s, preparePaintScreen);
	UNWRAP(zs, s, donePaintScreen);
	UNWRAP(zs, s, paintScreen);
	UNWRAP(zs, s, paintTransformedScreen);
	UNWRAP(zs, s, setScreenOptionForPlugin);
	UNWRAP(zs, s, applyScreenTransform);
	UNWRAP(zs, s, setClipPlanes);

	removeScreenAction(s, &zd->opt[ZOOM_DISPLAY_OPTION_IN].value.action);
	removeScreenAction(s, &zd->opt[ZOOM_DISPLAY_OPTION_OUT].value.action);

	free(zs);
}


static Bool zoomInit(CompPlugin * p)
{
	displayPrivateIndex = allocateDisplayPrivateIndex();
	if (displayPrivateIndex < 0)
		return FALSE;

	return TRUE;
}

static void zoomFini(CompPlugin * p)
{
	if (displayPrivateIndex >= 0)
		freeDisplayPrivateIndex(displayPrivateIndex);
}

CompPluginFeature zoomFeatures[] = {
	{"zoom"}
};

CompPluginVTable zoomVTable = {
	"inputzoom",
	N_("Input enabled Zoom"),
	N_("Input Enabled Zoom"),
	zoomInit,
	zoomFini,
	zoomInitDisplay,
	zoomFiniDisplay,
	zoomInitScreen,
	zoomFiniScreen,
	0,
	0,
	zoomGetDisplayOptions,
	zoomSetDisplayOption,
	zoomGetScreenOptions,
	zoomSetScreenOption,
	0,
	0,
	0,
	zoomFeatures,
	sizeof(zoomFeatures) / sizeof(zoomFeatures[0]),
	BERYL_ABI_INFO,
	"beryl-plugins",
	"accessibility",
	0,
	0,
	True,
};

CompPluginVTable *getCompPluginInfo(void)
{
	return &zoomVTable;
}
