#pragma once

struct TS_Base;
struct TS_TypeLike;

struct TS_Enum;
struct TS_Type;
struct TS_Namespace;
struct TS_Property;
struct TS_Signal;
struct TS_Function;
struct TS_Constant;
struct TS_Field;
struct TypeReference;

class VisitorInterface {
public:
    virtual void visit(const TS_Enum *) = 0;
    virtual void visit(const TS_Type *) = 0;
    virtual void visit(const TS_Namespace *) = 0;
    virtual void visit(const TS_Property *) = 0;
    virtual void visit(const TS_Signal *) = 0;
    virtual void visit(const TS_Function *) = 0;
    virtual void visit(const TS_Constant *) = 0;
    virtual void visit(const TS_Field *) = 0;

    virtual ~VisitorInterface() {}
};

