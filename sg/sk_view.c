/*
 * Copyright (c) 2006-2007 Hypertriton, Inc.
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
#include <agar/core/config.h>
#include <agar/core/view.h>

#include <stdarg.h>
#include <string.h>

#include "sk.h"
#include "sk_view.h"

#include <agar/gui/window.h>
#include <agar/gui/primitive.h>

const AG_WidgetOps skViewOps = {
	{
		"AG_Widget:SK_View",
		sizeof(SK_View),
		{ 0,0 },
		NULL,		/* init */
		NULL,		/* reinit */
		AG_WidgetDestroy,
		NULL,		/* load */
		NULL,		/* save */
		NULL		/* edit */
	},
	SK_ViewDraw,
	SK_ViewScale
};

#define SK_VIEW_X(skv,px) ((SG_Real)(px - AGWIDGET(skv)->w/2)) / ((SG_Real)AGWIDGET(skv)->w/2.0)
#define SK_VIEW_Y(skv,py) ((SG_Real)(py - AGWIDGET(skv)->h/2)) / ((SG_Real)AGWIDGET(skv)->h/2.0)
#define SK_VIEW_X_SNAP(skv,px) (px)
#define SK_VIEW_Y_SNAP(skv,py) (py)
#define SK_VIEW_SCALE_X(skv) (skv)->mView.m[0][0]
#define SK_VIEW_SCALE_Y(skv) (skv)->mView.m[1][1]

SK_View *
SK_ViewNew(void *parent, SK *sk, Uint flags)
{
	SK_View *skv;

	skv = Malloc(sizeof(SK_View), M_OBJECT);
	SK_ViewInit(skv, sk, flags);
	AG_ObjectAttach(parent, sk);
	if (flags & SK_VIEW_FOCUS) {
		AG_WidgetFocus(skv);
	}
	return (skv);
}

static void
SK_ViewMotion(AG_Event *event)
{
	SK_View *skv = AG_SELF();
	SK_Tool *tool = skv->curtool;
	SG_Vector vPos, vRel;
	SG_Matrix Ti;

	vPos.x = SK_VIEW_X(skv, AG_INT(1));
	vPos.y = SK_VIEW_Y(skv, AGWIDGET(skv)->h - AG_INT(2));
	vPos.z = 0.0;
	vRel.x = (SG_Real)AG_INT(3) * skv->wPixel;
	vRel.y = -(SG_Real)AG_INT(4) * skv->hPixel;
	vRel.z = 0.0;

	if (SG_MatrixInvert(&skv->mView, &Ti) == -1) {
		fprintf(stderr, "mView: %s\n", AG_GetError());
		return;
	}
	SG_MatrixMultVectorv(&vPos, &Ti);

	AG_MutexLock(&skv->sk->lock);

	if (skv->mouse.panning) {
		SG_MatrixTranslate2(&skv->mView, vRel.x, vRel.y);
		goto out;
	}
	if (tool != NULL && tool->ops->mousemotion != NULL) {
		if ((tool->ops->flags & SK_MOUSEMOTION_NOSNAP) == 0) {
			vPos.x = SK_VIEW_X_SNAP(skv,vPos.x);
			vPos.y = SK_VIEW_Y_SNAP(skv,vPos.y);
		}
		tool->ops->mousemotion(tool, vPos, vRel, AG_INT(5));
	}
out:
	skv->mouse.last.x = vPos.x;
	skv->mouse.last.y = vPos.y;
	AG_MutexUnlock(&skv->sk->lock);
}

