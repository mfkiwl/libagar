/*	$Csoft: mapedit.c,v 1.6 2005/09/27 00:25:18 vedge Exp $	*/

/*
 * Copyright (c) 2001, 2002, 2003, 2004, 2005 CubeSoft Communications, Inc.
 * <http://www.csoft.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <agar/core/core.h>
#include <agar/core/objmgr.h>

#include <agar/gui/widget.h>
#include <agar/gui/window.h>
#include <agar/gui/box.h>
#include <agar/gui/checkbox.h>
#include <agar/gui/spinbutton.h>
#include <agar/gui/mspinbutton.h>

#include "map.h"
#include "mapedit.h"

const AG_ObjectOps mapEditorOps = {
	"MAP_Editor",
	sizeof(AG_Object),
	{ 0, 0 },
	NULL,				/* init */
	NULL,				/* reinit */
	MAP_EditorDestroy,
	NULL,				/* load */
	NULL,				/* save */
	NULL				/* edit */
};

const AG_ObjectOps mapEditorPseudoOps = {
	"MAP_EditorPseudo",
	sizeof(AG_Object),
	{ 0, 0 },
	NULL,			/* init */
	NULL,			/* reinit */
	NULL,			/* destroy */
	NULL,			/* load */
	NULL,			/* save */
	MAP_EditorConfig	/* edit */
};

extern int agEditMode;
extern int mapViewAnimatedBg, mapViewBgTileSize;
extern int mapViewEditSelOnly;

MAP_Editor mapEditor;

int mapDefaultWidth = 9;		/* Default map geometry */
int mapDefaultHeight = 9;
int mapDefaultBrushWidth = 9;		/* Default brush geometry */
int mapDefaultBrushHeight = 9;

void
MAP_EditorInit(void)
{
	AG_ObjectInit(&mapEditor, "map-editor", &mapEditorOps);
	AGOBJECT(&mapEditor)->flags |= (AG_OBJECT_RELOAD_PROPS|
	                                AG_OBJECT_STATIC);
	AGOBJECT(&mapEditor)->save_pfx = "/map-editor";

	/* Attach a pseudo-object for dependency keeping purposes. */
	AG_ObjectInit(&mapEditor.pseudo, "map-editor-ref", &mapEditorPseudoOps);
	AGOBJECT(&mapEditor.pseudo)->flags |= (AG_OBJECT_NON_PERSISTENT|
				               AG_OBJECT_STATIC|
	                                       AG_OBJECT_INDESTRUCTIBLE);
	AG_ObjectAttach(agWorld, &mapEditor.pseudo);

	/*
	 * Allocate the copy/paste buffer.
	 * Use AG_OBJECT_READONLY to avoid circular reference in case a user
	 * attempts to paste contents of the copy buffer into itself.
	 */
	MAP_Init(&mapEditor.copybuf, "copybuf");
	AGOBJECT(&mapEditor.copybuf)->flags |= (AG_OBJECT_NON_PERSISTENT|
				               AG_OBJECT_STATIC|
	                                       AG_OBJECT_INDESTRUCTIBLE|
					       AG_OBJECT_READONLY);
	AG_ObjectAttach(&mapEditor.pseudo, &mapEditor.copybuf);

	agEditMode = 1;

	/* Initialize the default tunables. */
	AG_SetUint32(&mapEditor, "default-map-width", 12);
	AG_SetUint32(&mapEditor, "default-map-height", 8);
	AG_SetUint32(&mapEditor, "default-brush-width", 5);
	AG_SetUint32(&mapEditor, "default-brush-height", 5);

	/* Initialize the object manager. */
	AG_ObjMgrInit();
	AG_WindowShow(AG_ObjMgrWindow());
}

void
MAP_EditorDestroy(void *p)
{
	MAP_Destroy(&mapEditor.copybuf);
	AG_ObjMgrDestroy();
}

void
MAP_EditorSave(AG_Netbuf *buf)
{
	AG_WriteUint8(buf, 0);				/* Pad: mapViewBg */
	AG_WriteUint8(buf, (Uint8)mapViewAnimatedBg);
	AG_WriteUint16(buf, (Uint16)mapViewBgTileSize);
	AG_WriteUint8(buf, (Uint8)mapViewEditSelOnly);

	AG_WriteUint16(buf, (Uint16)mapDefaultWidth);
	AG_WriteUint16(buf, (Uint16)mapDefaultHeight);
	AG_WriteUint16(buf, (Uint16)mapDefaultBrushWidth);
	AG_WriteUint16(buf, (Uint16)mapDefaultBrushHeight);
}

void
MAP_EditorLoad(AG_Netbuf *buf)
{
	AG_ReadUint8(buf);				/* Pad: mapViewBg */
	mapViewAnimatedBg = (int)AG_ReadUint8(buf);
	mapViewBgTileSize = (int)AG_ReadUint16(buf);
	mapViewEditSelOnly = (int)AG_ReadUint8(buf);

	mapDefaultWidth = (int)AG_ReadUint16(buf);
	mapDefaultHeight = (int)AG_ReadUint16(buf);
	mapDefaultBrushWidth = (int)AG_ReadUint16(buf);
	mapDefaultBrushHeight = (int)AG_ReadUint16(buf);
}

void *
MAP_EditorConfig(void *p)
{
	AG_Window *win;
	AG_Checkbox *cb;
	AG_Spinbutton *sb;
	AG_MSpinbutton *msb;
	AG_Box *bo;

	win = AG_WindowNew(AG_WINDOW_NOVRESIZE);
	AG_WindowSetCaption(win, _("Map editor settings"));

	bo = AG_BoxNew(win, AG_BOX_VERT, AG_BOX_HFILL);
	AG_BoxSetSpacing(bo, 5);
	{
		cb = AG_CheckboxNew(bo,0 , _("Moving tiles"));
		AG_WidgetBind(cb, "state", AG_WIDGET_INT, &mapViewAnimatedBg);

		sb = AG_SpinbuttonNew(bo, 0, _("Tile size: "));
		AG_WidgetBind(sb, "value", AG_WIDGET_INT, &mapViewBgTileSize);
		AG_SpinbuttonSetMin(sb, 2);
		AG_SpinbuttonSetMax(sb, 16384);
	}

	bo = AG_BoxNew(win, AG_BOX_VERT, AG_BOX_HFILL);
	{
		cb = AG_CheckboxNew(bo, 0, _("Selection-bounded edition"));
		AG_WidgetBind(cb, "state", AG_WIDGET_INT, &mapViewEditSelOnly);
	}

	bo = AG_BoxNew(win, AG_BOX_VERT, AG_BOX_HFILL);
	{
		msb = AG_MSpinbuttonNew(bo, 0, "x",
		    _("Default map geometry: "));
		AG_WidgetBind(msb, "xvalue", AG_WIDGET_INT, &mapDefaultWidth);
		AG_WidgetBind(msb, "yvalue", AG_WIDGET_INT, &mapDefaultHeight);
		AG_MSpinbuttonSetMin(msb, 1);
		AG_MSpinbuttonSetMax(msb, MAP_WIDTH_MAX);
		
		msb = AG_MSpinbuttonNew(bo, 0, "x",
		    _("Default brush geometry: "));
		AG_WidgetBind(msb, "xvalue", AG_WIDGET_INT,
		    &mapDefaultBrushWidth);
		AG_WidgetBind(msb, "yvalue", AG_WIDGET_INT,
		    &mapDefaultBrushHeight);
		AG_MSpinbuttonSetMin(msb, 1);
		AG_MSpinbuttonSetMax(msb, MAP_WIDTH_MAX);
	}
	return (win);
}