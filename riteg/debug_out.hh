#ifndef RITEG_DEBUG_OUT_HH
#define RITEG_DEBUG_OUT_HH 1
#pragma once

class DebugOutWrapper final {
public:
    explicit DebugOutWrapper(void) = default;
    virtual ~DebugOutWrapper(void);

public:
    template<typename arg_type>
    inline DebugOutWrapper& operator<<(const arg_type &arg);

private:
    std::ostringstream stream;
};

template<typename arg_type>
inline DebugOutWrapper& DebugOutWrapper::operator<<(const arg_type &arg)
{
    stream << arg;
    return *this;
}

#define debug_out DebugOutWrapper()

#endif /* RITEG_DEBUG_OUT_HH */
