/*
 * Copyright (c) 2007 Hypertriton, Inc. <http://hypertriton.com/>
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

#include <core/core.h>
#include <core/view.h>

#include "graph.h"

#include <gui/primitive.h>
#include <gui/menu.h>

#include <stdarg.h>
#include <string.h>

static AG_WidgetOps agGraphOps = {
	{
		"AG_Widget:AG_Graph",
		sizeof(AG_Graph),
		{ 0,0 },
		NULL,			/* init */
		NULL,			/* reinit */
		AG_GraphDestroy,
		NULL,			/* load */
		NULL,			/* save */
		NULL			/* edit */
	},
	AG_GraphDraw,
	AG_GraphScale
};

AG_Graph *
AG_GraphNew(void *parent, Uint flags)
{
	AG_Graph *gf;

	gf = Malloc(sizeof(AG_Graph), M_OBJECT);
	AG_GraphInit(gf, flags);
	AG_ObjectAttach(parent, gf);
	return (gf);
}

static void
keydown(AG_Event *event)
{
	AG_Graph *gf = AG_SELF();
	int keysym = AG_INT(1);
	const int scrollIncr = 10;

	switch (keysym) {
	case SDLK_LEFT:
		gf->xOffs -= scrollIncr;
		break;
	case SDLK_RIGHT:
		gf->xOffs += scrollIncr;
		break;
	case SDLK_UP:
		gf->yOffs -= scrollIncr;
		break;
	case SDLK_DOWN:
		gf->yOffs += scrollIncr;
		break;
	case SDLK_0:
		gf->xOffs = 0;
		gf->yOffs = 0;
		break;
	}
}

static __inline__ int
MouseOverVertex(AG_GraphVertex *vtx, int x, int y)
{
	return (abs(x - vtx->x + vtx->graph->xOffs) <= vtx->w/2 &&
	        abs(y - vtx->y + vtx->graph->yOffs) <= vtx->h/2);
}

static __inline__ void
GetEdgeLabelCoords(AG_GraphEdge *edge, int *x, int *y)
{
	*x = (edge->v1->x + edge->v2->x)/2;
	*y = (edge->v1->y + edge->v2->y)/2;
}

static __inline__ int
MouseOverEdge(AG_GraphEdge *edge, int x, int y)
{
	int lx, ly;
	SDL_Surface *lbl;

	if (edge->labelSu == -1) {
		return (0);
	}
	GetEdgeLabelCoords(edge, &lx, &ly);
	lbl = AGWIDGET_SURFACE(edge->graph,edge->labelSu);
	return (abs(x - lx + edge->graph->xOffs) <= lbl->w/2 &&
	        abs(y - ly + edge->graph->yOffs) <= lbl->h/2);
}

static void
mousemotion(AG_Event *event)
{
	AG_Graph *gf = AG_SELF();
	int x = AG_INT(1);
	int y = AG_INT(2);
	int dx = AG_INT(3);
	int dy = AG_INT(4);
	int state = AG_INT(5);
	AG_GraphVertex *vtx;
	AG_GraphEdge *edge;

	if (gf->flags & AG_GRAPH_PANNING) {
		gf->xOffs -= dx;
		gf->yOffs -= dy;
		return;
	}
	if (gf->flags & AG_GRAPH_DRAGGING) {
		TAILQ_FOREACH(vtx, &gf->vertices, vertices) {
			if (vtx->flags & AG_GRAPH_SELECTED) {
				AG_GraphVertexPosition(vtx,
				    vtx->x + dx,
				    vtx->y + dy);
			}
		}
	} else {
		TAILQ_FOREACH(vtx, &gf->vertices, vertices) {
			if (MouseOverVertex(vtx, x, y)) {
				vtx->flags |= AG_GRAPH_MOUSEOVER;
			} else {
				vtx->flags &= ~AG_GRAPH_MOUSEOVER;
			}
		}
		TAILQ_FOREACH(edge, &gf->edges, edges) {
			if (MouseOverEdge(edge, x, y)) {
				edge->flags |= AG_GRAPH_MOUSEOVER;
			} else {
				edge->flags &= ~AG_GRAPH_MOUSEOVER;
			}
		}
	}
}

