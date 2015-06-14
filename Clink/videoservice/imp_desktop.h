#pragma once

#include "Plugin.h"
using namespace device;

void* imp_clink_play_open(const VS_SIGNAL* signal, SigCB cb, void* context, const Uuid& item);
int   imp_clink_play_close(void* handle);
int   imp_clink_play_set_i_frame(void* handle);
int   imp_clink_play_error(void* handle);