static void
SK_ViewButtonDown(AG_Event *event)
{
	SK_View *skv = AG_SELF();
	SK_Tool *tool = SK_CURTOOL(skv);
	int button = AG_INT(1);
	SG_Vector vPos;
	SG_Matrix Ti;

	vPos.x = SK_VIEW_X(skv, AG_INT(2));
	vPos.y = SK_VIEW_Y(skv, AGWIDGET(skv)->h - AG_INT(3));
	vPos.z = 0.0;

	if (SG_MatrixInvert(&skv->mView, &Ti) == -1) {
		fprintf(stderr, "mView: %s\n", AG_GetError());
		return;
	}
	SG_MatrixMultVectorv(&vPos, &Ti);

	AG_WidgetFocus(skv);
	AG_MutexLock(&skv->sk->lock);
	skv->mouse.last = vPos;
	
	switch (button) {
	case SDL_BUTTON_MIDDLE:
		skv->mouse.panning = 1;
		break;
	case SDL_BUTTON_WHEELDOWN:
		SK_ViewZoom(skv, SK_VIEW_SCALE_X(skv) -
		    logf(1.0+SK_VIEW_SCALE_X(skv))/3.0);
		goto out;
	case SDL_BUTTON_WHEELUP:
		SK_ViewZoom(skv, SK_VIEW_SCALE_X(skv) +
		    logf(1.0+SK_VIEW_SCALE_X(skv))/3.0);
		goto out;
	default:
		break;
	}
	if (tool != NULL && tool->ops->mousebuttondown != NULL) {
		if ((tool->ops->flags & SK_BUTTONDOWN_NOSNAP) == 0) {
			vPos.x = SK_VIEW_X_SNAP(skv, vPos.x);
			vPos.y = SK_VIEW_Y_SNAP(skv, vPos.y);
		}
		if (tool->ops->mousebuttondown(tool, vPos, button) == 1)
			goto out;
	}
	TAILQ_FOREACH(tool, &skv->tools, tools) {
		SK_ToolMouseBinding *mb;

		SLIST_FOREACH(mb, &tool->mbindings, mbindings) {
			if (mb->button != button) {
				continue;
			}
			tool->skv = skv;
			if (mb->func(tool, button, 1, vPos, mb->arg) == 1)
				goto out;
		}
	}
	if (skv->btndown_ev != NULL)
		AG_PostEvent(NULL, skv, skv->btndown_ev->name,
		    "%i,%f,%f", button, vPos.x, vPos.y);
out:
	AG_MutexUnlock(&skv->sk->lock);
}

static void
SK_ViewButtonUp(AG_Event *event)
{
	SK_View *skv = AG_SELF();
	SK_Tool *tool = SK_CURTOOL(skv);
	int button = AG_INT(1);
	SG_Vector vPos;
	SG_Matrix Ti;

	vPos.x = SK_VIEW_X(skv, AG_INT(2));
	vPos.y = SK_VIEW_Y(skv, AGWIDGET(skv)->h - AG_INT(3));
	vPos.z = 0.0;

	if (SG_MatrixInvert(&skv->mView, &Ti) == -1) {
		fprintf(stderr, "mView: %s\n", AG_GetError());
		return;
	}
	SG_MatrixMultVectorv(&vPos, &Ti);

	AG_MutexLock(&skv->sk->lock);

	if (tool != NULL && tool->ops->mousebuttonup != NULL) {
		if ((tool->ops->flags & SK_BUTTONUP_NOSNAP) == 0) {
			vPos.x = SK_VIEW_X_SNAP(skv, vPos.x);
			vPos.y = SK_VIEW_Y_SNAP(skv, vPos.y);
		}
		if (tool->ops->mousebuttonup(tool, vPos, button) == 1)
			goto out;
	}
	TAILQ_FOREACH(tool, &skv->tools, tools) {
		SK_ToolMouseBinding *mb;

		SLIST_FOREACH(mb, &tool->mbindings, mbindings) {
			if (mb->button != button) {
				continue;
			}
			tool->skv = skv;
			if (mb->func(tool, button, 0, vPos, mb->arg) == 1)
				goto out;
		}
	}
	switch (button) {
	case SDL_BUTTON_MIDDLE:
		skv->mouse.panning = 0;
		goto out;
	default:
		break;
	}
	if (skv->btnup_ev != NULL)
		AG_PostEvent(NULL, skv, skv->btnup_ev->name, "%i,%f,%f", button,
		    vPos.x, vPos.y);
out:
	AG_MutexUnlock(&skv->sk->lock);
}

