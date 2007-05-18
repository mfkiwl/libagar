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

#include "sg.h"

#include <GL/gl.h>
#include <GL/glu.h>

const AG_WidgetOps sgViewOps = {
	{
		"AG_Widget:AG_GLView:SG_View",
		sizeof(SG_View),
		{ 0,0 },
		NULL,		/* init */
		NULL,		/* reinit */
		SG_ViewDestroy,
		NULL,		/* load */
		NULL,		/* save */
		NULL		/* edit */
	},
	AG_GLViewDraw,
	AG_GLViewScale
};

SG_View	*
SG_ViewNew(void *parent, SG *sg, Uint flags)
{
	SG_View *sv;

	sv = Malloc(sizeof(SG_View), M_OBJECT);
	SG_ViewInit(sv, sg, flags);
	AG_ObjectAttach(parent, sv);
	return (sv);
}

int
SG_UnProject(SG_Real wx, SG_Real wy, SG_Real wz, const SG_Matrix *M,
    const SG_Matrix *P, int *vp, SG_Vector *vOut)
{
	SG_Matrix A;
	SG_Vector4 in, out;

	SG_MatrixMultpv(&A, M, P);
	in.x = wx;
	in.y = wy;
	in.z = wz;
	in.w = 1.0;
	in.x = (in.x - vp[0]) / vp[2];
	in.y = (in.y - vp[1]) / vp[3];
	in.x = in.x*2.0 - 1.0;
	in.y = in.y*2.0 - 1.0;
	in.z = in.z*2.0 - 1.0;

	out = SG_MatrixMultVector4p(&A, &in);
	if (out.w == 0.0) { return (-1); }
	out.x /= out.w;
	out.y /= out.w;
	out.z /= out.w;
	vOut->x = out.x;
	vOut->y = out.y;
	vOut->z = out.z;
	return (0);
}

void
SG_ViewUpdateProjection(SG_View *sv)
{
	SG_Matrix P;
	int m, n;

	SG_CameraGetProjection(sv->cam, &P);
	for (m = 0; m < 4; m++) {
		for (n = 0; n < 4; n++)
			AGGLVIEW(sv)->mProjection[(m<<2)+n] = P.m[n][m];
	}
}

int
SG_ViewUnProject(SG_View *sv, SG_Vector w, SG_Real zNear, SG_Real zFar,
    SG_Vector *vOut)
{
	SG_Matrix T, P;
	SG_Real y, zNearSv, zFarSv;
	int vp[4];

	vp[0] = AGWIDGET(sv)->cx;
	vp[1] = agView->h - AGWIDGET(sv)->cy2;
	vp[2] = AGWIDGET(sv)->w;
	vp[3] = AGWIDGET(sv)->h;
	y = (SG_Real)(agView->h - AGWIDGET(sv)->cy2 + AGWIDGET(sv)->h - w.y);

	zNearSv = sv->cam->zNear; sv->cam->zNear = zNear;
	zFarSv = sv->cam->zFar; sv->cam->zFar = zFar;
	SG_CameraGetProjection(sv->cam, &P);
	sv->cam->zNear = zNearSv;
	sv->cam->zFar = zFarSv;

	SG_GetNodeTransform(sv->cam, &T);
	SG_UnProject(AGWIDGET(sv)->cx+w.x, y, w.z, &T, &P, vp, vOut);
	return (0);
}

static void
ViewOverlay(AG_Event *event)
{
	char text[1024];
	SG_View *sv = AG_PTR(1);
	SDL_Surface *su;
	SG_Vector v;

	v = SG_NodePos(sv->cam);

	snprintf(text, sizeof(text), "%s (%f,%f,%f)",
	    SGNODE(sv->cam)->name, v.x, v.y, v.z);

	su = AG_TextRender(NULL, -1, AG_COLOR(TEXT_COLOR), text);
	AG_WidgetBlit(sv, su, 0, AGWIDGET(sv)->h - su->h);
	SDL_FreeSurface(su);
}

