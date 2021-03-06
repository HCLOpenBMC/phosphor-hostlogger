/**
 * @brief Log storage.
 *
 * This file is part of HostLogger project.
 *
 * Copyright (c) 2018 YADRO
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "log_storage.hpp"

#include "config.hpp"
#include "log_file.hpp"

#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <exception>

void LogStorage::parse(const char* data, size_t len)
{
    // Split log stream to separate messages.
    // Stream may not be ended with EOL, so we handle this situation by
    // last_complete_ flag.
    size_t pos = 0;
    while (pos < len)
    {
        // Search for EOL ('\n')
        size_t eol = pos;
        while (eol < len && data[eol] != '\n')
            ++eol;
        const bool eolFound = eol < len;
        const char* msgText = data + pos;
        size_t msgLen = (eolFound ? eol : len) - pos;

        // Remove '\r' from the end of message
        while (msgLen && msgText[msgLen - 1] == '\r')
            --msgLen;

        // Append message to store
        if (msgLen)
            append(msgText, msgLen);

        pos = eol + 1; // Skip '\n'
        last_complete_ = eolFound;
    }
}

void LogStorage::append(const char* msg, size_t len)
{
    if (!last_complete_)
    {
        // The last message is incomplete, add msg as part of it
        if (!messages_.empty())
        {
            Message& last_msg = *messages_.rbegin();
            last_msg.text.append(msg, len);
        }
    }
    else
    {
        Message new_msg;
        time(&new_msg.timeStamp);
        new_msg.text.assign(msg, len);
        messages_.push_back(new_msg);
        shrink();
    }
}

void LogStorage::clear()
{
    messages_.clear();
    last_complete_ = true;
}

bool LogStorage::empty() const
{
    return messages_.empty();
}

int LogStorage::save(const char* fileName) const
{
    int rc = 0;

    if (empty())
    {
        printf("No messages to write\n");
        return 0;
    }

    try
    {
        LogFile log(fileName);

        // Write full datetime stamp as the first record
        const time_t& tmStart = messages_.begin()->timeStamp;
        tm tmLocal;
        localtime_r(&tmStart, &tmLocal);
        char tmText[20]; // size of "%F %T" asciiz (YYYY-MM-DD HH:MM:SS)
        strftime(tmText, sizeof(tmText), "%F %T", &tmLocal);
        std::string titleMsg = ">>> Log collection started at ";
        titleMsg += tmText;
        log.write(tmStart, titleMsg);

        // Write messages
        for (auto it = messages_.begin(); it != messages_.end(); ++it)
            log.write(it->timeStamp, it->text);

        log.close();
    }
    catch (std::exception& e)
    {
        rc = EIO;
        fprintf(stderr, "%s\n", e.what());
    }

    return rc;
}

void LogStorage::shrink()
{
    if (loggerConfig.storageSizeLimit)
    {
        while (messages_.size() >
               static_cast<size_t>(loggerConfig.storageSizeLimit))
            messages_.pop_front();
    }
    if (loggerConfig.storageTimeLimit)
    {
        // Get time for N hours ago
        time_t oldestTimeStamp;
        time(&oldestTimeStamp);
        oldestTimeStamp -= loggerConfig.storageTimeLimit * 60 * 60;
        while (!messages_.empty() &&
               messages_.begin()->timeStamp < oldestTimeStamp)
            messages_.pop_front();
    }
}
