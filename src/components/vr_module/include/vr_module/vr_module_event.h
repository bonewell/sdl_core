/*
 Copyright (c) 2016, Ford Motor Company
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.

 Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following
 disclaimer in the documentation and/or other materials provided with the
 distribution.

 Neither the name of the Ford Motor Company nor the names of its contributors
 may be used to endorse or promote products derived from this software
 without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SRC_COMPONENTS_VR_MODULE_INCLUDE_VR_MODULE_VR_MODULE_EVENT_H_
#define SRC_COMPONENTS_VR_MODULE_INCLUDE_VR_MODULE_VR_MODULE_EVENT_H_

#include <string>

#include "application_manager/message.h"  // TODO(VS): Will be deleted when MessagePtr will be replaced with gpb generated class

#include "vr_module/event_engine/event.h"
#include "vr_module/interface/hmi.pb.h"

#include "functional_module/function_ids.h"

namespace vr_module {

class VRModuleEvent :
    public event_engine::Event<vr_hmi_api::ServiceMessage,
    vr_hmi_api::RPCName> {
 public:
  /**
   * @brief Constructor with parameters
   *
   * @param message GPB
   */
  explicit VRModuleEvent(const vr_hmi_api::ServiceMessage& message);

  /**
   * @brief Destructor
   */
  virtual ~VRModuleEvent();

  /*
   * @brief Retrieves event message request ID
   */
  virtual int32_t event_message_function_id() const;

  /*
   * @brief Retrieves event message correlation ID
   */
  virtual int32_t event_message_correlation_id() const;

  /*
   * @brief Retrieves event message response type
   */
  virtual event_engine::MessageType event_message_type() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(VRModuleEvent);
};

}  // namespace vr_module

#endif  // SRC_COMPONENTS_VR_MODULE_INCLUDE_VR_MODULE_VR_MODULE_EVENT_H_
