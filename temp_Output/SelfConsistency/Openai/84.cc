/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include <fstream>
#include <vector>
#include <string>

using namespace ns3;

// Trace file format: one size per line
std::vector<uint32_t> ReadTraceFile (std::string filename)
{
    std::vector<uint32_t> traceSizes;
    std::ifstream ifs (filename, std::ios::in);
    std::string line;
    while (std::getline (ifs, line))
    {
        if (line.empty()) continue;
        traceSizes.push_back (std::stoul (line));
    }
    return traceSizes;
}

class TraceBasedUdpClientApp : public Application
{
public:
    TraceBasedUdpClientApp ();
    virtual ~TraceBasedUdpClientApp ();
    void Setup (Address address, std::vector<uint32_t> packetSizes, double interval);

private:
    virtual void StartApplication (void);
    virtual void StopApplication (void);

    void ScheduleNextPacket ();
    void SendPacket ();

    Ptr<Socket>      m_socket;
    Address          m_peerAddress;
    std::vector<uint32_t> m_packetSizes;
    double           m_interval;
    uint32_t         m_currentPacket;
    EventId          m_sendEvent;
    bool             m_running;
};

TraceBasedUdpClientApp::TraceBasedUdpClientApp ()
    : m_socket (0),
      m_peerAddress (),
      m_packetSizes (),
      m_interval (1.0),
      m_currentPacket (0),
      m_sendEvent (),
      m_running (false)
{
}

TraceBasedUdpClientApp::~TraceBasedUdpClientApp ()
{
    m_socket = 0;
}

void TraceBasedUdpClientApp::Setup (Address address, std::vector<uint32_t> packetSizes, double interval)
{
    m_peerAddress = address;
    m_packetSizes = packetSizes;
    m_interval = interval;
}

void TraceBasedUdpClientApp::StartApplication ()
{
    m_running = true;
    m_currentPacket = 0;

    if (m_socket == 0)
    {
        m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
        m_socket->Connect (m_peerAddress);
    }
    SendPacket ();
}

void TraceBasedUdpClientApp::StopApplication ()
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

void TraceBasedUdpClientApp::SendPacket ()
{
    if (m_currentPacket < m_packetSizes.size ())
    {
        uint32_t pktSize = m_packetSizes[m_currentPacket];
        Ptr<Packet> packet = Create<Packet> (pktSize);

        m_socket->Send (packet);
        m_currentPacket++;
        ScheduleNextPacket ();
    }
}

void TraceBasedUdpClientApp::ScheduleNextPacket ()
{
    if (m_running && m_currentPacket < m_packetSizes.size ())
    {
        m_sendEvent = Simulator::Schedule (Seconds (m_interval), &TraceBasedUdpClientApp::SendPacket, this);
    }
}

int main (int argc, char *argv[])
{
    bool useIpv6 = false;
    bool enableLogging = false;
    std::string traceFile = "trace.txt";
    double packetInterval = 0.5;

    CommandLine cmd;
    cmd.AddValue ("useIpv6", "Use IPv6 addressing", useIpv6);
    cmd.AddValue ("enableLogging", "Enable info logging", enableLogging);
    cmd.AddValue ("traceFile", "Trace file with packet sizes per line", traceFile);
    cmd.AddValue ("packetInterval", "Interval between packets (s)", packetInterval);
    cmd.Parse (argc, argv);

    if (enableLogging)
    {
        LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
        LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable ("TraceBasedUdpClientApp", LOG_LEVEL_INFO);
        LogComponentEnable ("CsmaNetDevice", LOG_LEVEL_INFO);
    }

    // Nodes setup
    NodeContainer nodes;
    nodes.Create (2);

    // CSMA channel setup
    CsmaHelper csma;
    csma.SetChannelAttribute ("DataRate", StringValue ("5Mbps"));
    csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
    csma.SetDeviceAttribute ("Mtu", UintegerValue (1500));

    NetDeviceContainer devices = csma.Install (nodes);

    // Internet stack
    InternetStackHelper internetv4;
    InternetStackHelper internetv6;
    if (useIpv6)
        internetv6.Install (nodes);
    else
        internetv4.Install (nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i4ifaces;
    Ipv6AddressHelper ipv6;
    Ipv6InterfaceContainer i6ifaces;

    if (useIpv6)
    {
        ipv6.SetBase ("2001:1::", 64);
        i6ifaces = ipv6.Assign (devices);
    }
    else
    {
        i4ifaces = ipv4.Assign (devices);
    }

    // UDP server on n1
    uint16_t port = 9000;
    ApplicationContainer serverApps;
    if (useIpv6)
    {
        UdpServerHelper server (port);
        serverApps = server.Install (nodes.Get (1));
        serverApps.Start (Seconds (1.0));
        serverApps.Stop (Seconds (10.0));
    }
    else
    {
        UdpServerHelper server (port);
        serverApps = server.Install (nodes.Get (1));
        serverApps.Start (Seconds (1.0));
        serverApps.Stop (Seconds (10.0));
    }

    // Build address for client-to-server
    Address serverAddress;
    if (useIpv6)
    {
        // Get the first global address on node 1
        Ipv6Address addr = i6ifaces.GetAddress (1, 1); 
        serverAddress = Inet6SocketAddress (addr, port);
    }
    else
    {
        Ipv4Address addr = i4ifaces.GetAddress (1);
        serverAddress = InetSocketAddress (addr, port);
    }

    // Read trace file (packet sizes)
    std::vector<uint32_t> traceSizes = ReadTraceFile (traceFile);

    // Custom UDP client app on n0
    Ptr<TraceBasedUdpClientApp> traceClient = CreateObject<TraceBasedUdpClientApp> ();
    traceClient->Setup (serverAddress, traceSizes, packetInterval);
    nodes.Get (0)->AddApplication (traceClient);
    traceClient->SetStartTime (Seconds (2.0));
    traceClient->SetStopTime (Seconds (10.0));

    Simulator::Stop (Seconds (10.0));
    Simulator::Run ();
    Simulator::Destroy ();
    return 0;
}