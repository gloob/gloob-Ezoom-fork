/*
 * Copyright © 2005 Novell, Inc.
 * Copyright (C) 2007, 2008 Kristian Lyngstøl
 *
 * Ported to compiz 0.9 by:
 * Copyright (C) 2009, Sam Spilsbury
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Novell, Inc. not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 * Novell, Inc. makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without express or
 * implied warranty.
 *
 * NOVELL, INC. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL NOVELL, INC. BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *
 * Author(s):
 *	- Original zoom plug-in; David Reveman <davidr@novell.com>
 *	- Most features beyond basic zoom;
 *	  Kristian Lyngstol <kristian@bohemians.org>
 *	- Sam I suppose? (for the c++)
 *
 * Todo: Un-puke the header-file. Five levels of indentation in a .h is a
 * clear sign that something is horribly wrong.
 *
 * Essentially, this needs to be re-done for C++. As is the case of most of
 * eZoom.
 *
 */

#include <core/core.h>
#include <core/serialization.h>
#include <composite/composite.h>
#include <opengl/opengl.h>
#include <mousepoll/mousepoll.h>


#include "ezoom_options.h"

#include <cmath>

class EZoomScreen :
    public PluginClassHandler <EZoomScreen, CompScreen>,
    public PluginStateWriter <EZoomScreen>,
    public EzoomOptions,
    public ScreenInterface,
    public CompositeScreenInterface,
    public GLScreenInterface
{
    public:

	EZoomScreen (CompScreen *);
	~EZoomScreen ();

    public:

	CompositeScreen *cScreen;
	GLScreen	*gScreen;

    public:

	typedef enum {
	    NORTHEAST,
	    NORTHWEST,
	    SOUTHEAST,
	    SOUTHWEST,
	    CENTER
	} ZoomGravity;

	typedef enum {
	    NORTH,
	    SOUTH,
	    EAST,
	    WEST
	} ZoomEdge;

	class CursorTexture
	{
	    public:
		bool       isSet;
		GLuint     texture;
		CompScreen *screen;
		int        width;
		int        height;
		int        hotX;
		int        hotY;
	    public:
		CursorTexture ();
	};

	/* Stores an actual zoom-setup. This can later be used to store/restore
	 * zoom areas on the fly.
	 *
	 * [xy]Translate and newZoom are target values, and [xy]Translate always
	 * ranges from -0.5 to 0.5.
	 *
	 * currentZoom is actual zoomed value
	 *
	 * real[XY]Translate are the currently used values in the same range as
	 * [xy]Translate, and [xy]trans is adjusted for the zoom level in place.
	 * [xyz]trans should never be modified except in updateActualTranslates()
	 *
	 * viewport is a mask of the viewport, or ~0 for "any".
	 */
	class ZoomArea
	{
	    public:

		template <class Archive>
		void serialize (Archive &ar, const unsigned int)
		{
		    ar & output;
		    ar & viewport;
		    ar & newZoom;
		    ar & xVelocity;
		    ar & yVelocity;
		    ar & xTranslate;
		    ar & yTranslate;
		    ar & realXTranslate;
		    ar & realYTranslate;
		    ar & xtrans;
		    ar & ytrans;
		    ar & locked;
		}

	    public:
		int               output;
		unsigned long int viewport;
		GLfloat           currentZoom;
		GLfloat           newZoom;
		GLfloat           xVelocity;
		GLfloat           yVelocity;
		GLfloat           zVelocity;
		GLfloat           xTranslate;
		GLfloat           yTranslate;
		GLfloat           realXTranslate;
		GLfloat           realYTranslate;
		GLfloat           xtrans;
		GLfloat           ytrans;
		bool              locked;
	    public:

		ZoomArea (int out);
		ZoomArea ();

		void
		updateActualTranslates ();
	};

    public:

	template <class Archive>
	void serialize (Archive &ar, const unsigned int version)
	{
	    ar & zooms;
	    ar & lastChange;
	    ar & grabbed;
	}

	std::vector <ZoomArea>   zooms; // list of zooms (different zooms for
					// each output
	CompPoint		 mouse; // we get this from mousepoll
	unsigned long int	 grabbed;
	CompScreen::GrabHandle   grabIndex; // for zoomBox
	time_t			 lastChange;
	CursorTexture		 cursor; // the texture for the faux-cursor
					 // we paint to do fake input
					 // handling
	bool			 cursorInfoSelected;
	bool			 cursorHidden;
	CompRect		 box;
	CompPoint	         clickPos;

	MousePoller		 pollHandle; // mouse poller object

     private:

	bool fixesSupported;
	int fixesEventBase;
	int fixesErrorBase;
	bool canHideCursor;

     public:

	void
	postLoad ();

	void
	preparePaint (int);

	bool
	glPaintOutput (const GLScreenPaintAttrib &,
		       const GLMatrix		 &,
		       const CompRegion		 &,
		       CompOutput		 *,
		       unsigned int);

	void
	donePaint ();

	void
	handleEvent (XEvent *);

    public:

	int
	distanceToEdge (int out, EZoomScreen::ZoomEdge edge);

	bool
	isInMovement (int out);

	void
	adjustZoomVelocity (int out, float chunk);

	void
	adjustXYVelocity (int out, float chunk);

	void
	drawBox (const GLMatrix &transform,
		 CompOutput          *output,
		 CompRect             box);

	void
	setCenter (int x, int y, bool instant);

	void
	setZoomArea (int        x,
		     int        y,
		     int        width,
		     int        height,
		     bool       instant);

	void
	areaToWindow (CompWindow *w);

	void
	panZoom (int xvalue, int yvalue);

	void
	enableMousePolling ();

	void
	setScale (int out, float value);

	void
	syncCenterToMouse ();

	void
	convertToZoomed (int        out,
			 int        x,
			 int        y,
			 int        *resultX,
			 int        *resultY);

	void
	convertToZoomedTarget (int	  out,
			       int	  x,
			       int	  y,
			       int	  *resultX,
			       int	  *resultY);

	bool
	ensureVisibility (int x, int y, int margin);

	void
	ensureVisibilityArea (int         x1,
			      int         y1,
			      int         x2,
			      int         y2,
			      int         margin,
			      ZoomGravity gravity);

	void
	restrainCursor (int out);

	void
	cursorMoved ();

	void
	updateMousePosition (const CompPoint &p);

	void
	updateMouseInterval (const CompPoint &p);

	/* Make dtor */
	void
	freeCursor (CursorTexture * cursor);

	void
	drawCursor (CompOutput          *output,
		    const GLMatrix      &transform);

	void
	updateCursor (CursorTexture * cursor);

	void
	cursorZoomInactive ();

	void
	cursorZoomActive (int);

    public:

	bool
	setZoomAreaAction (CompAction         *action,
			   CompAction::State  state,
			   CompOption::Vector options);

	bool
	ensureVisibilityAction (CompAction         *action,
				CompAction::State  state,
				CompOption::Vector options);

	bool
	zoomBoxActivate (CompAction         *action,
			 CompAction::State  state,
			 CompOption::Vector options);

	bool
	zoomBoxDeactivate (CompAction         *action,
			   CompAction::State  state,
			   CompOption::Vector options);

	bool
	zoomIn (CompAction         *action,
		CompAction::State  state,
		CompOption::Vector options);

	bool
	lockZoomAction (CompAction         *action,
			CompAction::State  state,
			CompOption::Vector options);

	bool
	zoomSpecific (CompAction         *action,
		      CompAction::State  state,
		      CompOption::Vector options,
		      float		     target);

	bool
	zoomToWindow (CompAction         *action,
		      CompAction::State  state,
		      CompOption::Vector options);

	bool
	zoomPan (CompAction         *action,
		 CompAction::State  state,
		 CompOption::Vector options,
		 float		horizAmount,
		 float		vertAmount);

	bool
	zoomCenterMouse (CompAction         *action,
			 CompAction::State  state,
			 CompOption::Vector options);

	bool
	zoomFitWindowToZoom (CompAction         *action,
			     CompAction::State  state,
			     CompOption::Vector options);

	bool
	initiate (CompAction         *action,
		  CompAction::State  state,
		  CompOption::Vector options);

	bool
	zoomOut (CompAction         *action,
		 CompAction::State  state,
		 CompOption::Vector options);

	bool
	terminate (CompAction         *action,
		   CompAction::State  state,
		   CompOption::Vector options);

	void
	focusTrack (XEvent *event);
};

#define ZOOM_SCREEN(s)							       \
     EZoomScreen *zs = EZoomScreen::get (s)

class ZoomPluginVTable :
    public CompPlugin::VTableForScreen <EZoomScreen>
{
    public:

	bool init ();
};