void
SK_ViewInit(SK_View *skv, SK *sk, Uint flags)
{
	Uint wflags = AG_WIDGET_FOCUSABLE;

	if (flags & SK_VIEW_HFILL) wflags |= AG_WIDGET_HFILL;
	if (flags & SK_VIEW_VFILL) wflags |= AG_WIDGET_VFILL;

	AG_WidgetInit(skv, "skview", &skViewOps, wflags);
	
	if (!AG_Bool(agConfig, "view.opengl"))
		fatal("widget requires OpenGL mode");

	skv->sk = sk;
	skv->predraw_ev = NULL;
	skv->postdraw_ev = NULL;
	skv->scale_ev = NULL;
	skv->keydown_ev = NULL;
	skv->btndown_ev = NULL;
	skv->keyup_ev = NULL;
	skv->btnup_ev = NULL;
	skv->motion_ev = NULL;
	skv->status[0] = '\0';
	skv->mouse.last.x = 0;
	skv->mouse.last.y = 0;
	skv->mouse.panning = 0;
	skv->curtool = NULL;
	skv->deftool = NULL;
	skv->wPixel = 1.0;
	skv->hPixel = 1.0;
	SG_MatrixIdentityv(&skv->mView);
	SG_MatrixIdentityv(&skv->mProj);
	TAILQ_INIT(&skv->tools);

	AG_SetEvent(skv, "window-mousemotion", SK_ViewMotion, NULL);
	AG_SetEvent(skv, "window-mousebuttondown", SK_ViewButtonDown, NULL);
	AG_SetEvent(skv, "window-mousebuttonup", SK_ViewButtonUp, NULL);
	
	SK_ViewZoom(skv, 1.0/10.0);
}

void
SK_ViewDestroy(void *p)
{
}

void
SK_ViewPreDrawFn(SK_View *skv, AG_EventFn fn, const char *fmt, ...)
{
	skv->predraw_ev = AG_SetEvent(skv, NULL, fn, NULL);
	AG_EVENT_GET_ARGS(skv->predraw_ev, fmt);
}

void
SK_ViewPostDrawFn(SK_View *skv, AG_EventFn fn, const char *fmt, ...)
{
	skv->postdraw_ev = AG_SetEvent(skv, NULL, fn, NULL);
	AG_EVENT_GET_ARGS(skv->postdraw_ev, fmt);
}

void
SK_ViewScaleFn(SK_View *skv, AG_EventFn fn, const char *fmt, ...)
{
	skv->scale_ev = AG_SetEvent(skv, NULL, fn, NULL);
	AG_EVENT_GET_ARGS(skv->scale_ev, fmt);
}

void
SK_ViewKeydownFn(SK_View *skv, AG_EventFn fn, const char *fmt, ...)
{
	skv->keydown_ev = AG_SetEvent(skv, "window-keydown", fn, NULL);
	AG_EVENT_GET_ARGS(skv->keydown_ev, fmt);
}

void
SK_ViewKeyupFn(SK_View *skv, AG_EventFn fn, const char *fmt, ...)
{
	skv->keyup_ev = AG_SetEvent(skv, "window-keyup", fn, NULL);
	AG_EVENT_GET_ARGS(skv->keyup_ev, fmt);
}

void
SK_ViewButtondownFn(SK_View *skv, AG_EventFn fn, const char *fmt, ...)
{
	skv->btndown_ev = AG_SetEvent(skv, NULL, fn, NULL);
	AG_EVENT_GET_ARGS(skv->btndown_ev, fmt);
}

void
SK_ViewButtonupFn(SK_View *skv, AG_EventFn fn, const char *fmt, ...)
{
	skv->btnup_ev = AG_SetEvent(skv, "window-mousebuttonup", fn, NULL);
	AG_EVENT_GET_ARGS(skv->btnup_ev, fmt);
}

void
SK_ViewMotionFn(SK_View *skv, AG_EventFn fn, const char *fmt, ...)
{
	skv->motion_ev = AG_SetEvent(skv, "window-mousemotion", fn, NULL);
	AG_EVENT_GET_ARGS(skv->motion_ev, fmt);
}

void
SK_ViewScale(void *p, int w, int h)
{
	SK_View *skv = p;

	if (w == -1 && h == -1) {
		AGWIDGET(skv)->w = 32;		/* XXX */
		AGWIDGET(skv)->h = 32;
	} else {
		AGWIDGET(skv)->w = w;
		AGWIDGET(skv)->h = h;
	}

	SG_MatrixIdentityv(&skv->mProj);
	SK_ViewZoom(skv, 0.0);
}

void
SK_ViewZoom(SK_View *skv, SG_Real zoom)
{
	if (zoom != 0.0) {
		SK_VIEW_SCALE_X(skv) = zoom >= 0.01 ? zoom : 0.01;
		SK_VIEW_SCALE_Y(skv) = zoom >= 0.01 ? zoom : 0.01;
	}
	skv->wPixel = 1.0/((SG_Real)AGWIDGET(skv)->w)*2.0/SK_VIEW_SCALE_X(skv);
	skv->hPixel = 1.0/((SG_Real)AGWIDGET(skv)->h)*2.0/SK_VIEW_SCALE_Y(skv);
}

