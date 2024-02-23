#include "render_pipeline.hh"
#include "vulkan_helpers.hh"
#include "context.hh"

namespace rb::gfx
{

render_pipeline::render_pipeline()
: output(nullptr)
{
}

render_pipeline::render_pipeline(video_output& output)
: output(&output)
{
}

render_pipeline::~render_pipeline()
{
}

void render_pipeline::set_video_output(video_output* output)
{
    this->output = output;
    reinit();
}

video_output* render_pipeline::get_video_output() const
{
    return output;
}

void render_pipeline::reinit()
{
    if(!output) return;

    render_target target = reset();
    pipeline_output_target = target;
    blit.emplace(output->get_device(), target, *output);
}

void render_pipeline::render()
{
    if(!output) return;

    event swapchain_event;
    while(output->begin_frame(swapchain_event))
    {
        output->reset_swapchain();
        reinit();
    }

    if(!blit) reinit();

    uint32_t frame_index = output->get_device().get_in_flight_index();
    event e = render_stages(frame_index, swapchain_event);
    for(const std::string& path: screenshot_queue)
        e = do_screenshot(path, e);
    screenshot_queue.clear();
    e = blit->run(frame_index, e);

    if(output->end_frame(e))
    {
        output->reset_swapchain();
        reinit();
    }
}

void render_pipeline::screenshot(const std::string& output_path)
{
    screenshot_queue.push_back(output_path);
}

event render_pipeline::do_screenshot(const std::string& path, event e)
{
    if(!output) return {};
    return async_save_image(
        output->get_device(), path, pipeline_output_target.get_image(), pipeline_output_target.get_layout(),
        pipeline_output_target.get_format(), pipeline_output_target.get_size(), e, false
    );
}

}
