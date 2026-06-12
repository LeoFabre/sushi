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
 * @brief Interface for plugins that send audio to a return plugin
 * @Copyright 2017-2023 Elk Audio AB, Stockholm
 */

#ifndef SUSHI_SENDER_INTERFACE_H
#define SUSHI_SENDER_INTERFACE_H

namespace sushi::internal {

namespace return_plugin { class ReturnPlugin; }

/**
 * @brief Interface implemented by plugins that push audio to a ReturnPlugin,
 *        so that the return plugin can notify them if it is destroyed while
 *        they still hold a pointer to it.
 */
class SenderInterface
{
public:
    virtual ~SenderInterface() = default;

    /**
     * @brief Called by a ReturnPlugin from its destructor so that senders
     *        can clear any pointers to it.
     * @param destination The ReturnPlugin instance being destroyed.
     */
    virtual void return_deleted(return_plugin::ReturnPlugin* destination) = 0;
};

} // end namespace sushi::internal

#endif // SUSHI_SENDER_INTERFACE_H
