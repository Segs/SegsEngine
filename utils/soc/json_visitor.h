#pragma once

#include "visitor_interface.h"

class QJsonObject;

VisitorInterface *createJsonVisitor();
QJsonObject takeRootFromJsonVisitor(VisitorInterface *);
