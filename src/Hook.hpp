#pragma once

#include <MinHook.h>
#include <stdexcept>

// RAII wrapper for MinHook
template <typename T>
struct Hook {
    T call;
    T src;

    explicit Hook()
        : call(nullptr)
        , src(nullptr)
    {
    }

    explicit Hook(T src, T tgt)
        : Hook()
    {
        install(src, tgt);
    }
    // Install directly into the object read by the detour. The original-call
    // trampoline must be published there BEFORE enabling the patch: another
    // thread can enter the detour inside MH_EnableHook, before it returns.
    void install(T source, T target)
    {
        if (src)
            throw std::runtime_error("Hook already installed");
        if (MH_CreateHook(reinterpret_cast<void*>(source), reinterpret_cast<void*>(target), reinterpret_cast<void**>(&call)) != MH_OK)
            throw std::runtime_error("Could not hook");
        src = source;
        try {
            enable();
        } catch (...) {
            MH_RemoveHook(reinterpret_cast<void*>(src));
            src = call = nullptr;
            throw;
        }
    }
    void enable()
    {
        if (MH_EnableHook(reinterpret_cast<void*>(src)) != MH_OK) {
            throw std::runtime_error("Could not enable hook");
        }
    }
    void disable()
    {
        if (MH_DisableHook(reinterpret_cast<void*>(src)) != MH_OK) {
            throw std::runtime_error("Could not disable hook");
        }
    }
    Hook(const Hook&) = delete;
    Hook(Hook&& rhs)
    {
        call = rhs.call;
        src = rhs.src;
        rhs.call = nullptr;
        rhs.src = nullptr;
    }
    Hook& operator=(const Hook&) = delete;
    Hook& operator=(Hook&& rhs) noexcept
    {
        call = rhs.call;
        src = rhs.src;
        rhs.call = nullptr;
        rhs.src = nullptr;
        return *this;
    }
    ~Hook()
    {
        if (src)
            MH_RemoveHook(reinterpret_cast<void*>(src));

        call = nullptr;
        src = nullptr;
    }
};

template <typename T, typename F>
T* get_vtable(F* obj)
{
    return (T*)(*(uintptr_t*)obj);
}
