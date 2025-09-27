#ifndef MESSAGETYPES_H
#define MESSAGETYPES_H

namespace Messaging {
    struct WindowResized {
        int Width;
        int Height;
    };

    struct KeyPressed {
        int Key;
    };

    struct KeyReleased {
        int Key;
    };
}

#endif // MESSAGETYPES_H
