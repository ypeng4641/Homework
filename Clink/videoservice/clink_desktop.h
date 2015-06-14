#pragma once

#include "Plugin.h"
using namespace device;

void* clink_desktop_open(const VS_SIGNAL* signal, SigCB cb, void* context, const Uuid& item);
int   clink_desktop_close(void* handle);
int   clink_desktop_set_i_frame(void* handle);
int   clink_desktop_error(void* handle);