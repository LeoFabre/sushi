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
 * @brief Implementation of external control interface of sushi.
 * @Copyright 2017-2023 Elk Audio AB, Stockholm
 */

#include "osc_controller.h"

namespace sushi::internal::engine::controller_impl {

OscController::OscController(BaseEngine* engine, CompletionSender* sender) : _event_dispatcher(engine->event_dispatcher()),
                                                                             _processors(engine->processor_container()),
                                                                             _sender(sender) {}

std::string OscController::get_send_ip() const
{
    if (_osc_frontend)
    {
        return _osc_frontend->send_ip();
    }
    return "";
}

int OscController::get_send_port() const
{
    if (_osc_frontend)
    {
        return _osc_frontend->send_port();
    }
    return 0;
}

int OscController::get_receive_port() const
{
    if (_osc_frontend)
    {
        return _osc_frontend->receive_port();
    }
    return 0;
}

std::vector<std::string> OscController::get_enabled_parameter_outputs() const
{
    if (_osc_frontend)
    {
        return _osc_frontend->get_enabled_parameter_outputs();
    }
    return {};
}

control::ControlResponse OscController::enable_output_for_parameter(int processor_id, int parameter_id)
{
    if (_osc_frontend == nullptr)
    {
        return {control::ControlStatus::UNSUPPORTED_OPERATION, 0};
    }

    auto lambda = [=, this] () -> int
    {
        // Here we SHOULD use name, since it is needed for building the OSC "Address Path".
        // We could avoid the _processors dependency here, though not crucial, by having 4 parameters to the call.

        auto processor = _processors->processor(processor_id);
        if (processor == nullptr)
        {
            return ControlEventStatus::NOT_FOUND;
        }

        auto parameter_descriptor = processor->parameter_from_id(parameter_id);
        if (parameter_descriptor == nullptr)
        {
            return ControlEventStatus::NOT_FOUND;
        }

        bool status = _osc_frontend->connect_from_parameter(processor->name(), parameter_descriptor->name());

        if (status == false)
        {
            return ControlEventStatus::ERROR;
        }

        return ControlEventStatus::OK;
    };

    std::unique_ptr<Event> event(new LambdaEvent(std::move(lambda), IMMEDIATE_PROCESS));
    return {control::ControlStatus::ASYNC_RESPONSE, _sender->send_with_completion_notification(std::move(event))};
}

control::ControlResponse OscController::disable_output_for_parameter(int processor_id, int parameter_id)
{
    if (_osc_frontend == nullptr)
    {
        return {control::ControlStatus::UNSUPPORTED_OPERATION, 0};
    }

    auto lambda = [=, this] () -> int
    {
        // Here we SHOULD use name, since it is needed for building the OSC "Address Path".
        // We could avoid the _processors dependency here, though not crucial, by having 4 parameters to the call.

        auto processor = _processors->processor(processor_id);
        if (processor == nullptr)
        {
            return EventStatus::ERROR;
        }

        auto parameter_descriptor = processor->parameter_from_id(parameter_id);
        if (parameter_descriptor == nullptr)
        {
            return ControlEventStatus::NOT_FOUND;
        }

        bool status = _osc_frontend->disconnect_from_parameter(processor->name(), parameter_descriptor->name());

        if (status == false)
        {
            return ControlEventStatus::ERROR;
        }

        return ControlEventStatus::OK;
    };

    std::unique_ptr<Event> event(new LambdaEvent(std::move(lambda), IMMEDIATE_PROCESS));
    return {control::ControlStatus::ASYNC_RESPONSE, _sender->send_with_completion_notification(std::move(event))};
}

void OscController::set_osc_frontend(control_frontend::OSCFrontend* osc_frontend)
{
    _osc_frontend = osc_frontend;
}

control::ControlResponse OscController::enable_all_output()
{
    if (_osc_frontend == nullptr)
    {
        return {control::ControlStatus::UNSUPPORTED_OPERATION, 0};
    }

    auto lambda = [=, this] () -> int
    {
        _osc_frontend->connect_from_all_parameters();
        return ControlEventStatus::OK;
    };

    std::unique_ptr<Event> event(new LambdaEvent(std::move(lambda), IMMEDIATE_PROCESS));
    return {control::ControlStatus::ASYNC_RESPONSE, _sender->send_with_completion_notification(std::move(event))};
}

control::ControlResponse OscController::disable_all_output()
{
    if (_osc_frontend == nullptr)
    {
        return {control::ControlStatus::UNSUPPORTED_OPERATION, 0};
    }

    auto lambda = [=, this] () -> int
    {
        _osc_frontend->disconnect_from_all_parameters();
        return ControlEventStatus::OK;
    };

    std::unique_ptr<Event> event(new LambdaEvent(std::move(lambda), IMMEDIATE_PROCESS));
    return {control::ControlStatus::ASYNC_RESPONSE, _sender->send_with_completion_notification(std::move(event))};
}

} // end namespace sushi::internal::engine::controller_impl