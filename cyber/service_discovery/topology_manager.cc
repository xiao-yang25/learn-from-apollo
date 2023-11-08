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

#include "cyber/service_discovery/topology_manager.h"

#include "cyber/common/global_data.h"
#include "cyber/common/log.h"
#include "cyber/time/time.h"

namespace apollo {
namespace cyber {
namespace service_discovery {

TopologyManager::TopologyManager()
    : init_(false),
      node_manager_(nullptr),
      channel_manager_(nullptr),
      service_manager_(nullptr),
      participant_(nullptr),
      participant_listener_(nullptr) {
  Init();
}

TopologyManager::~TopologyManager() { Shutdown(); }

void TopologyManager::Shutdown() {
  ADEBUG << "topology shutdown.";
  // avoid shutdown twice
  if (!init_.exchange(false)) {
    return;
  }

  node_manager_->Shutdown();
  channel_manager_->Shutdown();
  service_manager_->Shutdown();
  participant_->Shutdown();

  delete participant_listener_;
  participant_listener_ = nullptr;

  change_signal_.DisconnectAllSlots();
}

TopologyManager::ChangeConnection TopologyManager::AddChangeListener(
    const ChangeFunc& func) {
  return change_signal_.Connect(func);
}

void TopologyManager::RemoveChangeListener(const ChangeConnection& conn) {
  auto local_conn = conn;
  local_conn.Disconnect();
}

//  第二种是基于主动式的拓扑变更广播
bool TopologyManager::Init() {
  if (init_.exchange(true)) {
    return true;
  }

  node_manager_ = std::make_shared<NodeManager>();
  channel_manager_ = std::make_shared<ChannelManager>();
  service_manager_ = std::make_shared<ServiceManager>();

  CreateParticipant();
  //  调用相应的初始化函数
  bool result =
      InitNodeManager() && InitChannelManager() && InitServiceManager();
  if (!result) {
    AERROR << "init manager failed.";
    participant_ = nullptr;
    delete participant_listener_;
    participant_listener_ = nullptr;
    node_manager_ = nullptr;
    channel_manager_ = nullptr;
    service_manager_ = nullptr;
    init_.store(false);
    return false;
  }

  return true;
}
//  在初始化时，会调用它们的StartDiscovery()函数开始启动自动发现机制。
//  接着通过StartDiscovery()中的CreateSubscriber()和CreatePublisher()函数创建相应的subscriber和publisher。
//  这层拓扑监控是主动式的，即需要相应的地方主动调用Join()或Leave()来触发，然后各子管理器中回调函数进行信息的更新。
//  如NodeChannelImpl创建时会调用NodeManager::Join()。
//  Reader和Writer初始化时会调用JoinTheTopolicy()函数，继而调用ChannelManager::Join()函数。
//  相应地，有LeaveTheTopology()函数表示退出拓扑网络。在这两个函数中，会调用Dispose()函数，而这个函数是虚函数，在各子管理器中有各自的实现。
//  另外Manager提供AddChangeListener()函数注册当拓扑发生变化时的回调函数。
//  举例来说，Reader::JoinTheTopology()函数中会通过该函数注册回调Reader::OnChannelChange()。
bool TopologyManager::InitNodeManager() {
  return node_manager_->StartDiscovery(participant_->fastrtps_participant());
}

bool TopologyManager::InitChannelManager() {
  return channel_manager_->StartDiscovery(participant_->fastrtps_participant());
}

bool TopologyManager::InitServiceManager() {
  return service_manager_->StartDiscovery(participant_->fastrtps_participant());
}

//  Cyber RT中有两个层面的拓扑变化的监控
//  第一种是基于Fast RTPS的发现机制
bool TopologyManager::CreateParticipant() {
  std::string participant_name =
      common::GlobalData::Instance()->HostName() + '+' +
      std::to_string(common::GlobalData::Instance()->ProcessId());
  //  它主要监视网络中是否有参与者加入或退出。
  //  TopologyManager::CreateParticipant()函数创建transport::Participant对象时会输入包含host name与process id的名称。
  //  ParticipantListener用于监听网络的变化。
  //  网络拓扑发生变化时，Fast RTPS传上来ParticipantDiscoveryInfo，
  //  在TopologyManager::Convert()函数中对该信息转换成Cyber RT中的数据结构ChangeMsg。
  //  然后调用回调函数TopologyManager::OnParticipantChange()，
  //  它会调用其它几个子管理器的OnTopoModuleLeave()函数。
  //  然后子管理器中便可以将相应维护的信息进行更新（如NodeManager中将相应的节点删除）。
  participant_listener_ = new ParticipantListener(std::bind(
      &TopologyManager::OnParticipantChange, this, std::placeholders::_1));
  participant_ = std::make_shared<transport::Participant>(
      participant_name, 11511, participant_listener_);
  return true;
}

void TopologyManager::OnParticipantChange(const PartInfo& info) {
  ChangeMsg msg;
  if (!Convert(info, &msg)) {
    return;
  }

  if (!init_.load()) {
    return;
  }

  if (msg.operate_type() == OperateType::OPT_LEAVE) {
    auto& host_name = msg.role_attr().host_name();
    int process_id = msg.role_attr().process_id();
    node_manager_->OnTopoModuleLeave(host_name, process_id);
    channel_manager_->OnTopoModuleLeave(host_name, process_id);
    service_manager_->OnTopoModuleLeave(host_name, process_id);
  }
  change_signal_(msg);
}

bool TopologyManager::Convert(const PartInfo& info, ChangeMsg* msg) {
  auto guid = info.rtps.m_guid;
  auto status = info.rtps.m_status;
  std::string participant_name("");
  OperateType opt_type = OperateType::OPT_JOIN;

  switch (status) {
    case eprosima::fastrtps::rtps::DISCOVERY_STATUS::DISCOVERED_RTPSPARTICIPANT:
      participant_name = info.rtps.m_RTPSParticipantName;
      participant_names_[guid] = participant_name;
      opt_type = OperateType::OPT_JOIN;
      break;

    case eprosima::fastrtps::rtps::DISCOVERY_STATUS::REMOVED_RTPSPARTICIPANT:
    case eprosima::fastrtps::rtps::DISCOVERY_STATUS::DROPPED_RTPSPARTICIPANT:
      if (participant_names_.find(guid) != participant_names_.end()) {
        participant_name = participant_names_[guid];
        participant_names_.erase(guid);
      }
      opt_type = OperateType::OPT_LEAVE;
      break;

    default:
      break;
  }

  std::string host_name("");
  int process_id = 0;
  if (!ParseParticipantName(participant_name, &host_name, &process_id)) {
    return false;
  }

  msg->set_timestamp(cyber::Time::Now().ToNanosecond());
  msg->set_change_type(ChangeType::CHANGE_PARTICIPANT);
  msg->set_operate_type(opt_type);
  msg->set_role_type(RoleType::ROLE_PARTICIPANT);
  auto role_attr = msg->mutable_role_attr();
  role_attr->set_host_name(host_name);
  role_attr->set_process_id(process_id);
  return true;
}

bool TopologyManager::ParseParticipantName(const std::string& participant_name,
                                           std::string* host_name,
                                           int* process_id) {
  // participant_name format: host_name+process_id
  auto pos = participant_name.find('+');
  if (pos == std::string::npos) {
    ADEBUG << "participant_name [" << participant_name << "] format mismatch.";
    return false;
  }
  *host_name = participant_name.substr(0, pos);
  std::string pid_str = participant_name.substr(pos + 1);
  try {
    *process_id = std::stoi(pid_str);
  } catch (const std::exception& e) {
    AERROR << "invalid process_id:" << e.what();
    return false;
  }
  return true;
}

}  // namespace service_discovery
}  // namespace cyber
}  // namespace apollo