static void
mousebuttonup(AG_Event *event)
{
	AG_Graph *gf = AG_SELF();
	int button = AG_INT(1);

	switch (button) {
	case SDL_BUTTON_LEFT:
		gf->flags &= ~(AG_GRAPH_DRAGGING);
		break;
	case SDL_BUTTON_MIDDLE:
		gf->flags &= ~(AG_GRAPH_PANNING);
		break;
	}
}

AG_GraphEdge *
AG_GraphEdgeFind(AG_Graph *gf, void *userPtr)
{
	AG_GraphEdge *edge;

	TAILQ_FOREACH(edge, &gf->edges, edges) {
		if (edge->userPtr == userPtr)
			return (edge);
	}
	return (NULL);
}

AG_GraphEdge *
AG_GraphEdgeNew(AG_Graph *gf, AG_GraphVertex *v1, AG_GraphVertex *v2,
    void *userPtr)
{
	AG_GraphEdge *edge;

	TAILQ_FOREACH(edge, &gf->edges, edges) {
		if (edge->v1 == v1 && edge->v2 == v2) {
			AG_SetError(_("Existing edge"));
			return (NULL);
		}
	}
	edge = Malloc(sizeof(AG_GraphEdge), M_WIDGET);
	edge->labelTxt[0] = '\0';
	edge->labelSu = -1;
	edge->edgeColor = SDL_MapRGB(agSurfaceFmt, 0,0,0);
	edge->labelColor = SDL_MapRGB(agSurfaceFmt, 0,0,0);
	edge->flags = 0;
	edge->v1 = v1;
	edge->v2 = v2;
	edge->userPtr = userPtr;
	edge->graph = gf;
	TAILQ_INSERT_TAIL(&gf->edges, edge, edges);
	gf->nedges++;

	edge->v1->edges = Realloc(edge->v1->edges, (edge->v1->nedges + 1) *
	                                           sizeof(AG_GraphEdge *));
	edge->v2->edges = Realloc(edge->v2->edges, (edge->v2->nedges + 1) *
	                                           sizeof(AG_GraphEdge *));
	edge->v1->edges[edge->v1->nedges++] = edge;
	edge->v2->edges[edge->v2->nedges++] = edge;
	return (edge);
}

void
AG_GraphEdgeFree(AG_GraphEdge *edge)
{
	if (edge->labelSu != -1) {
		AG_WidgetUnmapSurface(edge->graph, edge->labelSu);
	}
	Free(edge,M_WIDGET);
}

void
AG_GraphEdgeLabel(AG_GraphEdge *ge, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(ge->labelTxt, sizeof(ge->labelTxt), fmt, ap);
	va_end(ap);

	if (ge->labelSu >= 0) {
		AG_WidgetUnmapSurface(ge->graph, ge->labelSu);
	}
	ge->labelSu = AG_WidgetMapSurface(ge->graph,
	    AG_TextRender(NULL, -1, AG_VideoPixel(ge->labelColor),
	    ge->labelTxt));
}

void
AG_GraphEdgeColorLabel(AG_GraphEdge *edge, Uint8 r, Uint8 g, Uint8 b)
{
	edge->labelColor = SDL_MapRGB(agSurfaceFmt, r, g, b);
}

void
AG_GraphEdgeColor(AG_GraphEdge *edge, Uint8 r, Uint8 g, Uint8 b)
{
	edge->edgeColor = SDL_MapRGB(agSurfaceFmt, r, g, b);
}

static void
SetVertexStyle(AG_Event *event)
{
	AG_GraphVertex *vtx = AG_PTR(1);
	int style = AG_INT(2);

	AG_GraphVertexStyle(vtx, (enum ag_graph_vertex_style)style);
}

