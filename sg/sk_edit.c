/*
 * Copyright (c) 2005-2007 Hypertriton, Inc.
 * <http://www.hypertriton.com/>
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
#include <agar/gui/gui.h>

#include "sk.h"

#include <GL/gl.h>
#include <GL/glu.h>

#include <string.h>
#include <math.h>

#ifdef EDITION

extern SK_ToolOps skPointToolOps;
extern SK_ToolOps skLineToolOps;
extern SK_ToolOps skCircleToolOps;

SK_ToolOps *skToolkit[] = {
	&skPointToolOps,
	&skLineToolOps,
	&skCircleToolOps
};
Uint skToolkitCount = sizeof(skToolkit) / sizeof(skToolkit[0]);

static int
CompareClass(const char *pat, const char *cname)
{
	const char *c;

	if (pat[0] == '*' && pat[1] == '\0') {
		return (0);
	}
	for (c = &pat[0]; *c != '\0'; c++) {
		if (c[0] == ':' && c[1] == '*' && c[2] == '\0') {
			if (c == &pat[0])
				return (0);
			if (strncmp(cname, pat, c - &pat[0] + 1) == 0)
				return (0);
		}
	}
	return (1);
}

static void
ListLibraryItems(AG_Tlist *tl, const char *cname, int depth)
{
	char subname[SK_TYPE_NAME_MAX];
	const char *c;
	AG_TlistItem *it;
	int i, j;

	for (i = 0; i < skElementsCnt; i++) {
		SK_NodeOps *ops = skElements[i];
		
		if (CompareClass(cname, ops->name) != 0) {
			continue;
		}
		for (c = ops->name, j = 0; *c != '\0'; c++) {
			if (*c == ':')
				j++;
		}
		if (j == depth) {
			it = AG_TlistAdd(tl, AGICON(OBJ_ICON), "%s", ops->name);
			it->p1 = ops;
			it->depth = depth;
			strlcpy(subname, ops->name, sizeof(subname));
			strlcat(subname, ":*", sizeof(subname));
			ListLibraryItems(tl, subname, depth+1);
		}
	}
}

static void
ImportSketchDlg(AG_Event *event)
{
	SK *sk = AG_PTR(1);
	AG_Window *pwin = AG_PTR(2);
	AG_Window *win;
	AG_FileDlg *dlg;

	win = AG_WindowNew(0);
	dlg = AG_FileDlgNew(win, AG_FILEDLG_LOAD|AG_FILEDLG_CLOSEWIN|
	                         AG_FILEDLG_EXPAND);
#if 0
	AG_FileDlgAddType(dlg, _("DXF Format"), "*.dxf",
	    ImportFromDXF, "%p", sk);
	AG_FileDlgAddType(dlg, _("PDF Format"), "*.pdf",
	    ImportFromPDF, "%p", sk);
#endif
	AG_WindowShow(win);
}

static void
NodeDelete(AG_Event *event)
{
	SK_Node *node = AG_PTR(1);
}

static void
CreateNewView(AG_Event *event)
{
	SK *sk = AG_PTR(1);
	SK_View *skv;
	AG_Window *win;
	int num = 0;
	
	win = AG_WindowNew(0);
	AG_WindowSetCaption(win, "%s", AGOBJECT(sk)->name);
	skv = SK_ViewNew(win, sk, SK_VIEW_EXPAND);
	AG_WindowShow(win);
	AG_WidgetFocus(skv);
}

static void
NodePopupMenu(AG_Event *event)
{
	AG_Tlist *tl = AG_SELF();
	SK_Node *node = AG_TlistSelectedItemPtr(tl);
	SK *sk = AG_PTR(1);
	SK_View *skv = AG_PTR(2);
	AG_PopupMenu *pm;

	pm = AG_PopupNew(skv);
	AG_MenuAction(pm->item, _("Delete entity"), TRASH_ICON,
	    NodeDelete, "%p", node);
	AG_PopupShow(pm);
}

static void
FindNodes(AG_Tlist *tl, SK_Node *node, int depth)
{
	AG_TlistItem *it;
	SK_Node *cnode;

	it = AG_TlistAdd(tl, AGICON(EDA_NODE_ICON), "%s", node->ops->name);
	it->depth = depth;
	it->p1 = node;
	it->selected = (node->flags & SG_NODE_SELECTED);

	if (!TAILQ_EMPTY(&node->cnodes)) {
		it->flags |= AG_TLIST_HAS_CHILDREN;
	}
	if ((it->flags & AG_TLIST_HAS_CHILDREN) &&
	    AG_TlistVisibleChildren(tl, it)) {
		TAILQ_FOREACH(cnode, &node->cnodes, sknodes)
			FindNodes(tl, cnode, depth+1);
	}
}

static void
PollNodes(AG_Event *event)
{
	AG_Tlist *tl = AG_SELF();
	SG *sg = AG_PTR(1);

	AG_TlistClear(tl);
	FindNodes(tl, (SK_Node *)sg->root, 0);
	AG_TlistRestore(tl);
}

static void
SelectNode(AG_Event *event)
{
	AG_Tlist *tl = AG_SELF();
	AG_TlistItem *it = AG_PTR(1);
	int state = AG_INT(2);
	SK_Node *node = it->p1;
	
	if (state) {
		node->flags |= SK_NODE_SELECTED;
	} else {
		node->flags &= ~(SK_NODE_SELECTED);
	}
}

static void
SelectTool(AG_Event *event)
{
	SK_View *skv = AG_PTR(1);
	AG_TlistItem *it = AG_PTR(2);
	SK_Tool *tool = it->p1;

	dprintf("select tool %s\n", tool->ops->name);
	SK_ViewSelectTool(skv, tool, NULL);
}

void *
SK_Edit(void *p)
{
	extern SK_ToolOps *skToolkit[];
	extern Uint skToolkitCount;
	SK *sk = p;
	AG_Window *win;
	SK_View *skv;
	AG_Menu *menu;
	AG_MenuItem *pitem;
	AG_Pane *hp;
	int i, dx;

	win = AG_WindowNew(0);
	AG_WindowSetCaption(win, "%s", AGOBJECT(sk)->name);

	skv = Malloc(sizeof(SK_View), M_OBJECT);
	SK_ViewInit(skv, sk, SK_VIEW_EXPAND);
			
	for (i = 0; i < skToolkitCount; i++)
		SK_ViewRegTool(skv, skToolkit[i], NULL);

	menu = AG_MenuNew(win, AG_MENU_HFILL);
	pitem = AG_MenuAddItem(menu, _("File"));
	{
		AG_MenuAction(pitem, _("Import sketch..."), CLOSE_ICON,
		    ImportSketchDlg, "%p,%p", sk, win);
		AG_MenuSeparator(pitem);
		AG_ObjMgrGenericMenu(pitem, sk);
		AG_MenuSeparator(pitem);
		AG_MenuActionKb(pitem, _("Close document"), CLOSE_ICON,
		    SDLK_w, KMOD_CTRL,
		    AG_WindowCloseGenEv, "%p", win);
	}
	
	pitem = AG_MenuAddItem(menu, _("Edit"));
	{
		/* TODO */
		AG_MenuAction(pitem, _("Undo"), -1, NULL, "%p", NULL);
		AG_MenuAction(pitem, _("Redo"), -1, NULL, "%p", NULL);
	}
	pitem = AG_MenuAddItem(menu, _("View"));
	{
		AG_MenuAction(pitem, _("New view..."), -1,
		    CreateNewView, "%p", sk);
	}
	
	hp = AG_PaneNew(win, AG_PANE_HORIZ, AG_PANE_EXPAND);
	{
		AG_Notebook *nb;
		AG_NotebookTab *ntab;
		AG_Tlist *tl;
		AG_Pane *vp;
		AG_MPane *mp;

		vp = AG_PaneNew(hp->div[0], AG_PANE_VERT,
		    AG_PANE_EXPAND|AG_PANE_DIV1FILL);
		//AG_PaneSetDivisionMin(hp, 0, 0, 0);
		nb = AG_NotebookNew(vp->div[0], AG_NOTEBOOK_EXPAND);
		mp = AG_MPaneNew(hp->div[1], AG_MPANE1, AG_MPANE_EXPAND);
		AG_ObjectAttach(mp->panes[0], skv);
#if 0
		AG_SetEvent(tl, "tlist-dblclick", EditNode, "%p,%p,%p",
		    hp, vp, skv);
#endif
	
		ntab = AG_NotebookAddTab(nb, _("Tools"), AG_BOX_VERT);
		{
			AG_Tlist *tl;
			SK_Tool *tool;

			tl = AG_TlistNew(ntab, AG_TLIST_EXPAND);
			TAILQ_FOREACH(tool, &skv->tools, tools) {
				AG_TlistAddPtr(tl, AGICON(tool->ops->icon),
				    _(tool->ops->name), tool);
			}

			AG_SetEvent(tl, "tlist-selected", SelectTool, "%p",
			    skv);
		}
		ntab = AG_NotebookAddTab(nb, _("Nodes"), AG_BOX_VERT);
		{
			tl = AG_TlistNew(ntab, AG_TLIST_POLL|AG_TLIST_TREE|
			                       AG_TLIST_EXPAND|AG_TLIST_MULTI);
			AG_TlistPrescale(tl, "<Polygon>", 2);
			AG_TlistSetPopupFn(tl, NodePopupMenu, "%p,%p", sk, skv);
			AG_SetEvent(tl, "tlist-poll", PollNodes, "%p", sk);
			AG_SetEvent(tl, "tlist-changed", SelectNode, NULL);
			AGWIDGET(tl)->flags &= ~(AG_WIDGET_FOCUSABLE);
		}
	}
	
	AG_WindowScale(win, -1, -1);
	AG_WindowSetGeometry(win, agView->w/6, agView->h/6,
	                     2*agView->w/3, 2*agView->h/3);
	AG_WidgetFocus(skv);
	return (win);
}

#endif /* EDITION */