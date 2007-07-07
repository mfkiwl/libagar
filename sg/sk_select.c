/*
 * Copyright (c) 2007 Hypertriton, Inc. <http://hypertriton.com/>
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

/*
 * Generic selection tool for sketch editor.
 */

#include <config/edition.h>
#include <config/have_opengl.h>
#if defined(HAVE_OPENGL) && defined(EDITION)

#include <core/core.h>

#include "sk.h"
#include "sk_view.h"
#include "sg_gui.h"

#include <gui/checkbox.h>

typedef struct sk_select_tool {
	SK_Tool tool;
	Uint flags;
#define SELECT_POINTS	0x01
#define SELECT_LINES	0x02
#define SELECT_ARCS	0x04
} SK_SelectTool;

static int
mousemotion(void *p, SG_Vector pos, SG_Vector vel, int btn)
{
	SK_SelectTool *t = p;
	SK_View *skv = SKTOOL(t)->skv;
	SK *sk = skv->sk;
	SG_Vector vC;
	SK_Node *node;

	TAILQ_FOREACH(node, &sk->nodes, nodes) {
		node->flags &= ~(SK_NODE_MOUSEOVER);
		node->flags &= ~(SK_NODE_MOVED);
	}
	
	/* Give point proximity more weight than other entities. */
	if ((node = SK_ProximitySearch(sk, "Point", &pos, &vC, NULL)) != NULL &&
	    SG_VectorDistance2p(&pos, &vC) < skv->rSnap) {
		node->flags |= SK_NODE_MOUSEOVER;
	} else {
		if ((node = SK_ProximitySearch(sk, NULL, &pos, &vC, NULL))
		    != NULL) {
			node->flags |= SK_NODE_MOUSEOVER;
		}
	}

	/* Move selected elements. */
	if (btn & SDL_BUTTON_LEFT) {
		TAILQ_FOREACH(node, &sk->nodes, nodes) {
			if (node->flags & SK_NODE_MOVED ||
			  !(node->flags & SK_NODE_SELECTED)) {
				continue;
			}
			if (strcmp(node->ops->name, "Point") == 0) {
				SK_Translatev(node, &vel);
			} else if (strcmp(node->ops->name, "Line") == 0) {
				SK_Line *ln = (SK_Line *)node;

				if (!(SKNODE(ln->p1)->flags & SK_NODE_MOVED)) {
					SK_Translatev(ln->p1, &vel);
					SKNODE(ln->p1)->flags |= SK_NODE_MOVED;
				}
				if (!(SKNODE(ln->p2)->flags & SK_NODE_MOVED)) {
					SK_Translatev(ln->p2, &vel);
					SKNODE(ln->p2)->flags |= SK_NODE_MOVED;
				}
			} else if (strcmp(node->ops->name, "Circle") == 0) {
				SK_Circle *c = (SK_Circle *)node;

				if (!(SKNODE(c->p)->flags & SK_NODE_MOVED)) {
					SK_Translatev(c->p, &vel);
					SKNODE(c->p)->flags |= SK_NODE_MOVED;
				}
			}
			node->flags |= SK_NODE_MOVED;
		}
	}
	return (0);
}

static int
mousebuttondown(void *pTool, SG_Vector pos, int btn)
{
	SK_Tool *tool = pTool;
	SK_SelectTool *t = pTool;
	SK_View *skv = tool->skv;
	SK *sk = skv->sk;
	SK_Node *node;
	SG_Vector vC;
	SK_Point *pt;
	int ctrlMode = (SDL_GetModState() & KMOD_CTRL);
	
	if (btn != SDL_BUTTON_LEFT)
		return (0);
	
	if (!ctrlMode) {
		TAILQ_FOREACH(node, &sk->nodes, nodes)
			node->flags &= ~(SK_NODE_SELECTED);
	}
	
	/* Give point proximity more weight than other entities. */
	if ((node = SK_ProximitySearch(sk, "Point", &pos, &vC, NULL)) == NULL ||
	    SG_VectorDistance2p(&pos, &vC) >= skv->rSnap) {
		if ((node = SK_ProximitySearch(sk, NULL, &pos, &vC, NULL))
		    == NULL)
			return (0);
	}
	if (ctrlMode && node->flags & SK_NODE_SELECTED) {
		node->flags &= ~SK_NODE_SELECTED;
	} else {
		node->flags |=  SK_NODE_SELECTED;
	}
	return (0);
}

static void
init(void *p)
{
	SK_SelectTool *t = p;

	t->flags = 0;
}

static void
edit(void *p, void *box)
{
	SK_SelectTool *t = p;
	static const AG_FlagDescr flagDescr[] = {
	    { SELECT_POINTS,		"Points",	1 },
	    { SELECT_LINES,		"Lines",	1 },
	    { SELECT_ARCS,		"Arcs",		1 },
	    { 0,			NULL,		0 }
	};
	AG_CheckboxSetFromFlags(box, &t->flags, flagDescr);
}

SK_ToolOps skSelectToolOps = {
	N_("Select"),
	N_("Select and move sketch items"),
	SELECT_NODE_ICON,
	sizeof(SK_SelectTool),
	0,
	NULL,		/* init */
	NULL,		/* destroy */
	edit,
	mousemotion,
	mousebuttondown,
	NULL,		/* buttonup */
	NULL,		/* keydown */
	NULL		/* keyup */
};

#endif /* HAVE_OPENGL && EDITION */