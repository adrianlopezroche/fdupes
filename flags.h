#ifndef FLAGS_H
#define FLAGS_H

#define ISFLAG(a,b) ((a & b) == b)
#define SETFLAG(a,b) (a |= b)

#define F_RECURSE           0x0001
#define F_HIDEPROGRESS      0x0002
#define F_DSAMELINE         0x0004
#define F_FOLLOWLINKS       0x0008
#define F_DELETEFILES       0x0010
#define F_EXCLUDEEMPTY      0x0020
#define F_CONSIDERHARDLINKS 0x0040
#define F_SHOWSIZE          0x0080
#define F_OMITFIRST         0x0100
#define F_RECURSEAFTER      0x0200
#define F_NOPROMPT          0x0400
#define F_SUMMARIZEMATCHES  0x0800
#define F_EXCLUDEHIDDEN     0x1000
#define F_PERMISSIONS       0x2000
#define F_REVERSE           0x4000
#define F_IMMEDIATE         0x8000
#define F_PLAINPROMPT       0x10000
#define F_SHOWTIME          0x20000
#define F_DEFERCONFIRMATION 0x40000
#define F_CACHESIGNATURES   0x80000
#define F_CLEARCACHE        0x100000
#define F_PRUNECACHE        0x200000
#define F_READONLYCACHE     0x400000
#define F_VACUUMCACHE       0x800000

extern unsigned long flags;

#endif
