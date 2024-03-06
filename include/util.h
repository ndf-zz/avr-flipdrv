// SPDX-License-Identifier: MIT

#ifndef UTIL_H
#define UTIL_H

/* optimisation barrier - for ordering co-dependent register access  */
#define barrier() __asm__ __volatile__("": : :"memory")

#endif /* UTIL_H */
