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
#include "simulationtime.h"
#include "event.h"
#include "DynObj.h"
#include "Driver.h"
#include "mtable.h"
#include "Logs.h"
#include "sn_utils.h"

namespace multiplayer
{

ZMQMessage::ZMQMessage() {}

bool ZMQMessage::send(const CpperoMQ::Socket &socket, const bool moreToSend) const
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

bool ZMQMessage::receive(CpperoMQ::Socket &socket, bool &moreToReceive)
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
	sn_utils::bs_int32(m_data, n);
}

ZMQFrame::ZMQFrame(float n)
{
	m_messageSize = sizeof(n);
	m_data.resize(m_messageSize);
	sn_utils::bs_float32(m_data, n);
}

std::string ZMQFrame::ToString()
{
	return std::string(reinterpret_cast<char *>(m_data.data()), m_data.size());
}

int32_t ZMQFrame::ToInt()
{
	return sn_utils::bd_int32(m_data);
}

float ZMQFrame::ToFloat()
{
	return sn_utils::bd_float32(m_data);
}

const uint8_t *ZMQFrame::ToByteArray() const
{
	return m_data.data();
}

int &ZMQFrame::operator=(int32_t i)
{
	m_data.clear();
	m_messageSize = sizeof(i);
	m_data.resize(m_messageSize);
	sn_utils::bs_int32(m_data, i);
	return i;
}

float &ZMQFrame::operator=(float f)
{
	m_data.clear();
	m_messageSize = sizeof(f);
	m_data.resize(m_messageSize);
	sn_utils::bs_float32(m_data, f);
	return f;
}

std::string &ZMQFrame::operator=(std::string s)
{
	m_data = std::vector<uint8_t>(s.begin(), s.end());
	m_messageSize = m_data.size();
	return s;
}

ZMQConnection::ZMQConnection()
{
	// net_context = CpperoMQ::Context();
	// net_socket = net_context.createDealerSocket();
	if (Global.network_conf.identity != "auto")
		m_socket.setIdentity(Global.network_conf.identity.c_str());
	else
	{
		double b = Random(std::numeric_limits<double>::max());
		m_socket.setIdentity(reinterpret_cast<char *>(&b));
	}

	m_socket.connect(std::string("tcp://" + Global.network_conf.address + ":" + Global.network_conf.port).c_str());
	m_poller = CpperoMQ::Poller(0);
}

void ZMQConnection::poll()
{
	m_poller.poll(net_pollReceiver);
}

void ZMQConnection::send()
{
	auto it = Global.network_queue.begin();
	while (it != Global.network_queue.end())
	{
		m_socket.send(it->message);
		it = Global.network_queue.erase(it);
	}
}

auto ZMQConnection::getIncomingQueue() -> std::list<multiplayer::network_queue_t> &
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

void SendHandshakeInfo(bool change)
{
	// Handshake
	// 1 - typ wiadomoœci
	// 2 - typ clienta - symulator - zawsze 1
	// 3 - inicjacja (0) czy zmiana (1)
	// 4 - sceneria
	// 5 - pojazd
	auto msg = ZMQMessage();
	msg.AddFrame(network_codes::handshake_info);
	msg.AddFrame(1);
	msg.AddFrame(change);
	msg.AddFrame(Global.SceneryFile);
	msg.AddFrame(Global.asHumanCtrlVehicle);
	Global.network_queue.push_back(msg);
}

