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
 * @brief Cabinet simulator from Brickworks library
 * @copyright 2017-2023, Elk Audio AB, Stockholm
 */

#include <cassert>

#include "cab_sim_plugin.h"

namespace sushi::internal::cab_sim_plugin {

constexpr auto PLUGIN_UID = "sushi.brickworks.cab_sim";
constexpr auto DEFAULT_LABEL = "Cab Simulator";


CabSimPlugin::CabSimPlugin(HostControl host_control) : InternalPlugin(host_control)
{
    _max_input_channels = MAX_TRACK_CHANNELS;
    _max_output_channels = MAX_TRACK_CHANNELS;
    Processor::set_name(PLUGIN_UID);
    Processor::set_label(DEFAULT_LABEL);

    _cutoff_low = register_float_parameter("cutoff_low", "Cutoff Low", "",
                                           0.5f, 0.0f, 1.0f,
                                           Direction::AUTOMATABLE,
                                           new FloatParameterPreProcessor(0.0f, 1.0f));
    _cutoff_high = register_float_parameter("cutoff_high", "Cutoff High", "",
                                            0.5f, 0.0f, 1.0f,
                                            Direction::AUTOMATABLE,
                                            new FloatParameterPreProcessor(0.0f, 1.0f));
    _tone = register_float_parameter("tone", "Tone", "",
                                     0.5f, 0.0f, 1.0f,
                                     Direction::AUTOMATABLE,
                                     new FloatParameterPreProcessor(0.0f, 1.0f));

    assert(_cutoff_low);
    assert(_cutoff_high);
    assert(_tone);
}

ProcessorReturnCode CabSimPlugin::init(float sample_rate)
{
    bw_cab_init(&_cab_coeffs);
    bw_cab_set_sample_rate(&_cab_coeffs, sample_rate);
    return ProcessorReturnCode::OK;
}

void CabSimPlugin::configure(float sample_rate)
{
    bw_cab_set_sample_rate(&_cab_coeffs, sample_rate);
    return;
}

void CabSimPlugin::set_enabled(bool enabled)
{
    Processor::set_enabled(enabled);
    bw_cab_reset_coeffs(&_cab_coeffs);
    for (int i = 0; i < MAX_TRACK_CHANNELS; i++)
    {
        bw_cab_reset_state(&_cab_coeffs, &_cab_state[i], 0.0f);
    }
}

void CabSimPlugin::set_bypassed(bool bypassed)
{
    _host_control.post_event(std::make_unique<SetProcessorBypassEvent>(this->id(), bypassed, IMMEDIATE_PROCESS));
}

void CabSimPlugin::process_event(const RtEvent& event)
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

void CabSimPlugin::process_audio(const ChunkSampleBuffer &in_buffer, ChunkSampleBuffer &out_buffer)
{
    /* Update parameter values */
    bw_cab_set_cutoff_low(&_cab_coeffs, _cutoff_low->processed_value());
    bw_cab_set_cutoff_high(&_cab_coeffs, _cutoff_high->processed_value());
    bw_cab_set_tone(&_cab_coeffs, _tone->processed_value());

    if (_bypass_manager.should_process())
    {
        std::array<const float *, MAX_TRACK_CHANNELS> in_channel_ptrs {};
        std::array<float *, MAX_TRACK_CHANNELS> out_channel_ptrs {};

        for (int i = 0; i < _current_input_channels; i++)
        {
            in_channel_ptrs[i] = in_buffer.channel(i);
            out_channel_ptrs[i] = out_buffer.channel(i);
        }

        bw_cab_update_coeffs_ctrl(&_cab_coeffs);
        for (int n = 0; n < AUDIO_CHUNK_SIZE; n++)
        {
            bw_cab_update_coeffs_audio(&_cab_coeffs);
            for (int i = 0; i < _current_input_channels; i++)
            {
                *out_channel_ptrs[i]++ = bw_cab_process1(&_cab_coeffs, &_cab_state[i],
                                                         *in_channel_ptrs[i]++);
            }
        }
        if (_bypass_manager.should_ramp())
        {
            _bypass_manager.crossfade_output(in_buffer, out_buffer,
                                             _current_input_channels,
                                             _current_output_channels);
        }
    }
    else
    {
        bypass_process(in_buffer, out_buffer);
    }
}

std::string_view CabSimPlugin::static_uid()
{
    return PLUGIN_UID;
}


} // namespace sushi::internal::cab_sim_plugin
