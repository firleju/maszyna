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

	int& ZMQFrame::operator=(int32_t i)
	{
		m_data.clear();
		m_messageSize = sizeof(i);
		m_data.resize(m_messageSize);
		sn_utils::ls_int32(m_data, i);
		return i;
	}

	float & ZMQFrame::operator=(float f)
	{
		m_data.clear();
		m_messageSize = sizeof(f);
		m_data.resize(m_messageSize);
		sn_utils::ls_float32(m_data, f);
		return f;
	}

	std::string & ZMQFrame::operator=(std::string s)
	{
		m_data = std::vector<uint8_t>(s.begin(), s.end());
		m_messageSize = m_data.size();
		return s;
	}

	ZMQConnection::ZMQConnection()
	{
		//net_context = CpperoMQ::Context();
		//net_socket = net_context.createDealerSocket();
		if (Global.network_conf.identity != "auto")
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

	auto ZMQConnection::getIncomingQueue() -> std::list<multiplayer::network_queue_t>&
	{
		return m_incoming_queue;
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

	void SendVersionInfo()
	{
		auto msg = ZMQMessage();
		msg.AddFrame(network_codes::net_proto_version);
		msg.AddFrame(1);
		msg.AddFrame(Global.network->protocol_version);
		Global.network_queue.push_back(msg);
	}

	void SendScenery()
	{
		auto msg = ZMQMessage();
		msg.AddFrame(network_codes::scenery_name);
		msg.AddFrame(1);
		msg.AddFrame(Global.SceneryFile);
		Global.network_queue.push_back(msg);
	}

	void SendEventCallConfirmation(int status, std::string name)
	{
		auto msg = ZMQMessage();
		msg.AddFrame(network_codes::event_call);
		msg.AddFrame(1);
		msg.AddFrame(status);
		msg.AddFrame(name);
		Global.network_queue.push_back(msg);
	}

	void SendAiCommandConfirmation(int status, std::string vehicle, std::string command)
	{
		auto msg = ZMQMessage();
		msg.AddFrame(network_codes::ai_command);
		msg.AddFrame(1);
		msg.AddFrame(vehicle);
		msg.AddFrame(command);
		Global.network_queue.push_back(msg);
	}

	void SendTrackOccupancy(std::string name)
	{
		auto msg = ZMQMessage();
		msg.AddFrame(network_codes::track_occupancy);
		msg.AddFrame(1);
		if ("*" == name)
		{
			msg.AddFrame(0); //wielkoœæ
			int i = 0;
			for (auto *path : simulation::Paths.sequence()) {
				if (false == path->name().empty()) // musi byæ nazwa
				{
					i++;
					msg.AddFrame(path->name());
					msg.AddFrame((int)path->IsEmpty());
					msg.AddFrame(path->iDamageFlag);
				}
			}
			msg[2] = i; //podajemy liczbê torów
		}
		else
		{
			msg.AddFrame(1);
			msg.AddFrame(name);
			auto *track = simulation::Paths.find(name);
			if (track != nullptr) {
				msg.AddFrame((int)track->IsEmpty());
				msg.AddFrame(track->iDamageFlag);
			}
			else
			{
				msg.AddFrame(0);
				msg.AddFrame(0);
			}

		}
		Global.network_queue.push_back(msg);
	}

	void SendIsolatedOccupancy(std::string name)
	{
		TIsolated *Current;
		auto msg = ZMQMessage();
		msg.AddFrame(network_codes::isolated_occupancy);
		msg.AddFrame(1);
		if ("*" == name)
		{
			int i = 0;
			msg.AddFrame(0); // narazie zero odcinków
			for (Current = TIsolated::Root(); Current; Current = Current->Next(), i++) {
				msg.AddFrame(Current->asName);
				msg.AddFrame((int)Current->Busy());
			}
			msg[2] = i;
		}
		else
		{
			msg.AddFrame(1);
			msg.AddFrame(name);
			for (Current = TIsolated::Root(); Current; Current = Current->Next()) {
				if (Current->asName == name) {
					msg.AddFrame((int)Current->Busy());
					// nie sprawdzaj dalszych
					Global.network_queue.push_back(msg);

					return;
				}
			}
			// jesli nie znalaz³ to wysy³amy wolny
			msg.AddFrame(0);
		}
		Global.network_queue.push_back(msg);
	}

	void SendSimulationStatus(int which_param)
	{
		auto msg = ZMQMessage();
		msg.AddFrame(network_codes::param_set);
		msg.AddFrame(which_param);
		switch (which_param)
		{
		case 1: //pauza
			msg.AddFrame(Global.iPause);
			break;
		case 2: //godzina
			msg.AddFrame(float(Global.fTimeAngleDeg / 360.0));
		default:
			break;
		}
		Global.network_queue.push_back(msg);
	}

}