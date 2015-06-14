#ifndef CONFIG_INC
#define CONIFG_INC

#include <string>
#include <x_util/uuid.h>

#include <x_types/intxx.h>

#include <x_vs/xvs.h>

using namespace x_util;

extern unsigned short SERVICE_PORT;

int initConfig(int argc, char** argv);
std::string getWork();



#endif