void
SK_ViewDraw(void *p)
{
	SK_View *skv = p;
	SK *sk = skv->sk;
	SDL_Surface *status;

	glViewport(
	    AGWIDGET(skv)->cx, agView->h - AGWIDGET(skv)->cy2,
	    AGWIDGET(skv)->w, AGWIDGET(skv)->h);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	SG_LoadMatrixGL(&skv->mProj);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	SG_LoadMatrixGL(&skv->mView);

	glPushAttrib(GL_ENABLE_BIT|GL_POLYGON_BIT);

	glDisable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glPolygonMode(GL_FRONT, GL_FILL);
	glShadeModel(GL_FLAT);
	
	if (skv->predraw_ev != NULL)
		skv->predraw_ev->handler(skv->predraw_ev);

	SK_RenderNode(sk, (SK_Node *)sk->root, skv);
	SK_RenderAbsolute(sk, skv);

	if (skv->postdraw_ev != NULL)
		skv->postdraw_ev->handler(skv->postdraw_ev);

	glPopAttrib();

	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	
	glViewport(0, 0, agView->w, agView->h);
#if 0
	status = AG_TextRender(NULL, -1, AG_COLOR(TEXT_COLOR), skv->status);
	AG_WidgetBlit(skv, status, 0, AGWIDGET(skv)->h - status->h);
	SDL_FreeSurface(status);
#endif
}

void
SK_ViewSelectTool(SK_View *skv, SK_Tool *ntool, void *p)
{
	AG_Window *pwin;

	if (skv->curtool != NULL) {
		if (skv->curtool->trigger != NULL) {
			AG_WidgetSetBool(skv->curtool->trigger, "state", 0);
		}
		if (skv->curtool->win != NULL) {
			AG_WindowHide(skv->curtool->win);
		}
		if (skv->curtool->pane != NULL) {
			AG_Widget *wt;
			AG_Window *pwin;

			AGOBJECT_FOREACH_CHILD(wt, skv->curtool->pane,
			    ag_widget) {
				AG_ObjectDetach(wt);
				AG_ObjectDestroy(wt);
				Free(wt, M_OBJECT);
			}
			if ((pwin = AG_WidgetParentWindow(skv->curtool->pane))
			    != NULL) {
				AG_WINDOW_UPDATE(pwin);
			}
		}
		skv->curtool->skv = NULL;
	}
	skv->curtool = ntool;

	if (ntool != NULL) {
		ntool->p = p;
		ntool->skv = skv;

		if (ntool->trigger != NULL) {
			AG_WidgetSetBool(ntool->trigger, "state", 1);
		}
		if (ntool->win != NULL) {
			AG_WindowShow(ntool->win);
		}
#if 0
		if (ntool->pane != NULL && ntool->ops->edit != NULL) {
			AG_Window *pwin;

			ntool->ops->edit(ntool, ntool->pane);
			if ((pwin = AG_WidgetParentWindow(skv->curtool->pane))
			    != NULL) {
				AG_WINDOW_UPDATE(pwin);
			}
		}
#endif
		snprintf(skv->status, sizeof(skv->status), _("Tool: %s"),
		    ntool->ops->name);
	} else {
		skv->status[0] = '\0';
	}

//	if ((pwin = AG_WidgetParentWindow(skv)) != NULL) {
//		agView->focus_win = pwin;
//		AG_WidgetFocus(skv);
//	}
}

SK_Tool *
SK_ViewFindTool(SK_View *skv, const char *name)
{
	SK_Tool *tool;

	TAILQ_FOREACH(tool, &skv->tools, tools) {
		if (strcmp(tool->ops->name, name) == 0)
			return (tool);
	}
	return (NULL);
}

SK_Tool *
SK_ViewFindToolByOps(SK_View *skv, const SK_ToolOps *ops)
{
	SK_Tool *tool;

	TAILQ_FOREACH(tool, &skv->tools, tools) {
		if (tool->ops == ops)
			return (tool);
	}
	return (NULL);
}

SK_Tool *
SK_ViewRegTool(SK_View *skv, const SK_ToolOps *ops, void *p)
{
	SK_Tool *t;

	t = Malloc(ops->len, M_MAPEDIT);
	t->ops = ops;
	t->skv = skv;
	t->p = p;
	SK_ToolInit(t);
	TAILQ_INSERT_TAIL(&skv->tools, t, tools);
	return (t);
}

void
SK_ViewSetDefaultTool(SK_View *skv, SK_Tool *tool)
{
	skv->deftool = tool;
}