static void
ViewDraw(AG_Event *event)
{
	SG_View *sv = AG_PTR(1);

	glPushAttrib(GL_ENABLE_BIT|GL_POLYGON_BIT);
	glClear(GL_DEPTH_BUFFER_BIT);

	if ((sv->flags & SG_VIEW_NO_DEPTH_TEST) == 0)
		glEnable(GL_DEPTH_TEST);

	/* Set rendering parameters. */
	if (sv->cam->polyFace.cull && sv->cam->polyBack.cull) {
		glEnable(GL_CULL_FACE);
		glCullFace(GL_FRONT_AND_BACK);
	} else if (sv->cam->polyFace.cull) {
		glEnable(GL_CULL_FACE);
		glCullFace(GL_FRONT);
	} else if (sv->cam->polyBack.cull) {
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
	} else {
		glDisable(GL_CULL_FACE);
	}
	switch (sv->cam->polyFace.mode) {
	case SG_CAMERA_POINTS:
		glPolygonMode(GL_FRONT, GL_POINT);
		break;
	case SG_CAMERA_WIREFRAME:
		glPolygonMode(GL_FRONT, GL_LINE);
		break;
	case SG_CAMERA_FLAT_SHADED:
		glPolygonMode(GL_FRONT, GL_FILL);
		glShadeModel(GL_FLAT);
		break;
	case SG_CAMERA_SMOOTH_SHADED:
		glPolygonMode(GL_FRONT, GL_FILL);
		glShadeModel(GL_SMOOTH);
		break;
	}
	switch (sv->cam->polyBack.mode) {
	case SG_CAMERA_POINTS:
		glPolygonMode(GL_BACK, GL_POINT);
		break;
	case SG_CAMERA_WIREFRAME:
		glPolygonMode(GL_BACK, GL_LINE);
		break;
	case SG_CAMERA_FLAT_SHADED:
		glPolygonMode(GL_BACK, GL_FILL);
		glShadeModel(GL_FLAT);
		break;
	case SG_CAMERA_SMOOTH_SHADED:
		glPolygonMode(GL_BACK, GL_FILL);
		glShadeModel(GL_SMOOTH);
		break;
	}

	/* Set the modelview matrix for the current camera. */
	glLoadIdentity();
	SG_CameraSetup(sv->cam);

	/* Enable the light sources. */
	if ((sv->flags & SG_VIEW_NO_LIGHTING) == 0) {
		SG_Light *lt;
	
		glPushAttrib(GL_LIGHTING_BIT);
		glEnable(GL_LIGHTING);
		glEnable(GL_LIGHT0);

		SG_FOREACH_NODE_CLASS(lt, sv->sg, sg_light, "Light:*") {
			if (lt->light != GL_INVALID_ENUM)
				SG_LightSetup(lt, sv);
		}
		glLightModelf(GL_LIGHT_MODEL_TWO_SIDE, 1.0);
	}

	/* Render the scene. */
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	SG_RenderNode(sv->sg, SGNODE(sv->sg->root), sv);
	glPopMatrix();

	if ((sv->flags & SG_VIEW_NO_LIGHTING) == 0) {
		glPopAttrib();
	}
	glPopAttrib();
}

static void
ViewScale(AG_Event *event)
{
	SG_View *sv = AG_PTR(1);

	glMatrixMode(GL_PROJECTION);
	SG_CameraProject(sv->cam);
}

static void
RotateCameraByMouse(SG_View *sv, int x, int y)
{
	SG_Rotatev(sv->cam, sv->mouse.rsens.y*(SG_Real)y, SG_I);
	SG_Rotatev(sv->cam, sv->mouse.rsens.x*(SG_Real)x, SG_J);
}