void SendPing()
{
	// Ping
	// 1 - typ wiadomoœci
	// 2 - typ klienta
	auto msg = ZMQMessage();
	msg.AddFrame(network_codes::ping);
	msg.AddFrame(1);
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
	//1 - typ wiadomoœci
	//2 - symulator
	//3 - liczba torów
	//4 - nazwa
	//5 - zajêtoœæ
	//6 - flaga uszkodzeñ
	// 4,5,6 powtarza siê tyle razy ile jest wartoœæ w polu 3
	auto msg = ZMQMessage();
	msg.AddFrame(network_codes::track_occupancy);
	msg.AddFrame(1);
	if ("*" == name)
	{
		msg.AddFrame(0); // wielkoœæ
		int i = 0;
		for (auto *path : simulation::Paths.sequence())
		{
			if (false == path->name().empty()) // musi byæ nazwa
			{
				i++;
				msg.AddFrame(path->name());
				msg.AddFrame((int)path->IsEmpty());
				msg.AddFrame(path->iDamageFlag);
			}
		}
		msg[2] = i; // podajemy liczbê torów
	}
	else
	{
		msg.AddFrame(1);
		msg.AddFrame(name);
		auto *track = simulation::Paths.find(name);
		if (track != nullptr)
		{
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

void SendTrackOccupancy(std::string name, bool occupied, int damage_flag)
{
	auto msg = ZMQMessage();
	msg.AddFrame(network_codes::track_occupancy);
	msg.AddFrame(1);
	msg.AddFrame(1);
	msg.AddFrame(name);
	msg.AddFrame((int)occupied);
	msg.AddFrame(damage_flag);
	Global.network_queue.push_back(msg);
}

// void SendIsolatedOccupancy(std::string name)
//{
//	TIsolated *Current;
//	auto msg = ZMQMessage();
//	msg.AddFrame(network_codes::isolated_occupancy);
//	msg.AddFrame(1);
//	if ("*" == name)
//	{
//		int i = 0;
//		msg.AddFrame(0); // narazie zero odcinków
//		for (Current = TIsolated::Root(); Current; Current = Current->Next(), i++) {
//			msg.AddFrame(Current->asName);
//			msg.AddFrame((int)Current->Busy());
//		}
//		msg[2] = i;
//	}
//	else
//	{
//		msg.AddFrame(1);
//		msg.AddFrame(name);
//		for (Current = TIsolated::Root(); Current; Current = Current->Next()) {
//			if (Current->asName == name) {
//				msg.AddFrame((int)Current->Busy());
//				// nie sprawdzaj dalszych
//				Global.network_queue.push_back(msg);

//				return;
//			}
//		}
//		// jesli nie znalaz³ to wysy³amy wolny
//		msg.AddFrame(0);
//	}
//	Global.network_queue.push_back(msg);
//}

// void SendIsolatedOccupancy(std::string name, bool occupied)
//{
//	auto msg = ZMQMessage();
//	msg.AddFrame(network_codes::isolated_occupancy);
//	msg.AddFrame(1);
//	msg.AddFrame(1);
//	msg.AddFrame(name);
//	msg.AddFrame((int)occupied);
//	Global.network_queue.push_back(msg);
//}

void SendSimulationStatus(int which_param)
{
	auto msg = ZMQMessage();
	msg.AddFrame(network_codes::param_set);
	msg.AddFrame(which_param);
	switch (which_param)
	{
	case 1: // pauza
		msg.AddFrame(Global.iPause);
		break;
	case 2: // godzina
		msg.AddFrame(float(Global.fTimeAngleDeg / 360.0));
	default:
		break;
	}
	Global.network_queue.push_back(msg);
}

void HandleMessage(std::list<multiplayer::network_queue_t> &incoming_queue)
{
	for (auto &m : incoming_queue)
	{
		// check if message has min 2 frames: order code, order type code
		if (m.message.size() < 2)
			return;
		switch (m.message[0].ToInt())
		{
		case network_codes::handshake_info:
		{
			// handshake
			SendHandshakeInfo();
			break;
		}
		case network_codes::ping:
		{
			// ping
			SendPing();
		}
		break;
		case network_codes::event_call:
		{
			// wyw³oanie eventu
			switch (m.message[1].ToInt())
			{
			case 0:
			{
				CommLog(Now() + " " + m.message[0].ToString() + " Remote call of event" + " rcvd");
				if (m.message.size() != 3)
				{
					CommLog(Now() + " Wrong number of frames");
					return;
				}
				auto *event = simulation::Events.FindEvent(m.message[2].ToString());
				if (event != nullptr)
				{
					if ((typeid(*event) == typeid(multi_event)) || (typeid(*event) == typeid(lights_event)) || (event->m_sibling != 0))
					{
						// tylko jawne albo niejawne Multiple
						simulation::Events.AddToQuery(event, nullptr); // drugi parametr to dynamic wywo³uj¹cy - tu brak
					}
				}
				else
					multiplayer::SendEventCallConfirmation(0, m.message[2].ToString()); // b³¹d

				break;
			}
			case 1:
			{
				CommLog(Now() + " Client do not handle event launch confirmation messages");
				break;
			}
			default:
				CommLog(Now() + " Wrong code type");
			}
			break;
		}
		case network_codes::ai_command: // rozkaz dla AI
			switch (m.message[1].ToInt())
			{
			case 0:
			{
				CommLog(Now() + " " + m.message[0].ToString() + " Remote command for vehicle" + " rcvd");
				if (m.message.size() != 6)
				{
					CommLog(Now() + " Wrong number of frames");
					return;
				}
				CommLog(Now() + " Vehicle: " + m.message[2].ToString() + " Command: " + m.message[3].ToString() + " rcvd");
				// nazwa pojazdu jest druga
				auto *vehicle = simulation::Vehicles.find(m.message[2].ToString());
				if ((vehicle != nullptr) && (vehicle->Mechanik != nullptr))
				{
					vehicle->Mechanik->PutCommand(m.message[3].ToString(), m.message[4].ToFloat(), m.message[5].ToFloat(), nullptr, stopExt);
					WriteLog("AI command: " + m.message[3].ToString());
					multiplayer::SendAiCommandConfirmation(1, m.message[2].ToString(), m.message[3].ToString()); // OK
				}
				else
					multiplayer::SendAiCommandConfirmation(0, m.message[2].ToString(), m.message[3].ToString()); // b³¹d

				break;
			}
			case 1:
			{
				CommLog(Now() + " Client do not handle command launch confirmation messages");
				break;
			}
			default:
				CommLog(Now() + " Wrong code type");
			}
			break;
		case 4: // stan toru
			switch (m.message[1].ToInt())
			{
			case 0:
				CommLog(Now() + " " + m.message[0].ToString() + " Ask for track free / busy" + " rcvd");
				if (m.message.size() != 3)
				{
					CommLog(Now() + " Wrong number of frames");
					return;
				}
				CommLog(Now() + " Track: " + m.message[2].ToString() + " rcvd");
				multiplayer::SendTrackOccupancy(m.message[2].ToString());
				break;
			case 1:
				CommLog(Now() + " Client do not handle messages with track busy / free info");
				break;
			default:
				CommLog(Now() + " Wrong code type");
			}
			break;
		case 5: // zajêtoœæ odcinków izolowanych
		{
			switch (m.message[1].ToInt())
			{
			case 0:
				CommLog(Now() + " " + m.message[0].ToString() + " Ask for isolated free / busy" + " rcvd");
				if (m.message.size() != 3)
				{
					CommLog(Now() + " Wrong number of frames");
					return;
				}
				CommLog(Now() + " Isolated: " + m.message[2].ToString() + " rcvd");
				// multiplayer::SendIsolatedOccupancy(m.message[2].ToString());
				break;
			case 1:
				CommLog(Now() + " Client do not handle messages with isolated busy / free info");
				break;
			default:
				CommLog(Now() + " Wrong code type");
			}
			break;
		}
		case 6: // stan symulacji
			switch (m.message[1].ToInt())
			{
			case 0:
				CommLog(Now() + " Ask for simulation status rcvd");
				if (m.message.size() != 3)
				{
					CommLog(Now() + " Wrong number of frames");
					return;
				}
				switch (m.message[2].ToInt())
				{
				case 1:
					multiplayer::SendSimulationStatus(1);
					break;
				case 2:
					multiplayer::SendSimulationStatus(2);
					break;
				default:
					break;
				}
				break;
			case 1:
				CommLog(Now() + " Set pause rcvd");
				if (m.message.size() != 3)
				{
					CommLog(Now() + " Wrong number of frames");
					return;
				}
				Global.iPause = m.message[2].ToInt();
				break;
			case 2:
				CommLog(Now() + " Set simulation time rcvd");
				if (m.message.size() != 3)
				{
					CommLog(Now() + " Wrong number of frames");
					return;
				}
				double t = m.message[2].ToFloat();
				simulation::Time.data().wDay = std::floor(t); // niby nie powinno byæ dnia, ale...
				if (Global.fMoveLight >= 0)
					Global.fMoveLight = t; // trzeba by deklinacjê S³oñca przeliczyæ
				simulation::Time.data().wHour = std::floor(24 * t) - 24.0 * simulation::Time.data().wDay;
				simulation::Time.data().wMinute = std::floor(60 * 24 * t) - 60.0 * (24.0 * simulation::Time.data().wDay + simulation::Time.data().wHour);
				simulation::Time.data().wSecond =
				    std::floor(60 * 60 * 24 * t) - 60.0 * (60.0 * (24.0 * simulation::Time.data().wDay + simulation::Time.data().wHour) + simulation::Time.data().wMinute);
				break;
			}
			break;
		default:
			CommLog(Now() + " Recieved not handled message type code: " + to_string(m.message[0].ToInt()));
		}
	}
}
} // namespace multiplayer