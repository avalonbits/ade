/*
 * Copyright (C) 2026  Igor Cananea <icc@avalonbits.com>
 * Author: Igor Cananea <icc@avalonbits.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _PROJECT_H_
#define _PROJECT_H_

#include <stdbool.h>

#include <hub/hub.h>

/*
 * A project: project.ini in the current directory, naming a program and the
 * files it is built from. With one, F9 builds the project rather than the
 * file on screen.
 *
 *   [project]
 *   name = hello                   ; the program: hello.bin
 *   files = main.c util.c fast.s   ; each built, in order, then linked
 *
 *   [build]                        ; optional -- these are the defaults
 *   c = acc -c %f -o %b.o -errors %e
 *   s = zap %f %b.o -f acc -c -e %e
 *   asm = zap %f %b.o -f acc -c -e %e
 *   link = acc %o -o %n.bin -errors %e
 *
 * In a command, %f is the file, %b the file without its extension, %o every
 * file's %b.o in order, %n the program's name, %e the file a tool writes its
 * error to, and %% a percent sign. An empty link runs no link: a program
 * assembled in one piece, `s = zap %f %n.bin -c -e %e`, needs none.
 *
 * Each command is one of hub's jobs, and hub holds HUB_MAX_JOBS, counting the
 * continuation that brings ade back. Run adds the program: five files, the
 * link, the program and ade make eight.
 */

#define PROJ_FILE       "project.ini"
#define PROJ_FILES      5
#define PROJ_NAME_MAX   64

typedef struct {
    char name[PROJ_NAME_MAX];
    int nfiles;
    char files[PROJ_FILES][PROJ_NAME_MAX];
    char c[HUB_CMD_MAX + 1];
    char s[HUB_CMD_MAX + 1];
    char asm_[HUB_CMD_MAX + 1];
    char link[HUB_CMD_MAX + 1];
} project;

// Reads PROJ_FILE: 1 when there is a project, 0 when there is no file, and -1
// when the file is there but wrong, with what is wrong in `msg`.
int proj_load(project* p, char* msg, int max);

// How many commands building the project takes: one per file, and the link.
int proj_jobs(const project* p);

// Command `i` of proj_jobs, with its error going to `err`. False when it is
// longer than `max` allows.
bool proj_job(const project* p, int i, const char* err, char* out, int max);

#endif  // _PROJECT_H_
