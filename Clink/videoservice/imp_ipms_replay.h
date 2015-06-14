#pragma once

#include "Plugin.h"
using namespace device;

void* imp_ipms_replay_open(const VS_SIGNAL* signal, SigCB cb, void* context, const Uuid& item);
int   imp_ipms_replay_close(void* handle);
int   imp_ipms_replay_set_i_frame(void* handle);
int   imp_ipms_replay_error(void* handle);
