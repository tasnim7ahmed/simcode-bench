#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/network-module.h"
#include <fstream>
#include <vector>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CsmaUdpTraceSimulation");

class TraceBasedUdpClient : public Application
{
public:
    TraceBasedUdpClient();
    virtual ~TraceBasedUdpClient();
    void Setup(Address address, uint16_t port, std::vector<uint32_t> packetSizes, std::vector<double> sendTimes, uint32_t maxPacketSize);

protected:
    virtual void StartApplication(void);
    virtual void StopApplication(void);

    void SendPacket(void);

private:
    Ptr<Socket> m_socket;
    Address m_peerAddress;
    uint16_t m_peerPort;
    EventId m_sendEvent;
    std::vector<uint32_t> m_packetSizes;
    std::vector<double> m_sendTimes;
    uint32_t m_currentIndex;
    uint32_t m_maxPacketSize;
};

TraceBasedUdpClient::TraceBasedUdpClient()
    : m_socket(0),
      m_peerPort(0),
      m_sendEvent(),
      m_currentIndex(0),
      m_maxPacketSize(1472)
{
}

TraceBasedUdpClient::~TraceBasedUdpClient()
{
    m_socket = 0;
}

void TraceBasedUdpClient::Setup(Address address, uint16_t port, std::vector<uint32_t> packetSizes, std::vector<double> sendTimes, uint32_t maxPacketSize)
{
    m_peerAddress = address;
    m_peerPort = port;
    m_packetSizes = packetSizes;
    m_sendTimes = sendTimes;
    m_currentIndex = 0;
    m_maxPacketSize = maxPacketSize;
}

void TraceBasedUdpClient::StartApplication(void)
{
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    m_socket->Connect(InetSocketAddress::ConvertFrom(m_peerAddress).GetIpv4(), m_peerPort);
    if (m_sendTimes.empty())
        return;
    Simulator::Schedule(Seconds(m_sendTimes[0]), &TraceBasedUdpClient::SendPacket, this);
}

void TraceBasedUdpClient::StopApplication(void)
{
    if (m_socket)
        m_socket->Close();
    Simulator::Cancel(m_sendEvent);
}

void TraceBasedUdpClient::SendPacket(void)
{
    if (m_currentIndex >= m_packetSizes.size())
        return;
    uint32_t size = m_packetSizes[m_currentIndex];
    if (size > m_maxPacketSize)
        size = m_maxPacketSize;
    Ptr<Packet> packet = Create<Packet>(size);
    m_socket->Send(packet);

    if ((m_currentIndex + 1) < m_sendTimes.size()) {
        double nextTime = m_sendTimes[m_currentIndex + 1] - m_sendTimes[m_currentIndex];
        m_sendEvent = Simulator::Schedule(Seconds(nextTime), &TraceBasedUdpClient::SendPacket, this);
    }
    ++m_currentIndex;
}

// Helper function to read a simple trace file (CSV with time(s),size(bytes))
void ReadTraceFile(const std::string &filename, std::vector<double> &sendTimes, std::vector<uint32_t> &packetSizes)
{
    std::ifstream in(filename.c_str());
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        double t; uint32_t s;
        if (sscanf(line.c_str(), "%lf,%u", &t, &s) == 2) {
            sendTimes.push_back(t);
            packetSizes.push_back(s);
        }
    }
}

int main(int argc, char *argv[])
{
    bool useIpv6 = false;
    bool enableLogging = false;
    std::string traceFilename = "trace.csv";

    CommandLine cmd;
    cmd.AddValue("useIpv6", "Use IPv6 addressing", useIpv6);
    cmd.AddValue("enableLogging", "Enable verbose logging", enableLogging);
    cmd.AddValue("traceFilename", "Trace file path (CSV: time(s),size(bytes))", traceFilename);
    cmd.Parse(argc, argv);

    if (enableLogging)
    {
        LogComponentEnable("TraceBasedUdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
        LogComponentEnable("CsmaUdpTraceSimulation", LOG_LEVEL_INFO);
    }

    NodeContainer nodes;
    nodes.Create(2);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("5Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1500));
    NetDeviceContainer devices = csma.Install(nodes);

    if (!useIpv6)
    {
        InternetStackHelper stack;
        stack.Install(nodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

        uint16_t port = 4000;

        UdpServerHelper server(port);
        ApplicationContainer serverApps = server.Install(nodes.Get(1));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(10.0));

        // Read trace
        std::vector<double> sendTimes;
        std::vector<uint32_t> packetSizes;
        ReadTraceFile(traceFilename, sendTimes, packetSizes);

        Ptr<TraceBasedUdpClient> clientApp = CreateObject<TraceBasedUdpClient>();
        Address peerAddress = InetSocketAddress(interfaces.GetAddress(1), port);
        clientApp->Setup(peerAddress, port, packetSizes, sendTimes, 1472);

        nodes.Get(0)->AddApplication(clientApp);
        clientApp->SetStartTime(Seconds(2.0));
        clientApp->SetStopTime(Seconds(10.0));
    }
    else
    {
        InternetStackHelper stack;
        stack.Install(nodes);

        Ipv6AddressHelper ipv6;
        ipv6.SetBase("2001:db8::", Ipv6Prefix(64));
        Ipv6InterfaceContainer interfaces6 = ipv6.Assign(devices);

        uint16_t port = 4000;

        UdpServerHelper server(port);
        ApplicationContainer serverApps = server.Install(nodes.Get(1));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(10.0));

        std::vector<double> sendTimes;
        std::vector<uint32_t> packetSizes;
        ReadTraceFile(traceFilename, sendTimes, packetSizes);

        Ptr<TraceBasedUdpClient> clientApp = CreateObject<TraceBasedUdpClient>();
        Address peerAddress = Inet6SocketAddress(interfaces6.GetAddress(1,1), port);
        clientApp->Setup(peerAddress, port, packetSizes, sendTimes, 1472);

        nodes.Get(0)->AddApplication(clientApp);
        clientApp->SetStartTime(Seconds(2.0));
        clientApp->SetStopTime(Seconds(10.0));
    }

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}