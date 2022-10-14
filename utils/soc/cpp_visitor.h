#pragma once

#include "visitor_interface.h"

class QIODevice;

VisitorInterface *createCppVisitor();
void produceCppOutput(VisitorInterface *iface,QIODevice *tgt);
