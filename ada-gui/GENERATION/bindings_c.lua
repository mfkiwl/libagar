#!/usr/bin/env lua

local ts = tostring

io.write ([[
void
agar_gui_widget_bind_pointer (AG_Widget *w, const char *binding, void **p)
{
  AG_WidgetBindPointer (w, binding, p);
}

void
agar_gui_widget_bind_property (AG_Widget *w, const char *binding,
  AG_Object *obj, const char *prop_name)
{
  AG_WidgetBindProp (w, binding, obj, prop_name);
}

void
agar_gui_widget_bind_boolean (AG_Widget *w, const char *binding, int *var)
{
  AG_WidgetBindBool (w, binding, var);
}

void
agar_gui_widget_bind_integer (AG_Widget *w, const char *binding, int *var)
{
  AG_WidgetBindInt (w, binding, var);
}

void
agar_gui_widget_bind_unsigned (AG_Widget *w, const char *binding, unsigned *var)
{
  AG_WidgetBindUint (w, binding, var);
}

void
agar_gui_widget_bind_float (AG_Widget *w, const char *binding, float *var)
{
  AG_WidgetBindFloat (w, binding, var);
}

void
agar_gui_widget_bind_double (AG_Widget *w, const char *binding, double *var)
{
  AG_WidgetBindDouble (w, binding, var);
}

]])

for _, value in pairs ({8, 16, 32, 64}) do
  io.write ([[
void
agar_gui_widget_bind_uint]]..ts(value)..[[ (AG_Widget *w, const char *binding,
  Uint]]..ts(value)..[[ *val)
{
  AG_WidgetBindUint]]..ts(value)..[[ (w, binding, val);
}

void
agar_gui_widget_bind_int]]..ts(value)..[[ (AG_Widget *w, const char *binding,
  Sint]]..ts(value)..[[ *val)
{
  AG_WidgetBindSint]]..ts(value)..[[ (w, binding, val);
}

void
agar_gui_widget_bind_flag]]..ts(value)..[[ (AG_Widget *w, const char *binding,
  Uint]]..ts(value)..[[ *val, Uint]]..ts(value)..[[ bitmask)
{
  AG_WidgetBindFlag]]..ts(value)..[[ (w, binding, val, bitmask);
}

]])
end

io.write ([[
void *
agar_gui_widget_get_pointer (AG_Widget *w, const char *binding)
{
  return AG_WidgetPointer (w, binding);
}

int
agar_gui_widget_get_boolean (AG_Widget *w, const char *binding)
{
  return AG_WidgetBool (w, binding);
}

int
agar_gui_widget_get_integer (AG_Widget *w, const char *binding)
{
  return AG_WidgetInt (w, binding);
}

unsigned int
agar_gui_widget_get_unsigned (AG_Widget *w, const char *binding)
{
  return AG_WidgetUint (w, binding);
}

float
agar_gui_widget_get_float (AG_Widget *w, const char *binding)
{
  return AG_WidgetFloat (w, binding);
}

double
agar_gui_widget_get_double (AG_Widget *w, const char *binding)
{
  return AG_WidgetDouble (w, binding);
}

]])

for _, value in pairs ({8, 16, 32, 64}) do
  io.write ([[
Uint]]..ts(value)..[[

agar_gui_widget_get_uint]]..ts(value)..[[ (AG_Widget *w, const char *binding)
{
  return AG_WidgetUint]]..ts(value)..[[ (w, binding);
}

Sint]]..ts(value)..[[

agar_gui_widget_get_int]]..ts(value)..[[ (AG_Widget *w, const char *binding)
{
  return AG_WidgetSint]]..ts(value)..[[ (w, binding);
}

]])
end

io.write ([[
void
agar_gui_widget_set_pointer (AG_Widget *w, const char *binding, void *val)
{
  AG_WidgetSetPointer (w, binding, val);
}

void
agar_gui_widget_set_boolean (AG_Widget *w, const char *binding, int val)
{
  AG_WidgetSetBool (w, binding, val);
}

void
agar_gui_widget_set_integer (AG_Widget *w, const char *binding, int val)
{
  AG_WidgetSetInt (w, binding, val);
}

void
agar_gui_widget_set_unsigned (AG_Widget *w, const char *binding, unsigned int val)
{
  AG_WidgetSetUint (w, binding, val);
}

void
agar_gui_widget_set_float (AG_Widget *w, const char *binding, float val)
{
  AG_WidgetSetFloat (w, binding, val);
}

void
agar_gui_widget_set_double (AG_Widget *w, const char *binding, double val)
{
  AG_WidgetSetDouble (w, binding, val);
}

]])

for _, value in pairs ({8, 16, 32, 64}) do
  io.write ([[
void
agar_gui_widget_set_uint]]..ts(value)..[[ (AG_Widget *w, const char *binding,
  Uint]]..ts(value)..[[ val)
{
  AG_WidgetSetUint]]..ts(value)..[[ (w, binding, val);
}

void
agar_gui_widget_set_int]]..ts(value)..[[ (AG_Widget *w, const char *binding,
  Sint]]..ts(value)..[[ val)
{
  AG_WidgetSetSint]]..ts(value)..[[ (w, binding, val);
}

]])
end