static void
mousebuttondown(AG_Event *event)
{
	AG_Graph *gf = AG_SELF();
	int button = AG_INT(1);
	int x = AG_INT(2);
	int y = AG_INT(3);
	SDLMod mod = SDL_GetModState();
	AG_GraphVertex *vtx, *vtx2;
	AG_GraphEdge *edge, *edge2;
	AG_PopupMenu *pm;

	AG_WidgetFocus(gf);

	switch (button) {
	case SDL_BUTTON_MIDDLE:
		gf->flags |= AG_GRAPH_PANNING;
		break;
	case SDL_BUTTON_LEFT:
		if (mod & (KMOD_CTRL|KMOD_SHIFT)) {
			TAILQ_FOREACH(edge, &gf->edges, edges) {
				if (!MouseOverEdge(edge, x, y)) {
					continue;
				}
				if (edge->flags & AG_GRAPH_SELECTED) {
					edge->flags &= ~AG_GRAPH_SELECTED;
				} else {
					edge->flags |= AG_GRAPH_SELECTED;
				}
			}
			TAILQ_FOREACH(vtx, &gf->vertices, vertices) {
				if (!MouseOverVertex(vtx, x, y)) {
					continue;
				}
				if (vtx->flags & AG_GRAPH_SELECTED) {
					vtx->flags &= ~AG_GRAPH_SELECTED;
				} else {
					vtx->flags |= AG_GRAPH_SELECTED;
				}
			}
		} else {
			TAILQ_FOREACH(edge, &gf->edges, edges) {
				if (MouseOverEdge(edge, x, y))
					break;
			}
			if (edge != NULL) {
				TAILQ_FOREACH(edge2, &gf->edges, edges) {
					edge2->flags &= ~AG_GRAPH_SELECTED;
				}
				edge->flags |= AG_GRAPH_SELECTED;
			}
			TAILQ_FOREACH(vtx, &gf->vertices, vertices) {
				if (MouseOverVertex(vtx, x, y))
					break;
			}
			if (vtx != NULL) {
				TAILQ_FOREACH(vtx2, &gf->vertices, vertices) {
					vtx2->flags &= ~AG_GRAPH_SELECTED;
				}
				vtx->flags |= AG_GRAPH_SELECTED;
			}
		}
		gf->flags |= AG_GRAPH_DRAGGING;
		break;
	case SDL_BUTTON_RIGHT:
		TAILQ_FOREACH(vtx, &gf->vertices, vertices) {
			if (!MouseOverVertex(vtx, x, y)) {
				continue;
			}
			pm = AG_PopupNew(gf);
			AG_MenuIntFlags(pm->item, _("Hide vertex"), -1,
			    &vtx->flags, AG_GRAPH_HIDDEN, 1);
			AG_MenuSeparator(pm->item);
			AG_MenuAction(pm->item, _("Rectangular"), -1,
			    SetVertexStyle, "%p,%i", vtx, AG_GRAPH_RECTANGLE);
			AG_MenuAction(pm->item, _("Circular"), -1,
			    SetVertexStyle, "%p,%i", vtx, AG_GRAPH_CIRCLE);
			AG_PopupShow(pm);
			break;
		}
		TAILQ_FOREACH(edge, &gf->edges, edges) {
			if (!MouseOverEdge(edge, x, y)) {
				continue;
			}
			pm = AG_PopupNew(gf);
			AG_MenuIntFlags(pm->item, _("Hide edge"), -1,
			    &edge->flags, AG_GRAPH_HIDDEN, 1);
			AG_PopupShow(pm);
			break;
		}
	default:
		break;
	}
}

