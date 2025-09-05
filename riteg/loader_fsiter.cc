#include "riteg/pch.hh"

#include "riteg/loader_fsiter.hh"

#include "riteg/cmdline.hh"
#include "riteg/project.hh"

void Loader_FSIter::init(void)
{
    auto fsiter_path = cmdline::get("ipath");

    riteg_force_assert_msg(fsiter_path.size(), "Invalid argument [ipath]");

    for(auto& entry : std::filesystem::directory_iterator(fsiter_path)) {
        if(entry.is_regular_file()) {
            m_paths.emplace(entry.path());
        }
    }

    riteg_force_assert_msg(!m_paths.empty(), "No files found");
}

void Loader_FSIter::step(void)
{
    if(m_paths.empty()) {
        // Consider this a completion
        return;
    }

    project::load_input_RGBA(m_paths.front());

    m_paths.pop();
}
