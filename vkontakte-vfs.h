/*
 * vkontakte-vfs.h
 *
 *  Created on: Nov 2, 2013
 *      Author: scorp
 */

#ifndef VKONTAKTE_VFS_H_
#define VKONTAKTE_VFS_H_

#include <deadbeef/deadbeef.h>

const char *    vk_ddb_vfs_format_track_url(int aid,
                                            int owner_id,
                                            const char *artist,
                                            const char *title);

const char **   vk_ddb_vfs_get_schemes ();
DB_FILE *       vk_ddb_vfs_open (const char *fname);
int             vk_ddb_vfs_is_streaming (void);


#endif /* VKONTAKTE_VFS_H_ */
