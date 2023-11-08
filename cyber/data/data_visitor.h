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

#ifndef CYBER_DATA_DATA_VISITOR_H_
#define CYBER_DATA_DATA_VISITOR_H_

#include <algorithm>
#include <functional>
#include <memory>
#include <vector>

#include "cyber/common/log.h"
#include "cyber/data/channel_buffer.h"
#include "cyber/data/data_dispatcher.h"
#include "cyber/data/data_visitor_base.h"
#include "cyber/data/fusion/all_latest.h"
#include "cyber/data/fusion/data_fusion.h"

namespace apollo {
namespace cyber {
namespace data {

struct VisitorConfig {
  VisitorConfig(uint64_t id, uint32_t size)
      : channel_id(id), queue_size(size) {}
  uint64_t channel_id;
  uint32_t queue_size;
};

template <typename T>
using BufferType = CacheBuffer<std::shared_ptr<T>>;

template <typename M0, typename M1 = NullType, typename M2 = NullType,
          typename M3 = NullType>
class DataVisitor : public DataVisitorBase {
 public:
  explicit DataVisitor(const std::vector<VisitorConfig>& configs)
      : buffer_m0_(configs[0].channel_id,
                   new BufferType<M0>(configs[0].queue_size)),
        buffer_m1_(configs[1].channel_id,
                   new BufferType<M1>(configs[1].queue_size)),
        buffer_m2_(configs[2].channel_id,
                   new BufferType<M2>(configs[2].queue_size)),
        buffer_m3_(configs[3].channel_id,
                   new BufferType<M3>(configs[3].queue_size)) {
    DataDispatcher<M0>::Instance()->AddBuffer(buffer_m0_);
    DataDispatcher<M1>::Instance()->AddBuffer(buffer_m1_);
    DataDispatcher<M2>::Instance()->AddBuffer(buffer_m2_);
    DataDispatcher<M3>::Instance()->AddBuffer(buffer_m3_);
    data_notifier_->AddNotifier(buffer_m0_.channel_id(), notifier_);
    data_fusion_ = new fusion::AllLatest<M0, M1, M2, M3>(
        buffer_m0_, buffer_m1_, buffer_m2_, buffer_m3_);
  }

  ~DataVisitor() {
    if (data_fusion_) {
      delete data_fusion_;
      data_fusion_ = nullptr;
    }
  }

  bool TryFetch(std::shared_ptr<M0>& m0, std::shared_ptr<M1>& m1,    // NOLINT
                std::shared_ptr<M2>& m2, std::shared_ptr<M3>& m3) {  // NOLINT
    if (data_fusion_->Fusion(&next_msg_index_, m0, m1, m2, m3)) {
      next_msg_index_++;
      return true;
    }
    return false;
  }

 private:
  fusion::DataFusion<M0, M1, M2, M3>* data_fusion_ = nullptr;
  ChannelBuffer<M0> buffer_m0_;
  ChannelBuffer<M1> buffer_m1_;
  ChannelBuffer<M2> buffer_m2_;
  ChannelBuffer<M3> buffer_m3_;
};

template <typename M0, typename M1, typename M2>
class DataVisitor<M0, M1, M2, NullType> : public DataVisitorBase {
 public:
  explicit DataVisitor(const std::vector<VisitorConfig>& configs)
      : buffer_m0_(configs[0].channel_id,
                   new BufferType<M0>(configs[0].queue_size)),
        buffer_m1_(configs[1].channel_id,
                   new BufferType<M1>(configs[1].queue_size)),
        buffer_m2_(configs[2].channel_id,
                   new BufferType<M2>(configs[2].queue_size)) {
    DataDispatcher<M0>::Instance()->AddBuffer(buffer_m0_);
    DataDispatcher<M1>::Instance()->AddBuffer(buffer_m1_);
    DataDispatcher<M2>::Instance()->AddBuffer(buffer_m2_);
    data_notifier_->AddNotifier(buffer_m0_.channel_id(), notifier_);
    data_fusion_ =
        new fusion::AllLatest<M0, M1, M2>(buffer_m0_, buffer_m1_, buffer_m2_);
  }

  ~DataVisitor() {
    if (data_fusion_) {
      delete data_fusion_;
      data_fusion_ = nullptr;
    }
  }

