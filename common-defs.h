/*
 * common-defs.h
 *
 *  Created on: Dec 9, 2012
 *      Author: scorp
 */

#ifndef COMMON_DEFS_H_
#define COMMON_DEFS_H_

#include <glib/gprintf.h>

G_BEGIN_DECLS

#define trace(...) { g_fprintf(stderr, __VA_ARGS__); }

G_END_DECLS

#endif /* COMMON_DEFS_H_ */
