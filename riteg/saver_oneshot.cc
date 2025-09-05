#include "riteg/pch.hh"

#include "riteg/saver_oneshot.hh"

#include "riteg/cmdline.hh"
#include "riteg/project.hh"

void Saver_OneShot::init(void)
{
    auto oneshot_path = cmdline::get("opath");

    riteg_force_assert_msg(oneshot_path.size(), "Invalid argument [opath]");

    m_is_done = false;
    m_path = oneshot_path;
}

void Saver_OneShot::step(void)
{
    if(!m_is_done) {
        if(Saver::write_source_RGBA(project::get_output_source(), m_path)) {
            LOG_INFO("wrote image: {}", m_path.string());
        }
        else {
            LOG_WARNING("failed to write image: {}", m_path.string());
        }

        m_is_done = true;
    }
}
