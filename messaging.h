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

class TDynamicObject;

namespace multiplayer {

struct DaneRozkaz { // struktura komunikacji z EU07.EXE
    int iSygn; // sygnatura 'EU07'
    int iComm; // rozkaz/status (kod ramki)
    union {
        float fPar[ 62 ];
        int iPar[ 62 ];
        char cString[ 248 ]; // upakowane stringi
    };
};

struct DaneRozkaz2 {              // struktura komunikacji z EU07.EXE
    int iSygn; // sygnatura 'EU07'
    int iComm; // rozkaz/status (kod ramki)
    union {
        float fPar[ 496 ];
        int iPar[ 496 ];
        char cString[ 1984 ]; // upakowane stringi
    };
};

void Navigate( std::string const &ClassName, UINT Msg, WPARAM wParam, LPARAM lParam );

void WyslijEvent( const std::string &e, const std::string &d );
void WyslijString( const std::string &t, int n );
void WyslijWolny( const std::string &t );
void WyslijNamiary( TDynamicObject const *Vehicle );
void WyslijParam( int nr, int fl );
void WyslijUszkodzenia( const std::string &t, char fl );
void WyslijObsadzone(); // -> skladanie wielu pojazdow    

// Ramka danych wiadomości dla interfejsu ZeroMQ
class ZMQFrame
{
public:
	ZMQFrame(const char* buf, int len) : m_data(buf,buf + len), m_messageSize(m_data.size()) {};
	ZMQFrame(std::string str) : m_data(str.begin(), str.end()), m_messageSize(m_data.size()) {};
	ZMQFrame(int32_t);
	ZMQFrame(float);
	ZMQFrame() = default;
	std::string ToString();
	int32_t ToInt();
	float ToFloat();
	const uint8_t* ToByteArray() const;
	size_t size() const { return m_data.size(); };

private:
	int m_messageSize = 0;
	std::vector<uint8_t> m_data;
};

// Wiadomość wieloczęściowa dla interfejsu ZeroMQ
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
	// dostęp jak do zwykłej macierzy
	ZMQFrame operator[](int pos) {
		return m_frames.at(pos);
	}

private:
	std::vector<ZMQFrame> m_frames; // kolejne ramki z parametrami komendy
};

class ZMQConnection
{
public:
	ZMQConnection();
	void poll();
	auto getSocket() { return &net_socket; };
private:
	CpperoMQ::IsReceiveReady<CpperoMQ::DealerSocket> net_pollReceiver = CpperoMQ::isReceiveReady(net_socket, [this]()
	{
		bool more = true;
		while (more)
		{
			CpperoMQ::IncomingMessage mess;
			mess.receive(net_socket, more);
			WriteLog("TCP: ");
			WriteLog(std::string(mess.charData(), mess.size()).c_str());
		}
	});
	CpperoMQ::Context net_context = CpperoMQ::Context();
	CpperoMQ::DealerSocket net_socket = net_context.createDealerSocket();
	CpperoMQ::Poller net_poller;
};

} // multiplayer

//---------------------------------------------------------------------------
