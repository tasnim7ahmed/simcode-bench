/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-address.h"
#include "ns3/socket.h"
#include "ns3/udp-socket.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("CustomUdpSocketExample");

// Custom UDP Server Application
class CustomUdpServer : public Application
{
public:
    CustomUdpServer ();
    virtual ~CustomUdpServer ();
    void Setup (uint16_t port);

private:
    virtual void StartApplication (void);
    virtual void StopApplication (void);

    void HandleRead (Ptr<Socket> socket);

    Ptr<Socket> m_socket;
    uint16_t m_port;
};

CustomUdpServer::CustomUdpServer ()
    : m_socket (0),
      m_port (0)
{
}

CustomUdpServer::~CustomUdpServer ()
{
    m_socket = 0;
}

void
CustomUdpServer::Setup (uint16_t port)
{
    m_port = port;
}

void
CustomUdpServer::StartApplication ()
{
    if (!m_socket)
    {
        m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
        InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
        m_socket->Bind (local);
    }
    m_socket->SetRecvCallback (MakeCallback (&CustomUdpServer::HandleRead, this));
}

void
CustomUdpServer::StopApplication ()
{
    if (m_socket)
    {
        m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
        m_socket->Close ();
    }
}

void
CustomUdpServer::HandleRead (Ptr<Socket> socket)
{
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom (from)))
    {
        if (packet->GetSize () > 0)
        {
            InetSocketAddress address = InetSocketAddress::ConvertFrom (from);
            NS_LOG_INFO ("Server received " << packet->GetSize ()
                         << " bytes from " << address.GetIpv4 ()
                         << " port " << address.GetPort ()
                         << " at " << Simulator::Now ().GetSeconds () << "s");
        }
    }
}

// Custom UDP Client Application
class CustomUdpClient : public Application
{
public:
    CustomUdpClient ();
    virtual ~CustomUdpClient ();
    void Setup (Ipv4Address remoteAddress, uint16_t remotePort, uint32_t packetSize, Time interval);

private:
    virtual void StartApplication (void);
    virtual void StopApplication (void);

    void ScheduleTx (void);
    void SendPacket (void);

    Ptr<Socket> m_socket;
    Ipv4Address m_peerAddress;
    uint16_t m_peerPort;
    uint32_t m_packetSize;
    Time m_interval;
    EventId m_sendEvent;
    bool m_running;

};

CustomUdpClient::CustomUdpClient ()
    : m_socket (0),
      m_peerAddress (),
      m_peerPort (0),
      m_packetSize (0),
      m_interval (Seconds (1.0)),
      m_running (false)
{
}

CustomUdpClient::~CustomUdpClient ()
{
    m_socket = 0;
}

void
CustomUdpClient::Setup (Ipv4Address remoteAddress, uint16_t remotePort, uint32_t packetSize, Time interval)
{
    m_peerAddress = remoteAddress;
    m_peerPort = remotePort;
    m_packetSize = packetSize;
    m_interval = interval;
}

void
CustomUdpClient::StartApplication ()
{
    m_running = true;
    m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
    InetSocketAddress remote = InetSocketAddress (m_peerAddress, m_peerPort);
    m_socket->Connect (remote);
    ScheduleTx ();
}

void
CustomUdpClient::StopApplication ()
{
    m_running = false;

    if (m_sendEvent.IsRunning ())
    {
        Simulator::Cancel (m_sendEvent);
    }

    if (m_socket)
    {
        m_socket->Close ();
    }
}

void
CustomUdpClient::SendPacket ()
{
    Ptr<Packet> packet = Create<Packet> (m_packetSize);
    m_socket->Send (packet);
    ScheduleTx ();
}

void
CustomUdpClient::ScheduleTx ()
{
    if (m_running)
    {
        m_sendEvent = Simulator::Schedule (m_interval, &CustomUdpClient::SendPacket, this);
    }
}

int
main (int argc, char *argv[])
{
    LogComponentEnable ("CustomUdpSocketExample", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create (2);

    // Point-to-point link configuration
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
    p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

    NetDeviceContainer devices = p2p.Install (nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install (nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign (devices);

    // Application parameters
    uint16_t serverPort = 4000;
    uint32_t packetSize = 1024;
    Time sendInterval = Seconds (1.0);

    // Install and configure server
    Ptr<CustomUdpServer> serverApp = CreateObject<CustomUdpServer> ();
    serverApp->Setup (serverPort);
    nodes.Get (1)->AddApplication (serverApp);
    serverApp->SetStartTime (Seconds (0.0));
    serverApp->SetStopTime (Seconds (10.0));

    // Install and configure client
    Ptr<CustomUdpClient> clientApp = CreateObject<CustomUdpClient> ();
    clientApp->Setup (interfaces.GetAddress (1), serverPort, packetSize, sendInterval);
    nodes.Get (0)->AddApplication (clientApp);
    clientApp->SetStartTime (Seconds (1.0));
    clientApp->SetStopTime (Seconds (10.0));

    Simulator::Stop (Seconds (10.0));
    Simulator::Run ();
    Simulator::Destroy ();
    return 0;
}