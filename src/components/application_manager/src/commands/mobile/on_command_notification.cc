/*

 Copyright (c) 2013, Ford Motor Company
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

#include "application_manager/commands/mobile/on_command_notification.h"
#include "application_manager/application_manager_impl.h"
#include "application_manager/application_impl.h"
#include "interfaces/v4_protocol_v2_0_revT.h"

namespace application_manager {

namespace commands {

OnCommandNotification::OnCommandNotification(
  const MessageSharedPtr& message): CommandNotificationImpl(message) {
}

OnCommandNotification::~OnCommandNotification() {
}

void OnCommandNotification::Run() {
  LOG4CXX_INFO(logger_, "OnCommandNotification::Run ");

  ApplicationImpl* app =
    static_cast<ApplicationImpl*>(ApplicationManagerImpl::instance()->
                                  application((*message_)[strings::msg_params][strings::app_id].asInt()));

  if (!app) {
    LOG4CXX_ERROR_EXT(logger_, "No application associated with session key");
    return;
  }

  const unsigned int cmd_id = static_cast<unsigned int>(
                                (*message_)[strings::msg_params][strings::cmd_id].asInt());

  if (!app->FindCommand(cmd_id)) {
    LOG4CXX_ERROR_EXT(logger_,
                      " No applications found for the command " << cmd_id);
    return;
  }

  SendOnMenuCommand(app);
}

void OnCommandNotification::SendOnMenuCommand(const ApplicationImpl* app) {
  smart_objects::CSmartObject* on_menu_cmd =
    new smart_objects::CSmartObject();

  if (!on_menu_cmd) {
    LOG4CXX_ERROR_EXT(logger_, "OnCommandNotification NULL pointer");
    return;
  }

  if (!app) {
    LOG4CXX_ERROR_EXT(logger_, "OnCommandNotification NULL pointer");
    return;
  }

  const int correlation_id =
    (*message_)[strings::params][strings::correlation_id];
  const int connection_key =
    (*message_)[strings::params][strings::connection_key];

  (*on_menu_cmd)[strings::params][strings::message_type] =
    MessageType::kNotification;

  (*on_menu_cmd)[strings::msg_params][strings::app_id] =
    app->app_id();

  (*on_menu_cmd)[strings::msg_params][strings::cmd_id] =
    (*message_)[strings::msg_params][strings::cmd_id];

  (*on_menu_cmd)[strings::msg_params][strings::success] = true;
  (*on_menu_cmd)[strings::msg_params][strings::result_code] =
    NsSmartDeviceLinkRPC::V2::Result::SUCCESS;

  // msg trigger_source param is set in HMI notification

  message_.reset(on_menu_cmd);
  SendNotification();
}

}  // namespace commands

}  // namespace application_manager