static void
MoveCameraByMouse(SG_View *sv, int xrel, int yrel, int zrel)
{
	SG_Vector m;
	SG_Matrix *T = &SGNODE(sv->cam)->T;
	
	m.x = sv->mouse.tsens.x*(-(SG_Real)xrel);
	m.y = sv->mouse.tsens.y*((SG_Real)yrel);
	m.z = sv->mouse.tsens.z*(-(SG_Real)zrel);

	/* Translate along the global axis, not the camera axis. */
//	T->m[0][3] += m.x;
//	T->m[1][3] += m.y;
//	T->m[2][3] += m.z;
#if 1
	/* Translate along the local camera axis. */
	SG_MatrixTranslatev(&SGNODE(sv->cam)->T, m);
#endif

	if (sv->cam->pmode == SG_CAMERA_ORTHOGRAPHIC) {
		SG_ViewUpdateProjection(sv);
	}
}

static void
ViewMotion(AG_Event *event)
{
	SG_View *sv = AG_PTR(1);
	int x = AG_INT(2);
	int y = AG_INT(3);
	int xrel = AG_INT(4);
	int yrel = AG_INT(5);
	int state = AG_INT(6);

	if (state & SDL_BUTTON_LEFT) {
		if (SDL_GetModState() & KMOD_CTRL) {
			RotateCameraByMouse(sv, xrel, yrel);
		} else {
			MoveCameraByMouse(sv, xrel, yrel, 0);
		}
	}
}

static void
ViewSwitchCamera(AG_Event *event)
{
	SG_View *sv = AG_PTR(1);
	SG_Camera *cam = AG_PTR(2);

	sv->cam = cam;
}

static void
PopupMenuClose(SG_View *sv)
{
	AG_MenuCollapse(sv->popup.menu, sv->popup.item);
	AG_ObjectDestroy(sv->popup.menu);
	Free(sv->popup.menu, M_OBJECT);

	sv->popup.menu = NULL;
	sv->popup.item = NULL;
	sv->popup.win = NULL;
}

static void
PopupMenuOpen(SG_View *sv, int x, int y)
{
	SG *sg = sv->sg;

	if (sv->popup.menu != NULL)
		PopupMenuClose(sv);

	sv->popup.menu = Malloc(sizeof(AG_Menu), M_OBJECT);
	AG_MenuInit(sv->popup.menu, 0);

	sv->popup.item = AG_MenuAddItem(sv->popup.menu, NULL);
	{
		AG_MenuItem *mOvl, *mCam;

		AG_MenuIntFlags(sv->popup.item, _("Lighting"),
		    RG_CONTROLS_ICON, &sv->flags, SG_VIEW_NO_LIGHTING, 1);
		AG_MenuIntFlags(sv->popup.item, _("Z-Buffer"),
		    RG_CONTROLS_ICON, &sv->flags, SG_VIEW_NO_DEPTH_TEST, 1);

		mOvl = AG_MenuNode(sv->popup.item, _("Overlay"), -1);
		{
			AG_MenuIntFlags(mOvl, _("Wireframe"),
			    GRID_ICON,
			    &sg->flags, SG_OVERLAY_WIREFRAME, 0);
			AG_MenuIntFlags(mOvl, _("Vertices"),
			    VGPOINTS_ICON,
			    &sg->flags, SG_OVERLAY_VERTICES, 0);
			AG_MenuIntFlags(mOvl, _("Facet normals"),
			    UP_ARROW_ICON,
			    &sg->flags, SG_OVERLAY_FNORMALS, 0);
			AG_MenuIntFlags(mOvl, _("Vertex normals"),
			    UP_ARROW_ICON,
			    &sg->flags, SG_OVERLAY_VNORMALS, 0);
		}
		AG_MenuSeparator(sv->popup.item);
		AG_MenuSeparator(sv->popup.item);
		AG_MenuSeparator(sv->popup.item);
		AG_MenuSeparator(sv->popup.item);
		mCam = AG_MenuNode(sv->popup.item, _("Switch to camera"), -1);
		{
			AG_MenuItem *mi;
			SG_Camera *cam;

			SG_FOREACH_NODE_CLASS(cam, sg, sg_camera, "Camera") {
				mi = AG_MenuAction(mCam, SGNODE(cam)->name,
				    OBJ_ICON,
				    ViewSwitchCamera, "%p,%p", sv, cam);
				mi->state = (cam == sv->cam);
			}
		}
	}
	sv->popup.menu->sel_item = sv->popup.item;
	sv->popup.win = AG_MenuExpand(sv->popup.menu, sv->popup.item,
	    AGWIDGET(sv)->cx+x, AGWIDGET(sv)->cy+y);
}

