/*
    port.c

    Copyright (C) 1997 Michael J. Fromberger, All Rights Reserved.

    Port to name mapping routines for Xcaliber Mark II
 */

#include "port.h"
#ifdef DEBUG
#include <stdio.h>
#endif

int portlen = 54;
char *portmap[] = {
    "ThePits",  "Asylum",   "Mordor",   "Heaven",   "Vortex",   "Nowhere",
    "Fantasia", "Paradise", "Eden",     "Avalon",   "Scotland", "Reality",
    "Mirkwood", "Bangkok",  "Void",     "LaBrea",   "Madwand",  "London",
    "Lorien",   "Hades",    "Arabia",   "Orthanc",  "Zodiac",   "StarsEnd",
    "Scheme",   "Syntax",   "Istanbul", "BadLand",  "Fantasy",  "Mercury",
    "Sidhe",    "Fairy",    "Luna",     "Korea",    "Eriador",  "Tartarus",
    "Moscow",   "Nunnery",  "Krystle",  "Paradise", "Deep End", "Vergil",
    "Inferno",  "Eriador",  "Castle",   "Ithilien", "Boston",   "Tower",
    "Sylvanus", "Draconi",  "Polaris",  "World",    "Aurora",   "Jungle"};

char *map_port(unsigned int addr, unsigned short p) {
  unsigned int off;

  /* About 1/4 of each 8-bit subnet will have one port name */
  off = (addr >> 6) % portlen;

#ifdef DEBUG
  fprintf(stderr, "map_port: p = %d, off = %d, %s\n", p, off, portmap[off]);
#endif
  return portmap[off];
}
