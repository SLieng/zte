#pragma once

#include <string>
#include <map>

#include "/usr/include/gtk-3.0/gdk/gdkkeysyms.h"
using std::map;

#define extM(A) extern const map<int, const char *> A##ƌ; extern const map<int, const char *> A##Shiftƌ;
#define mX(A) const map<int, const char *> A##ƌ =
#define mY(A) const map<int, const char *> A##Shiftƌ =
