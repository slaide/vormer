#pragma once

#include<stdio.h>

#define CHECK(COND,MSG,...) if(!(COND)){printf("%s:%d: ",__FILE__,__LINE__);printf(MSG __VA_OPT__(,) __VA_ARGS__);exit(EXIT_FAILURE);}
#define discard (void)
