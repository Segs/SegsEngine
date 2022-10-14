/*
    Special Key:
{}
    The strategy here is similar to the one used by toolkits,
    which consists in leaving the 24 bits unicode range for printable
    characters, and use the upper 8 bits for special keys and
    modifiers. This way everything (char/keycode) can fit nicely in one 32 bits unsigned integer.
*/

enum KeyList {
    /* CURSOR/FUNCTION/BROWSER/MULTIMEDIA/MISC KEYS */
    KEY_ESCAPE = SPKEY | 0x01,
    KEY_TAB = SPKEY | 0x02,
    KEY_BACKTAB = SPKEY | 0x03,
    KEY_BACKSPACE = SPKEY | 0x04,
    KEY_ENTER = SPKEY | 0x05,
    KEY_KP_ENTER = SPKEY | 0x06,
};
SE_ENUM(KeyList)