void
AG_GraphInit(AG_Graph *gf, Uint flags)
{
	Uint wflags = AG_WIDGET_CLIPPING|AG_WIDGET_FOCUSABLE;
	int i;

	if (flags & AG_GRAPH_HFILL) wflags |= AG_WIDGET_HFILL;
	if (flags & AG_GRAPH_VFILL) wflags |= AG_WIDGET_VFILL;

	AG_WidgetInit(gf, &agGraphOps, wflags);
	gf->flags = flags;
	gf->wPre = 128;
	gf->hPre = 128;
	gf->xOffs = 0;
	gf->yOffs = 0;
	gf->xMin = 0;
	gf->yMin = 0;
	gf->xMax = 0;
	gf->yMax = 0;
	TAILQ_INIT(&gf->vertices);
	TAILQ_INIT(&gf->edges);
	gf->nvertices = 0;
	gf->nedges = 0;
	gf->pxMin = 0;
	gf->pxMax = 0;
	gf->pyMin = 0;
	gf->pyMax = 0;

#if 0
	gf->hbar = AG_ScrollbarNew(gf, AG_SCROLLBAR_HORIZ, 0);
	gf->vbar = AG_ScrollbarNew(gf, AG_SCROLLBAR_VERT, 0);
	AG_WidgetBind(gf->hbar, "value", AG_WIDGET_INT, &gf->xOffs);
	AG_WidgetBind(gf->hbar, "min", AG_WIDGET_INT, &gf->xMin);
	AG_WidgetBind(gf->hbar, "max", AG_WIDGET_INT, &gf->xMax);
	AG_WidgetBind(gf->hbar, "visible", AG_WIDGET_INT, &AGWIDGET(gf)->w);

	AG_WidgetBind(gf->vbar, "value", AG_WIDGET_INT, &gf->yOffs);
	AG_WidgetBind(gf->vbar, "min", AG_WIDGET_INT, &gf->yMin);
	AG_WidgetBind(gf->vbar, "max", AG_WIDGET_INT, &gf->yMax);
	AG_WidgetBind(gf->vbar, "visible", AG_WIDGET_INT, &AGWIDGET(gf)->h);
#endif

	AG_SetEvent(gf, "window-keydown", keydown, NULL);
	AG_SetEvent(gf, "window-mousebuttondown", mousebuttondown, NULL);
	AG_SetEvent(gf, "window-mousebuttonup", mousebuttonup, NULL);
	AG_SetEvent(gf, "window-mousemotion", mousemotion, NULL);
}

void
AG_GraphFreeVertices(AG_Graph *gf)
{
	AG_GraphVertex *vtx, *vtxNext;
	AG_GraphEdge *edge, *edgeNext;

	for (vtx = TAILQ_FIRST(&gf->vertices);
	     vtx != TAILQ_END(&gf->vertices);
	     vtx = vtxNext) {
		vtxNext = TAILQ_NEXT(vtx, vertices);
		AG_GraphVertexFree(vtx);
	}
	for (edge = TAILQ_FIRST(&gf->edges);
	     edge != TAILQ_END(&gf->edges);
	     edge = edgeNext) {
		edgeNext = TAILQ_NEXT(edge, edges);
		AG_GraphEdgeFree(edge);
	}
	TAILQ_INIT(&gf->vertices);
	TAILQ_INIT(&gf->edges);
	gf->nvertices = 0;
	gf->nedges = 0;
	gf->xMin = 0;
	gf->xMax = 0;
	gf->yMin = 0;
	gf->yMax = 0;
	gf->xOffs = 0;
	gf->yOffs = 0;
	gf->flags &= ~(AG_GRAPH_DRAGGING);
}

void
AG_GraphDestroy(void *p)
{
	AG_Graph *gf = p;

	AG_GraphFreeVertices(gf);
	AG_WidgetDestroy(gf);
}

void
AG_GraphPrescale(AG_Graph *gf, Uint w, Uint h)
{
	gf->wPre = w;
	gf->hPre = h;
}

void
AG_GraphScale(void *p, int w, int h)
{
	AG_Graph *gf = p;

	if (w == -1 && h == -1) {
		AGWIDGET(gf)->w = gf->wPre;
		AGWIDGET(gf)->h = gf->hPre;
		return;
	}
#if 0
	AGWIDGET(gf->hbar)->x = 0;
	AGWIDGET(gf->hbar)->y = AGWIDGET(gf)->h - gf->hbar->bw;
	AGWIDGET(gf->hbar)->w = AGWIDGET(gf)->w;
	AGWIDGET(gf->hbar)->h = gf->hbar->bw;
	AGWIDGET(gf->vbar)->x = AGWIDGET(gf)->w - gf->vbar->bw;
	AGWIDGET(gf->vbar)->y = gf->vbar->bw;
	AGWIDGET(gf->vbar)->w = gf->vbar->bw;
	AGWIDGET(gf->vbar)->h = AGWIDGET(gf)->h - gf->vbar->bw;
#endif
}

