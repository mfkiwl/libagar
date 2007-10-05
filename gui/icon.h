/*	Public domain	*/

#ifndef _AGAR_WIDGET_ICON_H_
#define _AGAR_WIDGET_ICON_H_

#ifdef _AGAR_INTERNAL
#include <gui/widget.h>
#include <gui/text.h>
#else
#include <agar/gui/widget.h>
#include <agar/gui/text.h>
#endif

#include "begin_code.h"

typedef struct ag_icon {
	struct ag_widget wid;
	Uint flags;
	int surface;			/* Icon surface */
} AG_Icon;

__BEGIN_DECLS
extern const AG_WidgetOps agIconOps;

AG_Icon *AG_IconNew(void *, Uint);
void     AG_IconInit(AG_Icon *, Uint);

void    AG_IconSetPadding(AG_Icon *, int, int, int, int);
#define	AG_IconSetPaddingLeft(b,v)   AG_IconSetPadding((b),(v),-1,-1,-1)
#define	AG_IconSetPaddingRight(b,v)  AG_IconSetPadding((b),-1,(v),-1,-1)
#define AG_IconSetPaddingTop(b,v)    AG_IconSetPadding((b),-1,-1,(v),-1)
#define	AG_IconSetPaddingBottom(b,v) AG_IconSetPadding((b),-1,-1,-1,(v))
void	AG_IconSetSurface(AG_Icon *, SDL_Surface *);
__END_DECLS

#include "close_code.h"
#endif /* _AGAR_WIDGET_ICON_H_ */