static void
SelectByMouse(SG_View *sv, int x, int y)
{
	int viewport[4];
	SG_Vector vOut;

	if (SG_ViewUnProject(sv, SG_VECTOR((SG_Real)x, (SG_Real)y, 0.0),
	    20.0, sv->cam->zFar, &vOut) == -1) {
		AG_TextMsg(AG_MSG_ERROR, "%s", AG_GetError());
	}
#if 0
	{
		SG_Node *node;

		dprintf("at %f,%f,%f\n", vOut.x, vOut.y, vOut.z);
		node = SG_NodeAdd(sv->sg->root, &sgSphereOps, 0);
		vOut.z -= 5.0;
		SG_NodeTranslateVec(node, vOut);
	}
#endif
}

static void
ViewKeydown(AG_Event *event)
{
	SG_View *sv = AG_PTR(1);
	int keysym = AG_INT(2);
	int kmod = AG_INT(3);
	int unicode = AG_INT(4);

	switch (keysym) {
	case SDLK_LEFT:
		if (kmod & KMOD_CTRL) {
			sv->rot_yaw = -sv->rot_yaw_incr;
			AG_ReplaceTimeout(sv, &sv->to_rot_yaw, sv->rot_vel_min);
		} else {
			sv->trans_x = -sv->trans_x_incr;
			AG_ReplaceTimeout(sv, &sv->to_trans_x,
			                  sv->trans_vel_min);
		}
		break;
	case SDLK_RIGHT:
		if (kmod & KMOD_CTRL) {
			sv->rot_yaw = +sv->rot_yaw_incr;
			AG_ReplaceTimeout(sv, &sv->to_rot_yaw, sv->rot_vel_min);
		} else {
			sv->trans_x = +sv->trans_x_incr;
			AG_ReplaceTimeout(sv, &sv->to_trans_x,
			                  sv->trans_vel_min);
		}
		break;
	case SDLK_UP:
		if (kmod & KMOD_CTRL) {
			sv->rot_pitch = -sv->rot_pitch_incr;
			AG_ReplaceTimeout(sv, &sv->to_rot_pitch,
			                  sv->rot_vel_min);
		} else {
			sv->trans_y = +sv->trans_y_incr;
			AG_ReplaceTimeout(sv, &sv->to_trans_y,
			                  sv->trans_vel_min);
		}
		break;
	case SDLK_DOWN:
		if (kmod & KMOD_CTRL) {
			sv->rot_pitch = +sv->rot_pitch_incr;
			AG_ReplaceTimeout(sv, &sv->to_rot_pitch,
			                  sv->rot_vel_min);
		} else {
			sv->trans_y = -sv->trans_y_incr;
			AG_ReplaceTimeout(sv, &sv->to_trans_y,
			                  sv->trans_vel_min);
		}
		break;
	case SDLK_PAGEUP:
		sv->trans_z = -sv->trans_z_incr;
		AG_ReplaceTimeout(sv, &sv->to_trans_z, sv->trans_vel_min);
		break;
	case SDLK_PAGEDOWN:
		sv->trans_z = +sv->trans_z_incr;
		AG_ReplaceTimeout(sv, &sv->to_trans_z, sv->trans_vel_min);
		break;
	case SDLK_DELETE:
		sv->rot_roll = -sv->rot_roll_incr;
		AG_ReplaceTimeout(sv, &sv->to_rot_roll, sv->rot_vel_min);
		break;
	case SDLK_END:
		sv->rot_roll = +sv->rot_roll_incr;
		AG_ReplaceTimeout(sv, &sv->to_rot_roll, sv->rot_vel_min);
		break;
	}
}