void
AG_GraphDraw(void *p)
{
	AG_Graph *gf = p;
	AG_GraphVertex *vtx;
	AG_GraphEdge *edge;
	int x0 = AGWIDGET(gf)->w/2;
	int y0 = AGWIDGET(gf)->h/2;
	int x, y;
	Uint i;
	Uint8 bg[4];

	agPrim.rect_outlined(gf,
	    gf->pxMin - gf->xOffs,
	    gf->pyMin - gf->yOffs, 
	    gf->pxMax - gf->pxMin,
	    gf->pyMax - gf->pyMin,
	    SDL_MapRGB(agVideoFmt, 128, 128, 128)); 

	/* Draw the edges */
	TAILQ_FOREACH(edge, &gf->edges, edges) {
		if (edge->flags & AG_GRAPH_HIDDEN) {
			continue;
		}
		agPrim.line(gf,
		    edge->v1->x - gf->xOffs,
		    edge->v1->y - gf->yOffs,
		    edge->v2->x - gf->xOffs,
		    edge->v2->y - gf->yOffs,
		    edge->edgeColor);

		if (edge->labelSu >= 0) {
			SDL_Surface *su = AGWIDGET_SURFACE(gf,edge->labelSu);
			int lblX, lblY;

			GetEdgeLabelCoords(edge, &lblX, &lblY);
			lblX -= gf->xOffs + su->w/2;
			lblY -= gf->yOffs + su->h/2;

			if (edge->flags & AG_GRAPH_SELECTED) {
				agPrim.rect_outlined(gf,
				    lblX-1, lblY-1,
				    su->w+2, su->h+2,
				    AG_VideoPixel(edge->labelColor));
			}
			if (edge->flags & AG_GRAPH_MOUSEOVER) {
				agPrim.rect_outlined(gf,
				    lblX-2, lblY-2,
				    su->w+4, su->h+4,
				    AG_COLOR(TEXT_COLOR));
			}
			bg[0] = 128;
			bg[1] = 128;
			bg[2] = 128;
			bg[3] = 128;
			agPrim.rect_blended(gf, lblX, lblY, su->w, su->h,
			    bg, AG_ALPHA_SRC);
			AG_WidgetBlitSurface(gf, edge->labelSu, lblX, lblY);
		}
	}

	/* Draw the vertices. */
	TAILQ_FOREACH(vtx, &gf->vertices, vertices) {
		SDL_Surface *lbl = AGWIDGET_SURFACE(gf, vtx->labelSu);

		if (vtx->flags & AG_GRAPH_HIDDEN) {
			continue;
		}
		SDL_GetRGBA(vtx->bgColor, agSurfaceFmt,
		    &bg[0], &bg[1], &bg[2], &bg[3]);
		switch (vtx->style) {
		case AG_GRAPH_RECTANGLE:
			agPrim.rect_blended(gf,
			    vtx->x - vtx->w/2 - gf->xOffs,
			    vtx->y - vtx->h/2 - gf->yOffs,
			    vtx->w,
			    vtx->h,
			    bg, AG_ALPHA_SRC);
			if (vtx->flags & AG_GRAPH_SELECTED) {
				agPrim.rect_outlined(gf,
				    vtx->x - vtx->w/2 - gf->xOffs - 1,
				    vtx->y - vtx->h/2 - gf->yOffs - 1,
				    vtx->w + 2,
				    vtx->h + 2,
				    SDL_MapRGB(agVideoFmt, 0,0,255));
			}
			if (vtx->flags & AG_GRAPH_MOUSEOVER) {
				agPrim.rect_outlined(gf,
				    vtx->x - vtx->w/2 - gf->xOffs - 2,
				    vtx->y - vtx->h/2 - gf->yOffs - 2,
				    vtx->w + 4,
				    vtx->h + 4,
				    SDL_MapRGB(agVideoFmt, 255,0,0));
			}
			break;
		case AG_GRAPH_CIRCLE:
			agPrim.circle(gf,
			    vtx->x - gf->xOffs,
			    vtx->y - gf->yOffs,
			    MAX(vtx->w,vtx->h)/2,
			    vtx->bgColor);
			break;
		}
		if (vtx->labelSu >= 0) {
			AG_WidgetBlitSurface(gf, vtx->labelSu,
			    vtx->x - lbl->w/2 - gf->xOffs,
			    vtx->y - lbl->h/2 - gf->yOffs);
		}
	}
}

