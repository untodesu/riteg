#include "riteg/pch.hh"

#include "riteg/loader_oneshot.hh"

#include "riteg/cmdline.hh"
#include "riteg/project.hh"

void Loader_OneShot::init(void)
{
    auto oneshot_path = cmdline::get("ipath");

    riteg_force_assert_msg(oneshot_path.size(), "Invalid argument [ipath]");

    m_is_done = false;
    m_path = oneshot_path;
}

void Loader_OneShot::step(void)
{
    if(!m_is_done) {
        project::load_input_RGBA(m_path);
        m_is_done = true;
    }
}
