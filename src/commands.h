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

#ifndef COMMANDS_H
#define COMMANDS_H

struct imv_list;

struct imv_commands {
  struct imv_list *command_list;
};

struct imv_commands *imv_commands_create();
void imv_commands_free(struct imv_commands *cmds);
void imv_command_register(struct imv_commands *cmds, const char *command, void (*handler)());
void imv_command_alias(struct imv_commands *cmds, const char *command, const char *alias);
int imv_command_exec(struct imv_commands *cmds, const char *command, void *data);

#endif

/* vim:set ts=2 sts=2 sw=2 et: */
