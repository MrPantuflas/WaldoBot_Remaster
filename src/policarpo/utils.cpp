#include "policarpo/utils.hpp"
#include <string>
#include <chrono>

namespace policarpo {

/*std::string format_duration(std::chrono::milliseconds ms) {
    using namespace std::chrono;
    auto secs = duration_cast<seconds>(ms);
    ms -= duration_cast<milliseconds>(secs);
    auto mins = duration_cast<minutes>(secs);
    secs -= duration_cast<seconds>(mins);
    auto hrs = duration_cast<hours>(mins);
    

    std::stringstream ss;

    if (hrs.count() > 0) {
        mins -= duration_cast<minutes>(hrs);
        ss << hrs.count() << ":";
        if (mins.count() < 10)
            ss << "0";
    }

    ss << mins.count() << ":";
    if (secs.count() < 10)
        ss << "0";
    ss << secs.count();
    return ss.str();
}*/

} // namespace policarpo