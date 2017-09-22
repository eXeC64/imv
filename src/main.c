/* Copyright (c) 2017 imv authors

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "imv.h"

int main(int argc, char** argv)
{
  struct imv *imv = imv_create();

  if(!imv) {
    return 1;
  }

  if(!imv_load_config(imv)) {
    imv_free(imv);
    return 1;
  }

  if(!imv_parse_args(imv, argc, argv)) {
    imv_free(imv);
    return 1;
  }

  int ret = imv_run(imv);

  imv_free(imv);

  return ret;
}

/* vim:set ts=2 sts=2 sw=2 et: */
