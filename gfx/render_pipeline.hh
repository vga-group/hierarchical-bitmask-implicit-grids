#ifndef RAYBASE_GFX_RENDER_PIPELINE_HH
#define RAYBASE_GFX_RENDER_PIPELINE_HH

#include "video_output.hh"
#include "event.hh"
#include "blit_stage.hh"
#include <optional>

namespace rb::gfx
{

class render_pipeline
{
public:
    render_pipeline();
    render_pipeline(video_output& output);
    virtual ~render_pipeline();

    void set_video_output(video_output* output);
    video_output* get_video_output() const;

    void reinit();
    void render();

    // Warning: the screenshot is saved for the next queued frame and may not
    // be saved immediately, this is somewhat asynchronous. But it shouldn't
    // harm the framerate too much!
    void screenshot(const std::string& output_path);

protected:
    // Returns the target that the renderer generates.
    virtual render_target reset() = 0;
    virtual event render_stages(uint32_t frame_index, event frame_start) = 0;

    video_output* output;

private:
    event do_screenshot(const std::string& path, event e);

    std::vector<std::string> screenshot_queue;
    render_target pipeline_output_target;
    std::optional<blit_stage> blit;
};

}

#endif
