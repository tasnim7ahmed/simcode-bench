#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-westwood.h"
#include "ns3/mobility-module.h"
#include <fstream>
#include <map>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpWestwoodSimulation");

std::map<Ptr<Socket>, uint32_t> cWndMap;
std::map<Ptr<Socket>, uint32_t> ssThreshMap;
std::map<Ptr<Socket>, Time> rttMap;
std::map<Ptr<Socket>, Time> rtoMap;
std::map<Ptr<Socket>, uint32_t> inFlightMap;

void
CwndTracer(Ptr<Socket> socket, uint32_t oldVal, uint32_t newVal)
{
    cWndMap[socket] = newVal;
    std::ofstream cwndFile("cwnd-trace.txt", std::ios::app);
    cwndFile << Simulator::Now().GetSeconds() << "\t" << newVal << std::endl;
}

void
SsThreshTracer(Ptr<Socket> socket, uint32_t oldVal, uint32_t newVal)
{
    ssThreshMap[socket] = newVal;
    std::ofstream ssthreshFile("ssthresh-trace.txt", std::ios::app);
    ssthreshFile << Simulator::Now().GetSeconds() << "\t" << newVal << std::endl;
}

void
RttTracer(Ptr<Socket> socket, Time oldVal, Time newVal)
{
    rttMap[socket] = newVal;
    std::ofstream rttFile("rtt-trace.txt", std::ios::app);
    rttFile << Simulator::Now().GetSeconds() << "\t" << newVal.GetSeconds() << std::endl;
}

void
RtoTracer(Ptr<Socket> socket, Time oldVal, Time newVal)
{
    rtoMap[socket] = newVal;
    std::ofstream rtoFile("rto-trace.txt", std::ios::app);
    rtoFile << Simulator::Now().GetSeconds() << "\t" << newVal.GetSeconds() << std::endl;
}

void
InFlightTracer(Ptr<Socket> socket, uint32_t oldVal, uint32_t newVal)
{
    inFlightMap[socket] = newVal;
    std::ofstream inflightFile("inflight-trace.txt", std::ios::app);
    inflightFile << Simulator::Now().GetSeconds() << "\t" << newVal << std::endl;
}

void
TraceTcpMetrics(Ptr<Socket> socket)
{
    socket->TraceConnectWithoutContext("CongestionWindow", MakeCallback(&CwndTracer));
    socket->TraceConnectWithoutContext("SlowStartThreshold", MakeCallback(&SsThreshTracer));
    socket->TraceConnectWithoutContext("RTT", MakeCallback(&RttTracer));
    socket->TraceConnectWithoutContext("RTO", MakeCallback(&RtoTracer));
    socket->TraceConnectWithoutContext("BytesInFlight", MakeCallback(&InFlightTracer));
}

int
main(int argc, char* argv[])
{
    std::string tcpVariant = "TcpWestwood";
    std::string bandwidth = "10Mbps";
    std::string delay = "5ms";
    double packetErrorRate = 0.0;
    uint32_t maxBytes = 0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("tcpVariant", "TCP variant to use (e.g., TcpWestwood, TcpWestwoodPlus)", tcpVariant);
    cmd.AddValue("bandwidth", "Bottleneck bandwidth", bandwidth);
    cmd.AddValue("delay", "Bottleneck delay", delay);
    cmd.AddValue("packetErrorRate", "Packet loss probability on bottleneck link", packetErrorRate);
    cmd.AddValue("maxBytes", "Total number of bytes the application will send", maxBytes);
    cmd.Parse(argc, argv);

    if (tcpVariant == "TcpWestwood")
    {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpWestwood::GetTypeId()));
    }
    else if (tcpVariant == "TcpWestwoodPlus")
    {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType",
                           TypeIdValue(TcpWestwood::GetTypeId()));
        Config::SetDefault("ns3::TcpWestwood::FilterType", StringValue("Tsoak"));
    }

    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1448));
    Config::SetDefault("ns3::TcpSocket::DelAckTimeout", TimeValue(Seconds(0)));
    Config::SetDefault("ns3::TcpSocketBase::MinRto", TimeValue(Seconds(0.2)));
    Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(1));
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(1 << 20));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(1 << 20));

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue(bandwidth));
    pointToPoint.SetChannelAttribute("Delay", StringValue(delay));
    pointToPoint.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("100p"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory",
                                     InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(1));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());

    TraceTcpMetrics(ns3TcpSocket);

    Ptr<MyApp> app = CreateObject<MyApp>();
    app->Setup(ns3TcpSocket, sinkAddress, 1040, maxBytes, DataRate(bandwidth));
    nodes.Get(0)->AddApplication(app);
    app->SetStartTime(Seconds(1.0));
    app->SetStopTime(Seconds(10.0));

    AsciiTraceHelper asciiTraceHelper;
    pointToPoint.EnableAsciiAll(asciiTraceHelper.CreateFileStream("tcp-westwood.tr"));
    pointToPoint.EnablePcapAll("tcp-westwood");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

class MyApp : public Application
{
  public:
    MyApp();
    virtual ~MyApp();

    void Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets,
               DataRate dataRate);

  private:
    virtual void StartApplication(void);
    virtual void StopApplication(void);

    void ScheduleTx(void);
    void SendPacket(void);

    Ptr<Socket> m_socket;
    Address m_peer;
    uint32_t m_packetSize;
    uint32_t m_nPackets;
    DataRate m_dataRate;
    EventId m_sendEvent;
    bool m_running;
    uint32_t m_packetsSent;
};

MyApp::MyApp()
    : m_socket(0),
      m_peer(),
      m_packetSize(0),
      m_nPackets(0),
      m_dataRate(),
      m_sendEvent(),
      m_running(false),
      m_packetsSent(0)
{
}

MyApp::~MyApp()
{
    m_socket = nullptr;
}

void
MyApp::Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets,
             DataRate dataRate)
{
    m_socket = socket;
    m_peer = address;
    m_packetSize = packetSize;
    m_nPackets = nPackets;
    m_dataRate = dataRate;
}

void
MyApp::StartApplication(void)
{
    m_running = true;
    m_packetsSent = 0;
    if (m_socket->Connect(m_peer) == -1)
    {
        NS_FATAL_ERROR("Failed to connect socket");
    }
    SendPacket();
}

void
MyApp::StopApplication(void)
{
    m_running = false;

    if (m_sendEvent.IsRunning())
    {
        Simulator::Cancel(m_sendEvent);
    }

    if (m_socket)
    {
        m_socket->Close();
    }
}

void
MyApp::SendPacket(void)
{
    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    m_socket->Send(packet);

    if (++m_packetsSent < m_nPackets)
    {
        ScheduleTx();
    }
}

void
MyApp::ScheduleTx(void)
{
    if (m_running)
    {
        Time tNext(Seconds(m_packetSize * 8.0 / m_dataRate.GetBitRate()));
        m_sendEvent = Simulator::Schedule(tNext, &MyApp::SendPacket, this);
    }
}