  bool TryFetch(std::shared_ptr<M0>& m0, std::shared_ptr<M1>& m1,  // NOLINT
                std::shared_ptr<M2>& m2) {                         // NOLINT
    if (data_fusion_->Fusion(&next_msg_index_, m0, m1, m2)) {
      next_msg_index_++;
      return true;
    }
    return false;
  }

 private:
  fusion::DataFusion<M0, M1, M2>* data_fusion_ = nullptr;
  ChannelBuffer<M0> buffer_m0_;
  ChannelBuffer<M1> buffer_m1_;
  ChannelBuffer<M2> buffer_m2_;
};

template <typename M0, typename M1>
class DataVisitor<M0, M1, NullType, NullType> : public DataVisitorBase {
 public:
  explicit DataVisitor(const std::vector<VisitorConfig>& configs)
      : buffer_m0_(configs[0].channel_id,
                   new BufferType<M0>(configs[0].queue_size)),
        buffer_m1_(configs[1].channel_id,
                   new BufferType<M1>(configs[1].queue_size)) {
    //  AddBuffer()在DataVisitor初始化时用来将这些个ChannelBuffer加入到DataDispatcher的管理中
    DataDispatcher<M0>::Instance()->AddBuffer(buffer_m0_);
    DataDispatcher<M1>::Instance()->AddBuffer(buffer_m1_);
    //  AddNotifier()用来以主channel的id为键值加入到DataNotifier的管理中
    data_notifier_->AddNotifier(buffer_m0_.channel_id(), notifier_);
    data_fusion_ = new fusion::AllLatest<M0, M1>(buffer_m0_, buffer_m1_);

    //  DataDispatcher与DataNotifier均为单例
    //  前者为模板类，意味着每一个消息类型会有对应的DataDispatcher对象，且相同消息类型会共享该对象。
    //  顾名思义，它主要用于数据传输层有数据来时的分发，即当新消息到来时通过DataDispatcher::Dispatch()函数把它放到相应的消息缓冲区中。
    //  后者用于管理所有的Notifier。它用于在消息派发完后唤醒相应的协程进行处理。
  }

  ~DataVisitor() {
    if (data_fusion_) {
      delete data_fusion_;
      data_fusion_ = nullptr;
    }
  }

  bool TryFetch(std::shared_ptr<M0>& m0, std::shared_ptr<M1>& m1) {  // NOLINT
    if (data_fusion_->Fusion(&next_msg_index_, m0, m1)) {
      next_msg_index_++;
      return true;
    }
    return false;
  }

 private:
  //  一个特殊的ChannelBuffer对象,用于存放多channel消息的组合消息（即各个channel的消息类型的tuple）。
  //  当填入主channel的消息时，会调用由SetFusionCallback()函数注册的回调。
  //  该回调判断是否所有channel都有消息，如果都有消息的话就将这些消息作为组合消息填入该组合消息的ChannelBuffer中。 
  //  在协程处理函数中会调用DataVisitor::TryFetch()函数从该ChannelBuffer中拿组合消息。
  //  值得注意的是这件事只在主channel有消息来时才会被触发，因此主channel的选取是有讲究的。
  fusion::DataFusion<M0, M1>* data_fusion_ = nullptr;
  //  对每一个channel都有一个对应的ChannelBuffer对象
  ChannelBuffer<M0> buffer_m0_;
  ChannelBuffer<M1> buffer_m1_;
};

template <typename M0>
class DataVisitor<M0, NullType, NullType, NullType> : public DataVisitorBase {
 public:
  explicit DataVisitor(const VisitorConfig& configs)
      : buffer_(configs.channel_id, new BufferType<M0>(configs.queue_size)) {
    DataDispatcher<M0>::Instance()->AddBuffer(buffer_);
    data_notifier_->AddNotifier(buffer_.channel_id(), notifier_);
  }

  DataVisitor(uint64_t channel_id, uint32_t queue_size)
      : buffer_(channel_id, new BufferType<M0>(queue_size)) {
    DataDispatcher<M0>::Instance()->AddBuffer(buffer_);
    data_notifier_->AddNotifier(buffer_.channel_id(), notifier_);
  }

  bool TryFetch(std::shared_ptr<M0>& m0) {  // NOLINT
    if (buffer_.Fetch(&next_msg_index_, m0)) {
      next_msg_index_++;
      return true;
    }
    return false;
  }

 private:
  ChannelBuffer<M0> buffer_;
};

}  // namespace data
}  // namespace cyber
}  // namespace apollo

#endif  // CYBER_DATA_DATA_VISITOR_H_
