#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include <fstream>
#include <string>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CsmaUdpTraceSimulation");

// Simple trace parser: expects one packet per line, in format: <time_sec> <size_bytes>
struct TraceEntry {
    double time;
    uint32_t size;
};
std::vector<TraceEntry> ReadTraceFile(std::string filename) {
    std::vector<TraceEntry> entries;
    std::ifstream infile(filename);
    double t;
    uint32_t s;
    while (infile >> t >> s) {
        TraceEntry e;
        e.time = t;
        e.size = s;
        entries.push_back(e);
    }
    return entries;
}

// Custom UDP trace application: sends according to trace vector.
class UdpTraceClientApp : public Application
{
public:
    UdpTraceClientApp();
    virtual ~UdpTraceClientApp();
    void Setup(Address address, uint16_t port, std::vector<TraceEntry> entries);
private:
    virtual void StartApplication() override;
    virtual void StopApplication() override;
    void ScheduleNext();
    void SendPacket();

    Ptr<Socket> m_socket;
    Address m_peerAddress;
    uint16_t m_peerPort;
    std::vector<TraceEntry> m_entries;
    uint32_t m_current;
    EventId m_sendEvent;
    bool m_running;
};
UdpTraceClientApp::UdpTraceClientApp()
    : m_socket(0), m_peerPort(0), m_current(0), m_running(false) {}
UdpTraceClientApp::~UdpTraceClientApp() {
    m_socket = 0;
}
void UdpTraceClientApp::Setup(Address address, uint16_t port, std::vector<TraceEntry> entries) {
    m_peerAddress = address;
    m_peerPort = port;
    m_entries = std::move(entries);
}
void UdpTraceClientApp::StartApplication() {
    m_running = true;
    m_current = 0;
    if (!m_socket) {
        m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
        m_socket->Connect(InetSocketAddress::IsMatchingType(m_peerAddress)
            ? m_peerAddress : Inet6SocketAddress(m_peerAddress, m_peerPort));
    }
    if (!m_entries.empty()) {
        double delay = m_entries.at(m_current).time - Simulator::Now().GetSeconds();
        if (delay < 0.0) delay = 0.0;
        m_sendEvent = Simulator::Schedule(Seconds(delay), &UdpTraceClientApp::SendPacket, this);
    }
}
void UdpTraceClientApp::StopApplication() {
    m_running = false;
    if (m_sendEvent.IsRunning()) {
        Simulator::Cancel(m_sendEvent);
    }
    if (m_socket) {
        m_socket->Close();
    }
}
void UdpTraceClientApp::SendPacket() {
    if (!m_running || m_current >= m_entries.size())
        return;
    uint32_t pktSize = m_entries.at(m_current).size;
    Ptr<Packet> packet = Create<Packet>(pktSize);
    m_socket->Send(packet);
    m_current++;
    if (m_current < m_entries.size()) {
        double delay = m_entries.at(m_current).time - m_entries.at(m_current-1).time;
        if (delay < 0.0) delay = 0.0;
        m_sendEvent = Simulator::Schedule(Seconds(delay), &UdpTraceClientApp::SendPacket, this);
    }
}

// Main simulation driver
int main(int argc, char *argv[]) {
    std::string traceFile = "trace.txt";
    bool useIpv6 = false;
    bool enableLog = false;
    CommandLine cmd;
    cmd.AddValue("traceFile", "Path to packet trace file", traceFile);
    cmd.AddValue("useIpv6", "Use IPv6 addresses (default: false)", useIpv6);
    cmd.AddValue("enableLog", "Enable logging (default: false)", enableLog);
    cmd.Parse(argc, argv);

    if (enableLog) {
        LogComponentEnable("CsmaUdpTraceSimulation", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
        LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    }

    NodeContainer nodes;
    nodes.Create(2);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("5Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("2ms"));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1500));
    NetDeviceContainer devices = csma.Install(nodes);

    if (useIpv6) {
        InternetStackHelper stack;
        stack.Install(nodes);

        Ipv6AddressHelper ipv6;
        ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
        Ipv6InterfaceContainer interfaces = ipv6.Assign(devices);
        interfaces.SetForwarding(0, true);
        interfaces.SetForwarding(1, true);

        uint16_t port = 9999;
        // UDP Echo Server
        UdpEchoServerHelper server(port);
        ApplicationContainer serverApp = server.Install(nodes.Get(1));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Read/parse trace file
        std::vector<TraceEntry> trace = ReadTraceFile(traceFile);

        // Trace-based UDP Client
        Ptr<UdpTraceClientApp> app = CreateObject<UdpTraceClientApp>();
        Inet6SocketAddress dstAddr(interfaces.GetAddress(1,1), port);
        app->Setup(dstAddr, port, trace);
        nodes.Get(0)->AddApplication(app);
        app->SetStartTime(Seconds(2.0));
        app->SetStopTime(Seconds(10.0));
    } else {
        InternetStackHelper stack;
        stack.Install(nodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

        uint16_t port = 9999;
        // UDP Echo Server
        UdpEchoServerHelper server(port);
        ApplicationContainer serverApp = server.Install(nodes.Get(1));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Read/parse trace file
        std::vector<TraceEntry> trace = ReadTraceFile(traceFile);

        // Trace-based UDP Client
        Ptr<UdpTraceClientApp> app = CreateObject<UdpTraceClientApp>();
        InetSocketAddress dstAddr(interfaces.GetAddress(1), port);
        app->Setup(dstAddr, port, trace);
        nodes.Get(0)->AddApplication(app);
        app->SetStartTime(Seconds(2.0));
        app->SetStopTime(Seconds(10.0));
    }

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}