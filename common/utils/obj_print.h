#ifndef OBJ_PRINT_H
#define OBJ_PRINT_H

#include <string>
#include <sstream>

namespace JCore {

template <typename T>
std::string ObjToStr(const T& object) {
    std::stringstream ss;
    ss << object;
    return ss.str();
}

} // namespace JCore

#endif // OBJ_PRINT_H
