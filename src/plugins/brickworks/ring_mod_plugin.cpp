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
 * @brief Ring modulator from Brickworks library
 * @copyright 2017-2023, Elk Audio AB, Stockholm
 */

#include <cassert>
#include <cmath>

#include <bw_osc_sin.h>

#include "ring_mod_plugin.h"

namespace sushi::internal::ring_mod_plugin {

constexpr auto PLUGIN_UID = "sushi.brickworks.ring_mod";
constexpr auto DEFAULT_LABEL = "Ring Modulator";


RingModPlugin::RingModPlugin(HostControl host_control) : InternalPlugin(host_control)
{
    _max_input_channels = MAX_TRACK_CHANNELS;
    _max_output_channels = MAX_TRACK_CHANNELS;
    Processor::set_name(PLUGIN_UID);
    Processor::set_label(DEFAULT_LABEL);

    _frequency = register_float_parameter("frequency", "Frequency", "Hz",
                                          1000.0f, 20.0f, 20000.0f,
                                          Direction::AUTOMATABLE,
                                          new CubicWarpPreProcessor(20.0f, 20000.0f));
    _amount = register_float_parameter("amount", "Amount", "",
                                       0.0f, -1.0f, 1.0f,
                                       Direction::AUTOMATABLE,
                                       new FloatParameterPreProcessor(-1.0f, 1.0f));

    assert(_frequency);
    assert(_amount);
}

ProcessorReturnCode RingModPlugin::init(float sample_rate)
{
    bw_phase_gen_init(&_phase_gen_coeffs);
    bw_phase_gen_set_sample_rate(&_phase_gen_coeffs, sample_rate);
    bw_ring_mod_init(&_ring_mod_coeffs);
    bw_ring_mod_set_sample_rate(&_ring_mod_coeffs, sample_rate);
    return ProcessorReturnCode::OK;
}

void RingModPlugin::configure(float sample_rate)
{
    bw_phase_gen_set_sample_rate(&_phase_gen_coeffs, sample_rate);
    bw_ring_mod_set_sample_rate(&_ring_mod_coeffs, sample_rate);
    return;
}

void RingModPlugin::set_enabled(bool enabled)
{
    Processor::set_enabled(enabled);
    bw_phase_gen_reset_coeffs(&_phase_gen_coeffs);
    bw_ring_mod_reset_coeffs(&_ring_mod_coeffs);
    for (int i = 0; i < MAX_TRACK_CHANNELS; i++)
    {
        float v, v_inc;
        bw_phase_gen_reset_state(&_phase_gen_coeffs, &_phase_gen_state[i], 0.0f, &v, &v_inc);
    }
}

void RingModPlugin::set_bypassed(bool bypassed)
{
    _host_control.post_event(std::make_unique<SetProcessorBypassEvent>(this->id(), bypassed, IMMEDIATE_PROCESS));
}

void RingModPlugin::process_event(const RtEvent& event)
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

void RingModPlugin::process_audio(const ChunkSampleBuffer &in_buffer, ChunkSampleBuffer &out_buffer)
{
    /* Update parameter values */
    bw_phase_gen_set_frequency(&_phase_gen_coeffs, _frequency->processed_value());
    bw_ring_mod_set_amount(&_ring_mod_coeffs, _amount->processed_value());

    if (_bypass_manager.should_process())
    {
        std::array<const float *, MAX_TRACK_CHANNELS> in_channel_ptrs {};
        std::array<float *, MAX_TRACK_CHANNELS> out_channel_ptrs {};

        for (int i = 0; i < _current_input_channels; i++)
        {
            in_channel_ptrs[i] = in_buffer.channel(i);
            out_channel_ptrs[i] = out_buffer.channel(i);
        }

        bw_phase_gen_update_coeffs_ctrl(&_phase_gen_coeffs);
        bw_ring_mod_update_coeffs_ctrl(&_ring_mod_coeffs);
        for (int n = 0; n < AUDIO_CHUNK_SIZE; n++)
        {
            bw_phase_gen_update_coeffs_audio(&_phase_gen_coeffs);
            bw_ring_mod_update_coeffs_audio(&_ring_mod_coeffs);
            for (int i = 0; i < _current_input_channels; i++)
            {
                float phase, phase_inc;
                bw_phase_gen_process1(&_phase_gen_coeffs, &_phase_gen_state[i], &phase, &phase_inc);
                float modulator = bw_osc_sin_process1(phase);
                *out_channel_ptrs[i]++ = bw_ring_mod_process1(&_ring_mod_coeffs, *in_channel_ptrs[i]++, modulator);
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

std::string_view RingModPlugin::static_uid()
{
    return PLUGIN_UID;
}


} // namespace sushi::internal::ring_mod_plugin
