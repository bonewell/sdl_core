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

#include <algorithm>

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "mock_layer.h"
#include "mock_connected_socket.h"
#include "vr_module/interface/hmi.pb.h"
#include "vr_module/interface/mobile.pb.h"
#include "vr_module/socket_channel.h"
#include "vr_module/vr_module.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;

static const size_t kHeaderSize = 4;
static const size_t kChunkSize = 1000;

MATCHER_P(RawDataEq, message, "") {
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

MATCHER_P(SendSizeEq, message, "") {
  std::string str;
  message.SerializeToString(&str);
  size_t total = str.size() + kHeaderSize;
  return arg == kChunkSize || arg == total % kChunkSize;
}

MATCHER_P(RecvSizeEq, message, "") {
  std::string str;
  message.SerializeToString(&str);
  size_t total = str.size();
  return arg == kHeaderSize || arg == kChunkSize || arg == total % kChunkSize;
}

ACTION_P2(CallSendFake, message, sync) {
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

ACTION_P3(CallRecvFake, header, message, sync){
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

namespace vr_module {

struct Synchronizer {
  pthread_mutex_t m1 = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_t m2 = PTHREAD_MUTEX_INITIALIZER;
  void Start() {
    pthread_mutex_lock(&m1);
    pthread_mutex_lock(&m2);
  }
  void Wait() {
    pthread_mutex_lock(&m1);
    pthread_mutex_lock(&m2);
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
};

TEST_F(IntegrationTest, SupportService) {
  MockLayer layer;
  net::MockConnectedSocket* socket = new net::MockConnectedSocket();
  VRModule module(&layer, new SocketChannel(socket));
  Synchronizer sync;

  vr_hmi_api::ServiceMessage request;
  request.set_rpc(vr_hmi_api::SUPPORT_SERVICE);
  request.set_rpc_type(vr_hmi_api::REQUEST);
  request.set_correlation_id(1);
  EXPECT_CALL(*socket, send(RawDataEq(request), SendSizeEq(request), _)).Times(
      CountSendChunks(request)).WillRepeatedly(
      CallSendFake(request, &sync));

  vr_hmi_api::ServiceMessage response;
  response.set_rpc(vr_hmi_api::SUPPORT_SERVICE);
  response.set_rpc_type(vr_hmi_api::RESPONSE);
  response.set_correlation_id(1);
  vr_hmi_api::SupportServiceResponse body;
  body.set_result(vr_hmi_api::SUCCESS);
  std::string params;
  body.SerializeToString(&params);
  response.set_params(params);
  UInt8 header[kHeaderSize] = { 10, 0, 0, 0 };
  EXPECT_CALL(*socket, recv(_, RecvSizeEq(response), _)).WillRepeatedly(
      CallRecvFake(header, response, &sync));
  EXPECT_CALL(*socket, shutdown()).Times(1);
  EXPECT_CALL(*socket, close()).Times(1);

  sync.Start();
  module.Start();
  sync.Wait();
  sleep(1);
  EXPECT_TRUE(module.IsSupported());
}

}  // namespace vr_module
