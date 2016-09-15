/*
 * Copyright (c) 2016, Ford Motor Company
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided with the
 * distribution.
 *
 * Neither the name of the Ford Motor Company nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <pthread.h>
#include <time.h>

#include <algorithm>

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "mock_connected_socket.h"
#include "mock_plugin_sender.h"
#include "protocol/common.h"
#include "vr_module/interface/hmi.pb.h"
#include "vr_module/interface/mobile.pb.h"
#include "vr_module/socket_channel.h"
#include "vr_module/vr_module.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;

static const size_t kHeaderSize = 4;
static const size_t kChunkSize = 1000;

MATCHER_P(RawDataEq, message, ""){
  static int number = 0;
  std::string str;
  message.SerializeToString(&str);
  size_t total = str.size();
  size_t header = (number == 0) ? kHeaderSize : 0;
  size_t offset = kChunkSize * number;
  size_t tail = offset + kChunkSize;
  if (total > tail) {
    ++number;
  } else {
    tail = total;
    number = 0;
  }
  const UInt8* begin = reinterpret_cast<const UInt8*>(str.c_str() + offset);
  const UInt8* end = reinterpret_cast<const UInt8*>(str.c_str() + tail);
  const UInt8* output = arg + header + offset;
  return std::equal(begin, end, output);
}

MATCHER_P(SendSizeEq, message, ""){
  std::string str;
  message.SerializeToString(&str);
  size_t total = str.size() + kHeaderSize;
  return arg == kChunkSize || arg == total % kChunkSize;
}

MATCHER_P(RecvSizeEq, message, ""){
  std::string str;
  message.SerializeToString(&str);
  size_t total = str.size();
  return arg == kHeaderSize || arg == kChunkSize || arg == total % kChunkSize;
}

MATCHER_P2(RawMessageEq, service, message, ""){
  const size_t size = message.ByteSize();
  uint8_t* data = new uint8_t[size];
  message.SerializeToArray(data, size);
  uint8_t header_size = protocol_handler::PROTOCOL_HEADER_V2_SIZE;
  return arg->connection_key() == uint32_t(service)
      && arg->protocol_version() == uint32_t(protocol_handler::PROTOCOL_VERSION_2)
      && arg->service_type() == uint32_t(protocol_handler::ServiceType::kVr)
      && arg->data_size() == header_size + size
      && std::equal(data, data + size, arg->data() + header_size);
}

ACTION_P2(CallSendRequest, message, sync){
  static int count = 0;
  std::string str;
  message.SerializeToString(&str);
  size_t total = str.size() + kHeaderSize;
  size_t offset = kChunkSize * (count + 1);
  if (total > offset) {
    ++count;
    return kChunkSize;
  } else {
    size_t size = total - kChunkSize * count;
    sync->Resume();
    return size;
  }
}

ACTION_P(CallSendNotification, message){
  static int count = 0;
  std::string str;
  message.SerializeToString(&str);
  size_t total = str.size() + kHeaderSize;
  size_t offset = kChunkSize * (count + 1);
  if (total > offset) {
    ++count;
    return kChunkSize;
  } else {
    size_t size = total - kChunkSize * count;
    return size;
  }
}

ACTION_P3(CallRecvResponse, header, message, sync){
  static int recv_count = 0;
  if (recv_count == 0) {
    sync->Pause();
  }
  UInt8* arg = const_cast<UInt8*>(arg0);
  std::string str;
  message.SerializeToString(&str);
  if (recv_count == -1) {
    const UInt8* begin = reinterpret_cast<const UInt8*>(str.c_str());
    const UInt8* end = reinterpret_cast<const UInt8*>(str.c_str());
    std::copy(begin, end, arg);
    return 0;
  }
  if (recv_count == 0) {
    ++recv_count;
    const UInt8* begin = header;
    const UInt8* end = header + kHeaderSize;
    std::copy(begin, end, arg);
    return kHeaderSize;
  }
  size_t total = str.size();
  size_t offset = kChunkSize * (recv_count - 1);
  if (total > offset + kChunkSize) {
    ++recv_count;
    const UInt8* begin = reinterpret_cast<const UInt8*>(str.c_str() + offset);
    const UInt8* end = reinterpret_cast<const UInt8*>(str.c_str() + offset
        + kChunkSize);
    std::copy(begin, end, arg);
    return kChunkSize;
  } else {
    size_t size = total - offset;
    recv_count = -1;
    const UInt8* begin = reinterpret_cast<const UInt8*>(str.c_str() + offset);
    const UInt8* end = reinterpret_cast<const UInt8*>(str.c_str() + offset
        + size);
    std::copy(begin, end, arg);
    sync->Stop();
    return size;
  }
}

ACTION_P2(CallRecvRequest, header, message){
  static int recv_count = 0;
  UInt8* arg = const_cast<UInt8*>(arg0);
  std::string str;
  message.SerializeToString(&str);
  if (recv_count == -1) {
    const UInt8* begin = reinterpret_cast<const UInt8*>(str.c_str());
    const UInt8* end = reinterpret_cast<const UInt8*>(str.c_str());
    std::copy(begin, end, arg);
    return 0;
  }
  if (recv_count == 0) {
    ++recv_count;
    const UInt8* begin = header;
    const UInt8* end = header + kHeaderSize;
    std::copy(begin, end, arg);
    return kHeaderSize;
  }
  size_t total = str.size();
  size_t offset = kChunkSize * (recv_count - 1);
  if (total > offset + kChunkSize) {
    ++recv_count;
    const UInt8* begin = reinterpret_cast<const UInt8*>(str.c_str() + offset);
    const UInt8* end = reinterpret_cast<const UInt8*>(str.c_str() + offset
        + kChunkSize);
    std::copy(begin, end, arg);
    return kChunkSize;
  } else {
    size_t size = total - offset;
    recv_count = -1;
    const UInt8* begin = reinterpret_cast<const UInt8*>(str.c_str() + offset);
    const UInt8* end = reinterpret_cast<const UInt8*>(str.c_str() + offset
        + size);
    std::copy(begin, end, arg);
    return size;
  }
}

namespace vr_module {

struct Synchronizer {
  pthread_mutex_t m1 = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_t m2 = PTHREAD_MUTEX_INITIALIZER;
  void Start() {
    pthread_mutex_lock(&m1);
    pthread_mutex_lock(&m2);
  }
  void Wait(int seconds) {
    timespec timestamp;
    clock_gettime(CLOCK_REALTIME, &timestamp);
    timestamp.tv_sec += seconds;
    pthread_mutex_timedlock(&m1, &timestamp);
    pthread_mutex_timedlock(&m2, &timestamp);
    pthread_mutex_unlock(&m2);
    pthread_mutex_unlock(&m1);
  }
  void Stop() {
    pthread_mutex_unlock(&m1);
    pthread_mutex_unlock(&m2);
  }
  void Pause() {
    pthread_mutex_lock(&m2);
  }
  void Resume() {
    pthread_mutex_unlock(&m2);
  }
};

class IntegrationTest : public ::testing::Test {
 protected:
  int CountSendChunks(const vr_hmi_api::ServiceMessage& message) {
    std::string str;
    message.SerializeToString(&str);
    size_t total = str.size() + kHeaderSize;
    size_t n = (total / kChunkSize) + (total % kChunkSize ? 1 : 0);
    return n;
  }
  int CountRecvChunks(const vr_hmi_api::ServiceMessage& message) {
    std::string str;
    message.SerializeToString(&str);
    size_t total = str.size();
    size_t n = (total / kChunkSize) + (total % kChunkSize ? 1 : 0);
    return n + 1;
  }
};

TEST_F(IntegrationTest, SupportService) {
  MockPluginSender sender;
  net::MockConnectedSocket* socket = new net::MockConnectedSocket();
  VRModule module(&sender, new SocketChannel(socket));
  Synchronizer sync;

  vr_hmi_api::ServiceMessage support_out;
  support_out.set_rpc(vr_hmi_api::SUPPORT_SERVICE);
  support_out.set_rpc_type(vr_hmi_api::REQUEST);
  support_out.set_correlation_id(1);
  EXPECT_CALL(*socket, send(RawDataEq(support_out), SendSizeEq(support_out), _))
      .Times(CountSendChunks(support_out)).WillRepeatedly(
      CallSendRequest(support_out, &sync));

  vr_hmi_api::ServiceMessage support_in;
  support_in.set_rpc(vr_hmi_api::SUPPORT_SERVICE);
  support_in.set_rpc_type(vr_hmi_api::RESPONSE);
  support_in.set_correlation_id(1);
  vr_hmi_api::SupportServiceResponse body;
  body.set_result(vr_hmi_api::SUCCESS);
  std::string params;
  body.SerializeToString(&params);
  support_in.set_params(params);
  UInt8 header[kHeaderSize] = { 10, 0, 0, 0 };
  EXPECT_CALL(*socket, recv(_, RecvSizeEq(support_in), _)).WillRepeatedly(
      CallRecvResponse(header, support_in, &sync));
  EXPECT_CALL(*socket, shutdown()).Times(1);
  EXPECT_CALL(*socket, close()).Times(1);

  sync.Start();
  module.Start();
  module.CheckSupport();
  sync.Wait(10);
  sleep(1);
  EXPECT_TRUE(module.IsSupported());
}

TEST_F(IntegrationTest, OnRegisterService) {
  MockPluginSender sender;
  net::MockConnectedSocket* socket = new net::MockConnectedSocket();
  VRModule module(&sender, new SocketChannel(socket));

  vr_hmi_api::ServiceMessage on_register_out;
  on_register_out.set_rpc(vr_hmi_api::ON_REGISTER);
  on_register_out.set_rpc_type(vr_hmi_api::NOTIFICATION);
  on_register_out.set_correlation_id(1);
  vr_hmi_api::OnRegisterServiceNotification notification;
  notification.set_appid(1);
  notification.set_default_(false);
  std::string params;
  notification.SerializeToString(&params);
  on_register_out.set_params(params);
  EXPECT_CALL(*socket, send(RawDataEq(on_register_out),
          SendSizeEq(on_register_out), _)).WillRepeatedly(
      CallSendNotification(on_register_out));
  EXPECT_CALL(*socket, close()).Times(1);

  module.supported_ = true;
  module.OnServiceStartedCallback(1);
}

TEST_F(IntegrationTest, ActivateService) {
  MockPluginSender sender;
  net::MockConnectedSocket* socket = new net::MockConnectedSocket();
  VRModule module(&sender, new SocketChannel(socket));
  Synchronizer sync;

  vr_hmi_api::ServiceMessage activate_in;
  activate_in.set_rpc(vr_hmi_api::ACTIVATE);
  activate_in.set_rpc_type(vr_hmi_api::REQUEST);
  activate_in.set_correlation_id(10);
  vr_hmi_api::ActivateServiceRequest request;
  request.set_appid(1);
  std::string params;
  request.SerializeToString(&params);
  activate_in.set_params(params);
  UInt8 header[kHeaderSize] = { 10, 0, 0, 0 };
  EXPECT_CALL(*socket, recv(_, RecvSizeEq(activate_in), _)).WillRepeatedly(
      CallRecvRequest(header, activate_in));

  vr_mobile_api::ServiceMessage activate_to;
  activate_to.set_rpc(vr_mobile_api::ACTIVATE);
  activate_to.set_rpc_type(vr_mobile_api::REQUEST);
  activate_to.set_correlation_id(1);
  EXPECT_CALL(sender,
      SendMessageToRemoteMobileService(RawMessageEq(1, activate_to))).Times(1);
  EXPECT_CALL(*socket, shutdown()).Times(1);
  EXPECT_CALL(*socket, close()).Times(1);

  sync.Start();
  module.supported_ = true;
  module.Start();
  sync.Wait(10);
}

}  // namespace vr_module
