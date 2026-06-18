#ifndef UTIL_H
#define UTIL_H

namespace JCore {

template<typename T>
void DeletePtr(T *& ptr)
{
    if (ptr) {
        delete ptr;
        ptr = nullptr;
    }
}

template<typename VecPtr>
void ReleaseVecPtr(std::vector<VecPtr*> &vec)
{
    while (!vec.empty()) {
        auto ptr = vec.begin();
        if (*ptr) {
            delete *ptr;
            *ptr = nullptr;
        }
        vec.erase(ptr);
    }
    vec.clear();
    vec.shrink_to_fit();
}

template<typename VecEle>
void ReleaseVecEle(std::vector<VecEle> &vec)
{
    if (!vec.empty()) {
        vec.clear();
        vec.shrink_to_fit();
    }
}
} // namespace JCore

#endif
