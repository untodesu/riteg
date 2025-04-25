#include "riteg/pch.hh"
#include "riteg/saver_oneshot.hh"

#include "riteg/cmdline.hh"
#include "riteg/project.hh"

void Saver_OneShot::init(void)
{
    auto oneshot_path = cmdline::get("opath");

    riteg_force_assert_msg(oneshot_path, "Invalid argument [opath]");

    m_is_done = false;
    m_path = oneshot_path;
}

void Saver_OneShot::step(void)
{
    if(!m_is_done) {
        if(Saver::write_source_RGBA(project::get_output_source(), m_path))
            riteg_info << "wrote image: " << m_path.string() << std::endl;
        else riteg_warning << "failed to write image: " << m_path.string() << std::endl;
        m_is_done = true;
    }
}
