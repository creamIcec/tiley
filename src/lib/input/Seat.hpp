#ifndef __SEAT_H__
#define __SEAT_H__

#include <LSeat.h>

using namespace Louvre;

namespace tiley{
    class Seat final : public LSeat{
        public:
            using LSeat::LSeat;
            void configureInputDevices() noexcept;
    };
}

#endif  //__SEAT_H__