#include "riteg/precompiled.hh"
#include "riteg/debug_out.hh"

DebugOutWrapper::~DebugOutWrapper(void)
{
    std::cerr << stream.str();
    std::cerr << std::endl;
}
