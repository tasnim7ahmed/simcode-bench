/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <cassert>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CsmaUdpTraceClientServerExample");

class TraceBasedUdpClient : public Application
{
public:
    TraceBasedUdpClient();
    virtual ~TraceBasedUdpClient();
    void SetRemote(Address address, uint16_t port);
    void SetTraceFile(std::string filename);
    void SetPacketSize(uint32_t packetSize);

protected:
    virtual void StartApplication() override;
    virtual void StopApplication() override;

private:
    void ScheduleNextTx();
    void SendPacket();

    Ptr<Socket> m_socket;
    Address m_peerAddress;
    uint16_t m_peerPort;
    EventId m_sendEvent;
    bool m_running;

    std::string m_traceFileName;
    std::vector<double> m_times; // times (seconds)
    std::vector<uint32_t> m_sizes;
    uint32_t m_nextTxIndex;
    uint32_t m_packetSize;

    void ParseTraceFile();
};

TraceBasedUdpClient::TraceBasedUdpClient()
    : m_socket(0),
      m_peerPort(0),
      m_running(false),
      m_nextTxIndex(0),
      m_packetSize(1472)
{
}

TraceBasedUdpClient::~TraceBasedUdpClient()
{
    m_socket = 0;
}

void TraceBasedUdpClient::SetRemote(Address address, uint16_t port)
{
    m_peerAddress = address;
    m_peerPort = port;
}

void TraceBasedUdpClient::SetPacketSize(uint32_t packetSize)
{
    m_packetSize = packetSize;
}

void TraceBasedUdpClient::SetTraceFile(std::string filename)
{
    m_traceFileName = filename;
}

void TraceBasedUdpClient::ParseTraceFile()
{
    m_times.clear();
    m_sizes.clear();

    std::ifstream infile(m_traceFileName.c_str());
    if (!infile)
    {
        NS_FATAL_ERROR("Cannot open trace file: " << m_traceFileName);
    }
    std::string line;
    while (std::getline(infile, line))
    {
        std::istringstream iss(line);
        double time;
        uint32_t size;
        if (!(iss >> time >> size))
        {
            continue; // ignore malformed lines
        }
        // Clamp packet size to m_packetSize
        if (size > m_packetSize)
        {
            size = m_packetSize;
        }
        m_times.push_back(time);
        m_sizes.push_back(size);
    }
    infile.close();
    NS_ASSERT(m_times.size() == m_sizes.size());
}

void TraceBasedUdpClient::StartApplication()
{
    NS_LOG_FUNCTION(this);
    m_running = true;
    m_nextTxIndex = 0;

    if (m_socket == 0)
    {
        m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
        m_socket->Connect(InetSocketAddress::ConvertFrom(m_peerAddress));
    }

    ParseTraceFile();

    if (m_times.empty())
    {
        NS_LOG_WARN("Trace file is empty or invalid.");
        return;
    }
    // Schedule the first send at (m_times[0])
    Time sendTime = Seconds(m_times[0]);
    m_sendEvent = Simulator::Schedule(sendTime, &TraceBasedUdpClient::SendPacket, this);
}

void TraceBasedUdpClient::StopApplication()
{
    NS_LOG_FUNCTION(this);
    m_running = false;

    if (m_sendEvent.IsRunning())
    {
        Simulator::Cancel(m_sendEvent);
    }

    if (m_socket)
    {
        m_socket->Close();
        m_socket = 0;
    }
}

void TraceBasedUdpClient::SendPacket()
{
    NS_LOG_FUNCTION(this);

    if (!m_running || m_nextTxIndex >= m_times.size())
        return;

    uint32_t size = m_sizes[m_nextTxIndex];
    Ptr<Packet> packet = Create<Packet>(size);

    NS_LOG_INFO("At " << Simulator::Now().GetSeconds() << "s, sending UDP packet of "
                      << size << " bytes to " << m_peerAddress << ":" << m_peerPort);

    m_socket->Send(packet);

    ++m_nextTxIndex;
    if (m_nextTxIndex < m_times.size())
    {
        double interval = m_times[m_nextTxIndex] - m_times[m_nextTxIndex - 1];
        if (interval < 0)
        {
            NS_LOG_WARN("Non-monotonic trace; skipping negative interval");
            ScheduleNextTx();
            return;
        }
        m_sendEvent = Simulator::Schedule(Seconds(interval), &TraceBasedUdpClient::SendPacket, this);
    }
}

void TraceBasedUdpClient::ScheduleNextTx()
{
    if (m_nextTxIndex < m_times.size())
    {
        m_sendEvent = Simulator::Schedule(Seconds(m_times[m_nextTxIndex]), &TraceBasedUdpClient::SendPacket, this);
    }
}

// --- MAIN PROGRAM ---

int main(int argc, char *argv[])
{
    bool enableIpv6 = false;
    bool enableLogging = false;
    std::string traceFile = "udp-trace.txt";

    CommandLine cmd;
    cmd.AddValue("enableIpv6", "Enable IPv6 addressing", enableIpv6);
    cmd.AddValue("enableLogging", "Enable verbose UDP client/server logging", enableLogging);
    cmd.AddValue("traceFile", "Path to trace file for UDP client (format: time packet_size)", traceFile);
    cmd.Parse(argc, argv);

    if (enableLogging)
    {
        LogComponentEnable("CsmaUdpTraceClientServerExample", LOG_LEVEL_ALL);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    }

    NodeContainer nodes;
    nodes.Create(2);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("5Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1500));
    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4Address serverV4Addr("10.1.1.2");
    Ipv6Address serverV6Addr("2001:1::2");
    Ipv4InterfaceContainer ipv4Ifs;
    Ipv6InterfaceContainer ipv6Ifs;

    if (enableIpv6)
    {
        Ipv6AddressHelper ipv6;
        ipv6.SetBase("2001:1::", 64);
        ipv6Ifs = ipv6.Assign(devices);
        // Set interfaces up
        ipv6Ifs.SetForwarding(0, true);
        ipv6Ifs.SetDefaultRouteInAllNodes(0);
    }
    else
    {
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        ipv4Ifs = ipv4.Assign(devices);
    }

    // UDP Server on n1
    uint16_t port = 4000;
    Address serverAddress;
    if (enableIpv6)
    {
        serverAddress = Inet6SocketAddress(ipv6Ifs.GetAddress(1, 1), port);
    }
    else
    {
        serverAddress = InetSocketAddress(ipv4Ifs.GetAddress(1), port);
    }

    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // UDP Trace-based Client on n0
    Ptr<TraceBasedUdpClient> clientApp = CreateObject<TraceBasedUdpClient>();
    clientApp->SetRemote(serverAddress, port);
    clientApp->SetTraceFile(traceFile);
    // 1500 (MTU) - 20 (IPv4) - 8 (UDP)
    clientApp->SetPacketSize(1472);

    nodes.Get(0)->AddApplication(clientApp);
    clientApp->SetStartTime(Seconds(2.0));
    clientApp->SetStopTime(Seconds(10.0));

    Simulator::Stop(Seconds(10.1));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}