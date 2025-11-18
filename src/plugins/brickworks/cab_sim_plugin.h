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
 * @copyright 2017-2023 Elk Audio AB, Stockholm
 */

#ifndef CAB_SIM_PLUGIN_H
#define CAB_SIM_PLUGIN_H

#include <bw_cab.h>

#include "library/internal_plugin.h"

ELK_PUSH_WARNING
ELK_DISABLE_DOMINANCE_INHERITANCE

namespace sushi::internal::cab_sim_plugin {

class CabSimPlugin : public InternalPlugin, public UidHelper<CabSimPlugin>
{
public:
    explicit CabSimPlugin(HostControl hostControl);

    ~CabSimPlugin() override = default;

    ProcessorReturnCode init(float sample_rate) override;

    void configure(float sample_rate) override;

    void set_enabled(bool enabled) override;

    void set_bypassed(bool bypassed) override;

    void process_event(const RtEvent& event) override;

    void process_audio(const ChunkSampleBuffer &in_buffer, ChunkSampleBuffer &out_buffer) override;

    static std::string_view static_uid();

private:
    BypassManager _bypass_manager;
    float _sample_rate{0};

    FloatParameterValue* _cutoff_low;
    FloatParameterValue* _cutoff_high;
    FloatParameterValue* _tone;

    bw_cab_coeffs _cab_coeffs;
    std::array<bw_cab_state, MAX_TRACK_CHANNELS> _cab_state;
};

} // namespace sushi::internal::cab_sim_plugin

ELK_POP_WARNING

#endif // CAB_SIM_PLUGIN_H
