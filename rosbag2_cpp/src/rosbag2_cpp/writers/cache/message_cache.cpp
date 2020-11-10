// Copyright 2020, Robotec.ai sp. z o.o.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <memory>
#include <utility>

#include "rosbag2_cpp/logging.hpp"
#include "rosbag2_cpp/writers/cache/message_cache.hpp"

namespace rosbag2_cpp
{
namespace writers
{
namespace cache
{

MessageCache::MessageCache(const uint64_t & max_buffer_size)
{
  primary_buffer_ = std::make_shared<MessageCacheBuffer>(max_buffer_size);
  secondary_buffer_ = std::make_shared<MessageCacheBuffer>(max_buffer_size);
}

bool MessageCache::push(std::shared_ptr<const rosbag2_storage::SerializedBagMessage> msg)
{
  // While pushing, we keep track of inserted and dropped messages as well
  bool pushed = false;
  {
    std::lock_guard<std::mutex> buffer_lock(buffer_mutex_);
    pushed = primary_buffer_->push(msg);
  }

  allow_swap();

  if (!pushed) {
    elements_dropped_++;
  }
  return pushed;
}

void MessageCache::allow_swap()
{
  swap_ready_.notify_one();
}

void MessageCache::swap_when_allowed()
{
  std::unique_lock<std::mutex> lock(buffer_mutex_);
  swap_ready_.wait(lock);
  std::swap(primary_buffer_, secondary_buffer_);
}

std::shared_ptr<MessageCacheBuffer> MessageCache::consumer_buffer()
{
  return secondary_buffer_;
}

void MessageCache::log_dropped()
{
  if (elements_dropped_ > 0) {
    ROSBAG2_CPP_LOG_WARN_STREAM(
      "Cache buffers total lost messages: " <<
        elements_dropped_ <<
        " where " <<
        primary_buffer_->size() + secondary_buffer_->size() <<
        " left in buffers.");
  }
}

MessageCache::~MessageCache()
{
  log_dropped();
}

}  // namespace cache
}  // namespace writers
}  // namespace rosbag2_cpp