static void
ViewKeyup(AG_Event *event)
{
	SG_View *sv = AG_PTR(1);
	int keysym = AG_INT(2);
	int keymode = AG_INT(3);
	int unicode = AG_INT(4);

	switch (keysym) {
	case SDLK_LEFT:
	case SDLK_RIGHT:
		AG_DelTimeout(sv, &sv->to_rot_yaw);
		AG_DelTimeout(sv, &sv->to_trans_x);
		break;
	case SDLK_UP:
	case SDLK_DOWN:
		AG_DelTimeout(sv, &sv->to_rot_pitch);
		AG_DelTimeout(sv, &sv->to_trans_y);
		break;
	case SDLK_PAGEUP:
	case SDLK_PAGEDOWN:
		AG_DelTimeout(sv, &sv->to_trans_z);
		break;
	case SDLK_DELETE:
	case SDLK_END:
		AG_DelTimeout(sv, &sv->to_rot_roll);
		break;
	}
}

static void
ViewButtondown(AG_Event *event)
{
	SG_View *sv = AG_PTR(1);
	int button = AG_INT(2);
	int x = AG_INT(3);
	int y = AG_INT(4);

	AG_WidgetFocus(sv);

	switch (button) {
	case SDL_BUTTON_WHEELUP:
		MoveCameraByMouse(sv, 0, 0, 1);
		break;
	case SDL_BUTTON_WHEELDOWN:
		MoveCameraByMouse(sv, 0, 0, -1);
		break;
	case SDL_BUTTON_RIGHT:
		PopupMenuOpen(sv, x, y);
		break;
	case SDL_BUTTON_LEFT:
		SelectByMouse(sv, x, y);
		break;
	}
}

void
SG_ViewSetCamera(SG_View *sv, SG_Camera *cam)
{
	sv->cam = cam;
	SG_ViewUpdateProjection(sv);
}

static void
SG_ViewAttached(AG_Event *event)
{
	SG_Camera *cam;
	SG_View *sv = AG_SELF();

	/* Attach to the default camera. */
	if ((cam = SG_FindNode(sv->sg, "Camera0")) == NULL) {
		fatal("no Camera0");
	}
	SG_ViewSetCamera(sv, cam);
}

static Uint32
RotateYawTimeout(void *obj, Uint32 ival, void *arg)
{
	SG_View *sv = obj;

	SG_Rotatev(sv->cam, sv->rot_yaw, SG_J);
	return (ival > sv->rot_vel_max ? ival-sv->rot_vel_accel : ival);
}

static Uint32
RotatePitchTimeout(void *obj, Uint32 ival, void *arg)
{
	SG_View *sv = obj;

	SG_Rotatev(sv->cam, sv->rot_pitch, SG_I);
	return (ival > sv->rot_vel_max ? ival-sv->rot_vel_accel : ival);
}

static Uint32
RotateRollTimeout(void *obj, Uint32 ival, void *arg)
{
	SG_View *sv = obj;

	SG_Rotatev(sv->cam, sv->rot_roll, SG_K);
	return (ival > sv->rot_vel_max ? ival-sv->rot_vel_accel : ival);
}

static Uint32
TranslateXTimeout(void *obj, Uint32 ival, void *arg)
{
	SG_View *sv = obj;
	SG_TranslateX(sv->cam, sv->trans_x);
	return (ival > sv->trans_vel_max ? ival-sv->trans_vel_accel : ival);
}

static Uint32
TranslateYTimeout(void *obj, Uint32 ival, void *arg)
{
	SG_View *sv = obj;
	SG_TranslateY(sv->cam, sv->trans_y);
	return (ival > sv->trans_vel_max ? ival-sv->trans_vel_accel : ival);
}

