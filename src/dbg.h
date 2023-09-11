//
// Created by toby on 2023/9/11.
//

#ifndef COMPILER_DBG_H
#define COMPILER_DBG_H

#ifdef _DEBUG_
#include </usr/local/include/dbg.h>
#else
#define dbg(...) (void(0))
#endif

#endif //COMPILER_DBG_H
