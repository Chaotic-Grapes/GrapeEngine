/* Start Header *****************************************************************/
/*!
\file   MessageTypes.h
\author Muhammad Nur Fadzly Bin Zulkifli
\par    muhammadnurfadzly.b@digipen.edu
\brief
This file contains the various types of messages that can be sent through the
MessageSystem class.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header
********************************************************************************/

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

    struct MousePressed
    {
        int Button;
    };

    struct MouseReleased
    {
        int Button;
    };
}

#endif // MESSAGETYPES_H
