#include "riteg/pch.hh"
#include "riteg/loader_sprintf.hh"

#include "riteg/cmdline.hh"
#include "riteg/project.hh"

void Loader_Sprintf::init(void)
{
    auto sprintf_format = cmdline::get("iformat");
    auto sprintf_limit = cmdline::get("imaxframes");

    riteg_force_assert_msg(sprintf_format, "Invalid argument [iformat]");

    m_format = sprintf_format;
    m_limit = sprintf_limit ? std::stoul(sprintf_limit) : SIZE_MAX;

    riteg_force_assert_msg(m_limit > 0, "Invalid argument [imaxframes]");

    m_frame = 1;
    m_buffer.clear();
    m_is_done = false;
}

void Loader_Sprintf::step(void)
{
    if(!m_is_done && m_frame <= m_limit) {
        m_buffer.resize(2 + std::snprintf(nullptr, 0, m_format.c_str(), static_cast<int>(m_frame)));
        std::snprintf(m_buffer.data(), m_buffer.size(), m_format.c_str(), static_cast<int>(m_frame));

        if(!project::load_input_RGBA(m_buffer)) {
            // Consider this a completeion
            m_is_done = true;
            return;
        }

        m_frame += 1;
    }
}
