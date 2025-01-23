/*
* Copyright 2017-2025 Elk Audio AB
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
* @brief Extraction interface for sending events with a completion callback
* @Copyright 2017-2025 Elk Audio AB, Stockholm
*/

#ifndef SUSHI_LIBRARY_COMPLETION_SENDER_H
#define SUSHI_LIBRARY_COMPLETION_SENDER_H

#include <memory>

#include "library/event.h"

namespace sushi::internal::engine {

/* Extension to EventStatus for Controller Events */
enum ControlEventStatus : int
{
    OK                    = sushi::internal::EventStatus::HANDLED_OK,
    ERROR                 = sushi::internal::EventStatus::ERROR,
    UNSUPPORTED_OPERATION = sushi::internal::EventStatus::EVENT_SPECIFIC,
    NOT_FOUND,
    OUT_OF_RANGE,
    INVALID_ARGUMENTS
};

/**
 * @brief Interface to send events with completion notification
 */
class CompletionSender
{
public:
    virtual ~CompletionSender() = default;

    /**
     * @brief Sends an event and adds a completion callback
     * @param event Event to send
     * @return The event id
     */
    virtual int send_with_completion_notification([[maybe_unused]] std::unique_ptr<Event> event)
    {
        return 0;
    };
};

} // end namespace sushi::internal::engine::controller_impl

#endif //SUSHI_LIBRARY_COMPLETION_SENDER_H
