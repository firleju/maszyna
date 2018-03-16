/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#include "stdafx.h"
#include "network.h"
#include "Globals.h"
#include "simulation.h"
#include "mtable.h"
#include "Logs.h"
#include "sn_utils.h"

namespace multiplayer {

	ZMQMessage::ZMQMessage()
	{

	}

	bool ZMQMessage::send(const CpperoMQ::Socket & socket, const bool moreToSend) const
	{
		using namespace CpperoMQ;
		auto s = m_frames.size();
		for (int i = 0; i < s; i++)
		{
			auto msg = OutgoingMessage(m_frames[i].size(), m_frames[i].ToByteArray());
			if (!msg.send(socket, i != s - 1 ? true : moreToSend))
				return false;
		}

		return true;
	}

	bool ZMQMessage::receive(CpperoMQ::Socket& socket, bool& moreToReceive)
	{
		using namespace CpperoMQ;
		m_frames.clear();
		moreToReceive = true;
		while (moreToReceive)
		{
			IncomingMessage msg;
			if (!msg.receive(socket, moreToReceive))
				return false;
			m_frames.emplace_back(msg.charData(), msg.size());
		}
		return true;
	}

	ZMQFrame::ZMQFrame(int32_t n)
	{
		m_messageSize = sizeof(n);
		m_data.resize(m_messageSize);
		sn_utils::ls_int32(m_data, n);
	}

	ZMQFrame::ZMQFrame(float n)
	{
		m_messageSize = sizeof(n);
		m_data.resize(m_messageSize);
		sn_utils::ls_float32(m_data, n);
	}

	std::string ZMQFrame::ToString()
	{
		return std::string(reinterpret_cast<char*>(m_data.data()), m_data.size());
	}

	int32_t ZMQFrame::ToInt()
	{
		return sn_utils::ld_int32(m_data);
	}

	float ZMQFrame::ToFloat()
	{
		return sn_utils::ld_float32(m_data);
	}

	const uint8_t* ZMQFrame::ToByteArray() const
	{
		return m_data.data();
	}

	ZMQConnection::ZMQConnection()
	{
		//net_context = CpperoMQ::Context();
		//net_socket = net_context.createDealerSocket();
		m_socket.setIdentity(Global.network_conf.identity.c_str());
		m_socket.connect(std::string("tcp://" + Global.network_conf.address + ":" + Global.network_conf.port).c_str());
		m_poller = CpperoMQ::Poller(0);

	}

	void ZMQConnection::poll()
	{
		m_poller.poll(net_pollReceiver);
	}

	void ZMQConnection::send()
	{
	
		for (auto &m : Global.network_queue)
		{
			if (m.checkTime())
				m_socket.send(m.message);
		}

	}

	bool network_queue_t::checkTime()
	{
		{
			auto now = std::chrono::high_resolution_clock::now();
			if (std::chrono::duration_cast<std::chrono::seconds>(now - time).count() >= 5.0f)
			{
				time = now;
				return true;
			}
			else
				return false;
		}
	}

}