SE_NAMESPACE(Godot)
/*
    Special Key:
{}
    The strategy here is similar to the one used by toolkits,
    which consists in leaving the 24 bits unicode range for printable
    characters, and use the upper 8 bits for special keys and
    modifiers. This way everything (char/keycode) can fit nicely in one 32 bits unsigned integer.
*/
enum {
    SPKEY = (1 << 24)
};
SE_CONSTANT(SPKEY)

