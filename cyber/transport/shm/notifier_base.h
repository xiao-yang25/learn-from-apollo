/******************************************************************************
 * Copyright 2018 The Apollo Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

#ifndef CYBER_TRANSPORT_SHM_NOTIFIER_BASE_H_
#define CYBER_TRANSPORT_SHM_NOTIFIER_BASE_H_

#include <memory>

#include "cyber/transport/shm/readable_info.h"

namespace apollo {
namespace cyber {
namespace transport {

class NotifierBase;
using NotifierPtr = NotifierBase*;
//  通知机制是通过NotifierBase实现的。它有两个实现类，分别为ConditionNotifier和MulticastNotifier。
//  前者为默认设置。它会单独开一块共享共享专门用于通知，其中包含了ReadableInfo等信息。
//  MulticastNotifier的主要区别是它是通过指定的socket广播
class NotifierBase {
 public:
  virtual ~NotifierBase() = default;

  virtual void Shutdown() = 0;
  virtual bool Notify(const ReadableInfo& info) = 0;
  virtual bool Listen(int timeout_ms, ReadableInfo* info) = 0;
};

}  // namespace transport
}  // namespace cyber
}  // namespace apollo

#endif  // CYBER_TRANSPORT_SHM_NOTIFIER_BASE_H_