AG_GraphVertex *
AG_GraphVertexFind(AG_Graph *gf, void *userPtr)
{
	AG_GraphVertex *vtx;

	TAILQ_FOREACH(vtx, &gf->vertices, vertices) {
		if (vtx->userPtr == userPtr)
			return (vtx);
	}
	return (NULL);
}

AG_GraphVertex *
AG_GraphVertexNew(AG_Graph *gf, void *userPtr)
{
	AG_GraphVertex *vtx;
	
	vtx = Malloc(sizeof(AG_GraphVertex), M_WIDGET);
	vtx->labelTxt[0] = '\0';
	vtx->labelSu = -1;
	vtx->labelColor = SDL_MapRGB(agSurfaceFmt, 0,0,0);
	vtx->bgColor = SDL_MapRGBA(agSurfaceFmt, 255,255,255,128);
	vtx->style = AG_GRAPH_RECTANGLE;
	vtx->flags = 0;
	vtx->x = 0;
	vtx->y = 0;
	vtx->w = 32;
	vtx->h = 32;
	vtx->graph = gf;
	vtx->userPtr = userPtr;
	vtx->edges = NULL;
	vtx->nedges = 0;
	TAILQ_INSERT_TAIL(&gf->vertices, vtx, vertices);
	gf->nvertices++;
	return (vtx);
}

void
AG_GraphVertexFree(AG_GraphVertex *vtx)
{
	if (vtx->labelSu != -1) {
		AG_WidgetUnmapSurface(vtx->graph, vtx->labelSu);
	}
	Free(vtx->edges,M_WIDGET);
	Free(vtx,M_WIDGET);
}

void
AG_GraphVertexColorLabel(AG_GraphVertex *vtx, Uint8 r, Uint8 g, Uint8 b)
{
	vtx->labelColor = SDL_MapRGB(agSurfaceFmt, r, g, b);
}

void
AG_GraphVertexColorBG(AG_GraphVertex *vtx, Uint8 r, Uint8 g, Uint8 b)
{
	vtx->bgColor = SDL_MapRGB(agSurfaceFmt, r, g, b);
}

void
AG_GraphVertexLabel(AG_GraphVertex *vtx, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(vtx->labelTxt, sizeof(vtx->labelTxt), fmt, ap);
	va_end(ap);

	if (vtx->labelSu >= 0) {
		AG_WidgetUnmapSurface(vtx->graph, vtx->labelSu);
	}
	vtx->labelSu = AG_WidgetMapSurface(vtx->graph,
	    AG_TextRender(NULL, -1, AG_VideoPixel(vtx->labelColor),
	    vtx->labelTxt));
}

void
AG_GraphVertexPosition(AG_GraphVertex *vtx, int x, int y)
{
	AG_Graph *gf = vtx->graph;

	vtx->x = x;
	vtx->y = y;

	if (x < gf->xMin) { gf->xMin = x; }
	if (y < gf->yMin) { gf->yMin = y; }
	if (x > gf->xMax) { gf->xMax = x; }
	if (y > gf->yMax) { gf->yMax = y; }
}

void
AG_GraphVertexSize(AG_GraphVertex *vtx, Uint w, Uint h)
{
	vtx->w = w;
	vtx->h = h;
}

void
AG_GraphVertexStyle(AG_GraphVertex *vtx, enum ag_graph_vertex_style style)
{
	vtx->style = style;
}