static Uint32
TranslateZTimeout(void *obj, Uint32 ival, void *arg)
{
	SG_View *sv = obj;
	SG_TranslateZ(sv->cam, sv->trans_z);
	return (ival > sv->trans_vel_max ? ival-sv->trans_vel_accel : ival);
}

void
SG_ViewInit(SG_View *sv, SG *sg, Uint flags)
{
	Uint glvflags = AG_GLVIEW_FOCUS;

	if (flags & SG_VIEW_HFILL) { glvflags |= AG_GLVIEW_HFILL; }
	if (flags & SG_VIEW_VFILL) { glvflags |= AG_GLVIEW_VFILL; }

	AG_GLViewInit(AGGLVIEW(sv), glvflags);
	AG_GLViewDrawFn(AGGLVIEW(sv), ViewDraw, "%p", sv);
	AG_GLViewOverlayFn(AGGLVIEW(sv), ViewOverlay, "%p", sv);
	AG_GLViewScaleFn(AGGLVIEW(sv), ViewScale, "%p", sv);
	AG_GLViewMotionFn(AGGLVIEW(sv), ViewMotion, "%p", sv);
	AG_GLViewButtondownFn(AGGLVIEW(sv), ViewButtondown, "%p", sv);
	AG_GLViewKeydownFn(AGGLVIEW(sv), ViewKeydown, "%p", sv);
	AG_GLViewKeyupFn(AGGLVIEW(sv), ViewKeyup, "%p", sv);
	AG_ObjectSetOps(sv, &sgViewOps);

	sv->flags = flags;
	sv->sg = sg;
	sv->cam = NULL;
	sv->popup.menu = NULL;
	sv->popup.item = NULL;
	sv->popup.win = NULL;
	sv->editPane = NULL;

	AG_SetEvent(sv, "attached", SG_ViewAttached, NULL);

	sv->mouse.rsens = SG_VECTOR(0.002, 0.002, 0.002);
	sv->mouse.tsens = SG_VECTOR(0.01, 0.01, 0.5);

	sv->rot_vel_min = 30;
	sv->rot_vel_accel = 1;
	sv->rot_vel_max = 10;
	sv->trans_vel_min = 20;
	sv->trans_vel_accel = 1;
	sv->trans_vel_max = 5;

	sv->rot_yaw_incr = 0.05;
	sv->rot_pitch_incr = 0.05;
	sv->rot_roll_incr = 0.05;
	sv->trans_x_incr = 0.1;
	sv->trans_y_incr = 0.1;
	sv->trans_z_incr = 0.1;

	AG_SetTimeout(&sv->to_rot_yaw, RotateYawTimeout, NULL, 0);
	AG_SetTimeout(&sv->to_rot_pitch, RotatePitchTimeout, NULL, 0);
	AG_SetTimeout(&sv->to_rot_roll, RotateRollTimeout, NULL, 0);
	AG_SetTimeout(&sv->to_trans_x, TranslateXTimeout, NULL, 0);
	AG_SetTimeout(&sv->to_trans_y, TranslateYTimeout, NULL, 0);
	AG_SetTimeout(&sv->to_trans_z, TranslateZTimeout, NULL, 0);
}

void
SG_ViewDestroy(void *p)
{
	AG_GLViewDestroy(p);
}

void
SG_ViewKeydownFn(SG_View *sv, AG_EventFn fn, const char *fmt, ...)
{
}

void
SG_ViewKeyupFn(SG_View *sv, AG_EventFn fn, const char *fmt, ...)
{
}

void
SG_ViewButtondownFn(SG_View *sv, AG_EventFn fn, const char *fmt, ...)
{
}

void
SG_ViewButtonupFn(SG_View *sv, AG_EventFn fn, const char *fmt, ...)
{
}

void
SG_ViewMotionFn(SG_View *sv, AG_EventFn fn, const char *fmt, ...)
{
}