#pragma once

#ifndef __ATED__
#define __ATED__

#define ALLOC_STEP 1024

// find min and max value
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

// to find the center of offseted length
#define CENTER(length, offset) ((((length) - (offset)) / 2))

// to make ctrl + ; key maps more readable
#define CTRL(x) ((x) & 0x1F)

#endif
