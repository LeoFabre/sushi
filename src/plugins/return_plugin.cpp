/*
 * Copyright 2017-2023 Elk Audio AB
 *
 * SUSHI is free software: you can redistribute it and/or modify it under the terms of
 * the GNU Affero General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * SUSHI is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License along with
 * SUSHI. If not, see http://www.gnu.org/licenses/
 */

/**
 * @brief Aux return plugin to return audio from a send plugin
 * @Copyright 2017-2023 Elk Audio AB, Stockholm
 */

#include <algorithm>

#include "elklog/static_logger.h"

#include "return_plugin.h"
#include "send_plugin.h"

ELKLOG_GET_LOGGER_WITH_MODULE_NAME("return_plugin");

namespace sushi::internal::return_plugin {

constexpr auto PLUGIN_UID = "sushi.testing.return";
constexpr auto DEFAULT_LABEL = "Return";

ReturnPlugin::ReturnPlugin(HostControl host_control, SendReturnFactory* manager) : InternalPlugin(host_control),
                                                                                   _manager(manager)
{
    Processor::set_name(PLUGIN_UID);
    Processor::set_label(DEFAULT_LABEL);
    _max_input_channels = MAX_SEND_CHANNELS;
    _max_output_channels = MAX_SEND_CHANNELS;
}

ReturnPlugin::~ReturnPlugin()
{
    _manager->on_return_destruction(this);
    for (auto& sender : _senders)
    {
        sender->clear_destination();
    }
}

void ReturnPlugin::send_audio(const ChunkSampleBuffer& buffer, int start_channel, float gain, int thread_id)
{
    std::scoped_lock<SpinLock> lock(_buffer_lock);
    _maybe_swap_buffers(_host_control.transport()->current_samples());

    /* If the sender invoking this function is on the same thread, and process_audio() has not yet been called, we can
     * copy directly to _active_out (i.e. zero delay), if not, we must copy to _active_in (with 1 buffer delay) */
    auto target_buffer = (thread_id == _current_processing_thread && !_processed_this_block)? _active_out : _active_in;

    int max_channels = std::max(0, std::min(buffer.channel_count(), _current_output_channels - start_channel));

    for (int c = 0 ; c < max_channels; ++c)
    {
        target_buffer->add_with_gain(start_channel++, c, buffer, gain);
    }
}

void ReturnPlugin::send_audio_with_ramp(const ChunkSampleBuffer& buffer, int start_channel,
                                        float start_gain, float end_gain, int thread_id)
{
    std::scoped_lock<SpinLock> lock(_buffer_lock);
    _maybe_swap_buffers(_host_control.transport()->current_samples());

    /* See comment in send_audio() */
    auto target_buffer = (thread_id == _current_processing_thread && !_processed_this_block)? _active_out : _active_in;

    int max_channels = std::max(0, std::min(buffer.channel_count(), _current_output_channels - start_channel));

    for (int c = 0 ; c < max_channels; ++c)
    {
        target_buffer->add_with_ramp(start_channel++, c, buffer, start_gain, end_gain);
    }
}

void ReturnPlugin::add_sender(send_plugin::SendPlugin* sender)
{
    _senders.push_back(sender);
}

void ReturnPlugin::remove_sender(send_plugin::SendPlugin* sender)
{
    _senders.erase(std::remove(_senders.begin(), _senders.end(), sender), _senders.end());
}

ProcessorReturnCode ReturnPlugin::init(float sample_rate)
{
    configure(sample_rate);
    return ProcessorReturnCode::OK;
}

void ReturnPlugin::configure(float sample_rate)
{
    _sample_rate = sample_rate;
    for (auto& buffer : _buffers)
    {
        buffer.clear();
    }
}

void ReturnPlugin::set_channels(int inputs, int outputs)
{
    Processor::set_channels(inputs, outputs);

    int max_channels = std::max(inputs, outputs);
    if (_buffers.front().channel_count() != max_channels)
    {
        _buffers.fill(ChunkSampleBuffer(max_channels));
    }
}

void ReturnPlugin::set_enabled(bool enabled)
{
    if (enabled == false)
    {
        for (auto& buffer : _buffers)
        {
            buffer.clear();
        }
    }
}

void ReturnPlugin::process_event(const RtEvent& event)
{
    switch (event.type())
    {
        case RtEventType::SET_BYPASS:
        {
            bool bypassed = static_cast<bool>(event.processor_command_event()->value());
            Processor::set_bypassed(bypassed);
            _bypass_manager.set_bypass(bypassed, _sample_rate);
            break;
        }
        default:
            InternalPlugin::process_event(event);
            break;
    }
}

void ReturnPlugin::process_audio(const ChunkSampleBuffer& /*in_buffer*/, ChunkSampleBuffer& out_buffer)
{
    {
        std::scoped_lock<SpinLock> lock(_buffer_lock);
        _maybe_swap_buffers(_host_control.transport()->current_samples());
    }

    if (_bypass_manager.should_process())
    {
        auto buffer = ChunkSampleBuffer::create_non_owning_buffer(*_active_out, 0, out_buffer.channel_count());
        out_buffer.replace(buffer);

        if (_bypass_manager.should_ramp())
        {
            _bypass_manager.ramp_output(out_buffer);
        }
    }
    else
    {
        out_buffer.clear();
    }
    _processed_this_block = true;
}

bool ReturnPlugin::bypassed() const
{
    return _bypass_manager.bypassed();
}

void ReturnPlugin::set_bypassed(bool bypassed)
{
    _host_control.post_event(std::make_unique<SetProcessorBypassEvent>(this->id(), bypassed, IMMEDIATE_PROCESS));
}

std::string_view ReturnPlugin::static_uid()
{
    return PLUGIN_UID;
}

void inline ReturnPlugin::_swap_buffers()
{
    std::swap(_active_in, _active_out);
    _active_in->clear();
}

void inline ReturnPlugin::_maybe_swap_buffers(int64_t current_samples)
{
    int64_t prev_samples = _last_process_samples.load(std::memory_order_acquire);
    ELKLOG_LOG_INFO("maybe_swap_buffers() called. Current: {}, prev: {}, _processed_this_block: {}, delta: {}",
        current_samples, prev_samples, _processed_this_block, current_samples - prev_samples);

    if (prev_samples != current_samples)
    {
        _last_process_samples.store(current_samples, std::memory_order_release);
        _processed_this_block = false;
        _swap_buffers();
    }
}

} // end namespace sushi::internal::return_plugin