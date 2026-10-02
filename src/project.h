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

/*
 * A project: project.ini in the current directory, naming a program and the
 * files it is built from. With one, F9 builds the project rather than the
 * file on screen.
 *
 *   [project]
 *   name = hello                   ; the program: hello.bin
 *   files = main.c util.c fast.s   ; each built, in order, then linked --
 *                                  ; or, with no files line, every .c, .s
 *                                  ; and .asm in the directory, by name
 *   exclude = scratch.c            ; with no files line: all but these
 *   run = bin/%n -v data.txt       ; how Run runs it: bin/%n, the program,
 *                                  ; alone unless this says otherwise
 *
 *   [build]                        ; optional -- these are the defaults
 *   c = acc -c %f -o build/%b.o -errors %e
 *   s = zap %f build/%b.o -f acc -c -e %e
 *   asm = zap %f build/%b.o -f acc -c -e %e
 *   link = acc %o -o bin/%n.bin -errors %e
 *
 * Objects go in build/ and the program in bin/, both made as the build
 * starts, so only the source is beside project.ini. In a command, %f is the
 * file, %b the file without its extension, %o every file's build/%b.o in
 * order, %n the program's name, %e the file a tool writes its
 * error to, and %% a percent sign. An empty link runs no link: a program
 * assembled in one piece, `s = zap %f %n.bin -c -e %e`, needs none.
 *
 * The commands go into PROJ_OBEY, beside project.ini, which hub runs as one
 * job with MOS's Obey: it stops at the first command that fails, and so does
 * the build, however many files there are. The file is written again only
 * when project.ini changes -- its first line is a comment holding a checksum
 * of project.ini and of ade's own defaults -- so a hand edit to it lasts
 * until then. It can also be run at MOS's prompt: `obey build.obey`.
 */

#define PROJ_FILE       "project.ini"
#define PROJ_OBJ_DIR    "build"
#define PROJ_BIN_DIR    "bin"
#define PROJ_OBEY       "build.obey"
#define PROJ_FILES      32
#define PROJ_EXCLUDE    8
#define PROJ_NAME_MAX   64

// The longest command: an Obey line is read into 256 bytes, with its end.
#define PROJ_CMD_MAX    250

typedef struct {
    char name[PROJ_NAME_MAX];
    bool listed;                        // files named in a files line
    int nfiles;
    char files[PROJ_FILES][PROJ_NAME_MAX];
    int nexclude;
    char exclude[PROJ_EXCLUDE][PROJ_NAME_MAX];
    char c[PROJ_CMD_MAX + 1];
    char s[PROJ_CMD_MAX + 1];
    char asm_[PROJ_CMD_MAX + 1];
    char link[PROJ_CMD_MAX + 1];
    char run[PROJ_CMD_MAX + 1];
    unsigned long sum;                  // of project.ini and the defaults
} project;

// Reads PROJ_FILE: 1 when there is a project, 0 when there is no file, and -1
// when the file is there but wrong, with what is wrong in `msg`.
int proj_load(project* p, char* msg, int max);

// How many commands building the project takes: one per file, and the link.
int proj_jobs(const project* p);

// Command `i` of proj_jobs, with its error going to `err`. False when it is
// longer than `max` allows.
bool proj_job(const project* p, int i, const char* err, char* out, int max);

// How Run runs the program: `run` with its % names put in. False when it is
// longer than `max` allows.
bool proj_run(const project* p, char* out, int max);

// Writes a fresh PROJ_FILE naming the program `name`, with no files line --
// so every source file in the directory is the project -- and what else it
// may say in comments. False with why in `msg` when there is one already or
// it cannot be written.
bool proj_create(const char* name, char* msg, int max);

// Makes PROJ_OBJ_DIR and PROJ_BIN_DIR, where they are not there already.
void proj_dirs(void);

// Makes sure PROJ_OBEY holds the project's commands, writing it when it was
// made from another project.ini or not at all. False with what went wrong in
// `msg` when it cannot be written.
bool proj_obey(const project* p, const char* err, char* msg, int max);

#endif  // _PROJECT_H_
