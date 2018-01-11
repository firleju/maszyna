// The MIT License (MIT)
//
// Copyright (c) 2015 Jason Shipman
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#pragma once

#include <CpperoMQ/Common.hpp>
#include <CpperoMQ/Context.hpp>
#include <CpperoMQ/DealerSocket.hpp>
#include <CpperoMQ/Error.hpp>
#include <CpperoMQ/ExtendedPublishSocket.hpp>
#include <CpperoMQ/ExtendedSubscribeSocket.hpp>
#include <CpperoMQ/IncomingMessage.hpp>
#include <CpperoMQ/Message.hpp>
#include <CpperoMQ/OutgoingMessage.hpp>
#include <CpperoMQ/Poller.hpp>
#include <CpperoMQ/PollItem.hpp>
#include <CpperoMQ/Proxy.hpp>
#include <CpperoMQ/PublishSocket.hpp>
#include <CpperoMQ/PullSocket.hpp>
#include <CpperoMQ/PushSocket.hpp>
#include <CpperoMQ/Receivable.hpp>
#include <CpperoMQ/ReplySocket.hpp>
#include <CpperoMQ/RequestSocket.hpp>
#include <CpperoMQ/RouterSocket.hpp>
#include <CpperoMQ/Sendable.hpp>
#include <CpperoMQ/Socket.hpp>
#include <CpperoMQ/SubscribeSocket.hpp>
#include <CpperoMQ/Version.hpp>
#include <CpperoMQ/Mixins/ConflatingSocket.hpp>
#include <CpperoMQ/Mixins/IdentifyingSocket.hpp>
#include <CpperoMQ/Mixins/ReceivingSocket.hpp>
#include <CpperoMQ/Mixins/RequestingSocket.hpp>
#include <CpperoMQ/Mixins/RouterProbingSocket.hpp>
#include <CpperoMQ/Mixins/RoutingSocket.hpp>
#include <CpperoMQ/Mixins/SendingSocket.hpp>
#include <CpperoMQ/Mixins/SocketTypeWrapper.hpp>
#include <CpperoMQ/Mixins/SubscribingSocket.hpp>
