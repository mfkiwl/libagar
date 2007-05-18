/*	Public domain	*/
/*
 * This program uses Agar-SG to plot the Lorenz attractor.
 */

#include <agar/core.h>
#include <agar/gui.h>
#include <agar/sg.h>
#include <agar/sg/sg_view.h>
#include <agar/sg/sg_gui.h>

#include <string.h>
#include <unistd.h>
#include <math.h>
#include <stdlib.h>

SG_Real r = 28.0;
SG_Real sigma = 10.0;
SG_Real b = 8.0/3.0;
SG_Real h = 0.01;
SG_Real spinX = 0.0, spinY = 0.0005, spinZ = 0.0;
SG_Node *node;
int iter = 1, iteriter = 10;

static void
LorenzPlot(void *pNode, SG_View *sgv)
{
	SG_Node *node = pNode;
	SG_Vector p = SG_VECTOR(0.0, 1.0, -1.0);
	SG_Color color;
	int i;
	
	SG_Begin(SG_LINE_STRIP);
	for (i = 0; i < iter; i++) {
		color.b = color.g = 0.2 + 0.8*(SG_Real)i/(SG_Real)iter;
		color.r = 1.0;
		SG_Color3v(&color);
		p.x += h*(sigma*(p.y - p.x));
		p.y += h*(p.x*(r - p.z) - p.y);
		p.z += h*(p.x*p.y - b*p.z);
		SG_Vertex3v(&p);
	}
	SG_End();
	
	SG_RotateEul(node, spinX, spinY, spinZ);
	iter += iteriter;
}

SG_NodeOps LorenzOps = {
	"Lorenz attractor",
	sizeof(SG_Node),
	0,
	NULL,		/* init */
	NULL,		/* destroy */
	NULL,		/* load */
	NULL,		/* save */
	NULL,		/* edit */
	NULL,	
	NULL,		
	LorenzPlot
};

AG_Window *
SettingsWin(void)
{
	AG_Window *win;
	AG_FSpinbutton *fsb;
	AG_Spinbutton *sb;
	
	win = AG_WindowNew(0);
	AG_WindowSetCaption(win, "Parameters");
	AG_WindowSetPosition(win, AG_WINDOW_MIDDLE_LEFT, 0);
	
	sb = AG_SpinbuttonNew(win, 0, "Iterations:");
	AG_SpinbuttonSetIncrement(sb, 1);
	AG_WidgetBindInt(sb, "value", &iter);
	
	sb = AG_SpinbuttonNew(win, 0, "Iterations':");
	AG_SpinbuttonSetIncrement(sb, 1);
	AG_WidgetBindInt(sb, "value", &iteriter);

	fsb = AG_FSpinbuttonNew(win, 0, NULL, "r:");
	AG_FSpinbuttonSetIncrement(fsb, 0.001);
	SG_WidgetBindReal(fsb, "value", &r);
	
	fsb = AG_FSpinbuttonNew(win, 0, NULL, "sigma:");
	AG_FSpinbuttonSetIncrement(fsb, 0.001);
	SG_WidgetBindReal(fsb, "value", &sigma);

	fsb = AG_FSpinbuttonNew(win, 0, NULL, "h:");
	AG_FSpinbuttonSetIncrement(fsb, 0.001);
	SG_WidgetBindReal(fsb, "value", &h);

	fsb = AG_FSpinbuttonNew(win, 0, NULL, "b:");
	AG_FSpinbuttonSetIncrement(fsb, 0.01);
	SG_WidgetBindReal(fsb, "value", &b);

	fsb = AG_FSpinbuttonNew(win, 0, NULL, "Pitch:");
	AG_FSpinbuttonSetIncrement(fsb, 0.0001);
	SG_WidgetBindReal(fsb, "value", &spinX);

	fsb = AG_FSpinbuttonNew(win, 0, NULL, "Yaw:");
	AG_FSpinbuttonSetIncrement(fsb, 0.0001);
	SG_WidgetBindReal(fsb, "value", &spinY);
	
	fsb = AG_FSpinbuttonNew(win, 0, NULL, "Roll:");
	AG_FSpinbuttonSetIncrement(fsb, 0.0001);
	SG_WidgetBindReal(fsb, "value", &spinZ);
	
	AG_WindowShow(win);
}

int
main(int argc, char *argv[])
{
	int c, i, fps = -1;
	AG_Window *win;
	SG_View *sv;
	SG_Node *node;
	SG_Camera *cam0;
	SG *sg;
	char *s;

	if (AG_InitCore("lorenz-demo", 0) == -1) {
		fprintf(stderr, "%s\n", AG_GetError());
		return (1);
	}
	if (AG_InitVideo(1024, 700, 32, AG_VIDEO_OPENGL) == -1 ||
	    AG_InitInput(0) == -1) {
		fprintf(stderr, "%s\n", AG_GetError());
		return (-1);
	}
	AG_InitConfigWin(0);
	AG_SetRefreshRate(fps);
	AG_BindGlobalKey(SDLK_ESCAPE, KMOD_NONE, AG_Quit);
	AG_BindGlobalKey(SDLK_F8, KMOD_NONE, AG_ViewCapture);
	agColors[WINDOW_BG_COLOR] = SDL_MapRGB(agVideoFmt, 0,0,0);
	SettingsWin();

	sg = SG_New(agWorld, "scene");
	node = SG_NodeAdd(sg->root, "lorenz", &LorenzOps, 0);
	if ((cam0 = SG_FindNode(sg, "Camera0")) != NULL) {
		SG_Translate3(cam0, 0.0, 0.0, -100.0);
	}

	win = AG_WindowNew(0);
	AG_WindowSetCaption(win, "Lorenz Attractor");
	sv = SG_ViewNew(win, sg, SG_VIEW_EXPAND|SG_VIEW_NO_LIGHTING);
	AG_WindowSetGeometry(win, 64, 0, agView->w-64, agView->h);
	AG_WindowShow(win);

	AG_EventLoop();
	AG_Destroy();
	return (0);
fail:
	AG_Destroy();
	return (1);
}