static int 
CompareVertices(const void *p1, const void *p2)
{
	const AG_GraphVertex *v1 = *(const void **)p1;
	const AG_GraphVertex *v2 = *(const void **)p2;

	return (v2->nedges - v1->nedges);
}

static AG_GraphVertex *
VertexAtCoords(AG_Graph *gf, int x, int y)
{
	AG_GraphVertex *vtx;

	TAILQ_FOREACH(vtx, &gf->vertices, vertices) {
		if (vtx->x == x && vtx->y == y)
			return (vtx);
	}
	return (NULL);
}

static void
PlaceVertex(AG_Graph *gf, AG_GraphVertex *vtx, AG_GraphVertex **vSorted,
    int x, int y)
{
	int ox, oy;
	int i;

	vtx->x = x;
	vtx->y = y;
	vtx->flags |= AG_GRAPH_AUTOPLACED;

	if (x < gf->pxMin) { gf->pxMin = x; }
	if (x > gf->pxMax) { gf->pxMax = x; }
	if (y < gf->pyMin) { gf->pyMin = y; }
	if (y > gf->pyMax) { gf->pyMax = y; }

	for (i = 0; i < vtx->nedges; i++) {
		AG_GraphEdge *edge = vtx->edges[i];
		AG_GraphVertex *oVtx;
		float r = 128.0;
		float theta = 0.0;

		if (edge->v1 == vtx) { oVtx = edge->v2; }
		else { oVtx = edge->v1; }

		if (oVtx->flags & AG_GRAPH_AUTOPLACED) {
			continue;
		}
		for (;;) {
			ox = (int)(r*cosf(theta));
			oy = (int)(r*sinf(theta));
			if (VertexAtCoords(gf, ox, oy) == NULL) {
				PlaceVertex(gf, oVtx, vSorted, ox, oy);
				break;
			}
			theta += (M_PI*2.0)/6;
			if (theta >= (M_PI*2.0)) {
				r += 64.0;
			}
		}
	}
}

/*
 * Try to position the vertices such that they are distributed evenly
 * throughout a given bounding box, with a minimal number of edges
 * crossing each other.
 */
void
AG_GraphAutoPlace(AG_Graph *gf, Uint w, Uint h)
{
	AG_GraphVertex **vSorted, *vtx;
	int nSorted = 0, i;
	int tx, ty, tOffs;

	if (gf->nvertices == 0 || gf->nedges == 0)
		return;

	/* Sort the vertices based on their number of connected edges. */
	vSorted = Malloc(gf->nvertices*sizeof(AG_GraphVertex *), M_WIDGET);
	TAILQ_FOREACH(vtx, &gf->vertices, vertices) {
		vtx->flags &= ~(AG_GRAPH_AUTOPLACED);
		vSorted[nSorted++] = vtx;
	}
	qsort(vSorted, (size_t)nSorted, sizeof(AG_GraphVertex *),
	    CompareVertices);
	gf->pxMin = 0;
	gf->pxMax = 0;
	gf->pyMin = 0;
	gf->pyMax = 0;
	for (i = 0; i < nSorted; i++) {
		if (vSorted[i]->flags & AG_GRAPH_AUTOPLACED) {
			continue;
		}
		for (tx = gf->pxMax+128, ty = 0;
		     ty <= gf->pyMax;
		     ty += 64) {
			if (VertexAtCoords(gf, tx, ty) == NULL) {
				PlaceVertex(gf, vSorted[i], vSorted, tx, ty);
				break;
			}
		}
		if (ty <= gf->pyMax) {
			continue;
		}
		for (tx = gf->pxMin-128, ty = 0;
		     ty <= gf->pyMax;
		     ty += 64) {
			if (VertexAtCoords(gf, tx, ty) == NULL) {
				PlaceVertex(gf, vSorted[i], vSorted, tx, ty);
				break;
			}
		}
		if (ty <= gf->pyMax) {
			continue;
		}
	}
	Free(vSorted, M_WIDGET);
}
