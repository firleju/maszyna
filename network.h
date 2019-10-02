/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include "stdafx.h"
#include <CpperoMQ/All.hpp>

#include "winheaders.h"
#include "Logs.h"

namespace multiplayer {
	enum network_codes {
		handshake_info = 0,
		ping = 1,
		pong = 2,
		ai_command = 3,
		track_occupancy = 4,
		isolated_occupancy = 5,
		param_set = 6,
		param_set_pause = 1,
		param_set_time = 2,
		vehicle_ask = 7,
		vehicle_params = 8,
		vehicle_damage = 9
	};

	// Ramka danych wiadomoœci dla interfejsu ZeroMQ
	class ZMQFrame
	{
	public:
		ZMQFrame(const char* buf, int len) : m_data(buf, buf + len), m_messageSize(m_data.size()) {};
		ZMQFrame(std::string str) : m_data(str.begin(), str.end()), m_messageSize(m_data.size()) {};
		ZMQFrame(int32_t);
		ZMQFrame(float);
		ZMQFrame() = default;
		std::string ToString();
		int32_t ToInt();
		float ToFloat();
		const uint8_t* ToByteArray() const;
		size_t size() const { return m_data.size(); };
		int& operator=(int32_t i);
		float& operator=(float f);
		std::string& operator=(std::string s);
	private:
		size_t m_messageSize = 0;
		std::vector<uint8_t> m_data;
	};

	// Wiadomoœæ wieloczêœciowa dla interfejsu ZeroMQ
	class ZMQMessage : public CpperoMQ::Sendable, public CpperoMQ::Receivable
	{
	public:
		ZMQMessage();
		virtual bool send(const CpperoMQ::Socket& socket, const bool moreToSend) const override;
		virtual bool receive(CpperoMQ::Socket& socket, bool& moreToReceive) override;
		auto AddFrame(std::string str) {
			m_frames.emplace_back(str);
		};
		auto AddFrame(int i) {
			m_frames.emplace_back(i);
		};
		auto AddFrame(float f) {
			m_frames.emplace_back(f);
		};
		auto AddFrame() {
			m_frames.emplace_back();
		};

		// for for range-base loops
		auto begin() {
			return m_frames.begin();
		}
		auto cbegin() {
			return m_frames.cbegin();
		}
		auto end() {
			return m_frames.end();
		}
		auto cend() {
			return m_frames.cend();
		}
		// dostêp jak do zwyk³ej macierzy
		ZMQFrame& operator[](size_t pos) {
			return m_frames.at(pos);
		}

		auto size() {
			return m_frames.size();
		}

	private:
		std::vector<ZMQFrame> m_frames; // kolejne ramki z parametrami komendy
	};

	struct network_conf_t
	{
		bool enable = false;
		std::string address = "127.0.0.1";
		std::string port = "5555";
		std::string identity = "EU07";
	};

	struct network_queue_t
	{
		multiplayer::ZMQMessage message;
		resource_timestamp time = std::chrono::high_resolution_clock::now();
		network_queue_t(multiplayer::ZMQMessage& m)
		{
			message = m;
			time = std::chrono::high_resolution_clock::now();
		}
		bool checkTime();
	};


	// Po³¹czenie ZeroMQ
	class ZMQConnection
	{
	public:
		ZMQConnection();
		void poll(); // otrzymuje wiadomoœci z socketa i wrzuca do wewnêtrznej listy
		void send(); // wysy³a wiadomoœci z ogólnodostepnej listy
		auto getIncomingQueue()->std::list<multiplayer::network_queue_t>&;
		static const int protocol_version = 1;
	private:
		CpperoMQ::IsReceiveReady<CpperoMQ::DealerSocket> net_pollReceiver = CpperoMQ::isReceiveReady(m_socket, [this]()
		{
			bool more = true;
			ZMQMessage mess;
			while (more)
			{
				mess.receive(m_socket, more);
				//WriteLog("TCP: " << mess);
				//WriteLog(std::string(mess.charData(), mess.size()).c_str());

			}
			// check what is commming in and delete outgoing message if this is recieve OK signal
			m_incoming_queue.push_back(mess);

		});
		CpperoMQ::Context m_context = CpperoMQ::Context();
		CpperoMQ::DealerSocket m_socket = m_context.createDealerSocket();
		CpperoMQ::Poller m_poller;
		std::list<multiplayer::network_queue_t> m_incoming_queue{}; // lista wiadomoœci przychodz¹cych
	};
	
	
	void SendHandshakeInfo(bool change = false);
    void SendPing();
	void SendEventCallConfirmation(int status, std::string name);
	void SendAiCommandConfirmation(int status, std::string vehicle, std::string command);
	void SendTrackOccupancy(std::string name);
	void SendTrackOccupancy(std::string name, bool occupied, int damage_flag);
	//void SendIsolatedOccupancy(std::string name);
	//void SendIsolatedOccupancy(std::string name, bool occupied);
	void SendSimulationStatus(int which_param);
	void HandleMessage(std::list<network_queue_t> &incoming_queue);
}