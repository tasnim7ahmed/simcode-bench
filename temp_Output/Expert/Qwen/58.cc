#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-header.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpBbrNetworkSimulation");

class CwndTracer {
public:
  static void TraceCwnd(Ptr<OutputStreamWrapper> stream, uint32_t oldVal, uint32_t newVal) {
    *stream->GetStream() << Simulator::Now().GetSeconds() << " " << newVal << std::endl;
  }
};

class ThroughputTracer {
public:
  ThroughputTracer()
    : m_lastTime(0), m_lastTxBytes(0) {}

  static void TraceThroughput(Ptr<OutputStreamWrapper> stream, Ptr<const Packet> pkt, const TcpHeader& header, SocketWho who) {
    auto tracer = new ThroughputTracer();
    tracer->m_stream = stream;
    tracer->m_lastTime = Simulator::Now().GetSeconds();
    tracer->m_lastTxBytes = 0;
    Config::ConnectWithoutContext("/NodeList/*/ApplicationList/*/$ns3::TcpSocketBase/Tx", MakeBoundCallback(&ThroughputTracer::UpdateThroughput, tracer));
  }

  void UpdateThroughput(Ptr<const Packet> pkt, const TcpHeader& header, SocketWho who) {
    double now = Simulator::Now().GetSeconds();
    m_lastTxBytes += pkt->GetSize();

    if (now - m_lastTime >= 1.0) {
      double throughput = (m_lastTxBytes * 8.0) / (now - m_lastTime) / 1e6; // Mbps
      *m_stream->GetStream() << now << " " << throughput << std::endl;
      m_lastTxBytes = 0;
      m_lastTime = now;
    }
  }

private:
  Ptr<OutputStreamWrapper> m_stream;
  double m_lastTime;
  uint64_t m_lastTxBytes;
};

int main(int argc, char *argv[]) {
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpBbr::GetTypeId()));
  Config::SetDefault("ns3::TcpBbr::RttProbeInterval", TimeValue(Seconds(10)));
  Config::SetDefault("ns3::TcpBbr::MinimumCwndInSegments", UintegerValue(4));

  NodeContainer sender;
  sender.Create(1);
  NodeContainer r1;
  r1.Create(1);
  NodeContainer r2;
  r2.Create(1);
  NodeContainer receiver;
  receiver.Create(1);

  PointToPointHelper p2pHighBandwidth;
  p2pHighBandwidth.SetDeviceAttribute("DataRate", DataRateValue(DataRate("1000Mbps")));
  p2pHighBandwidth.SetChannelAttribute("Delay", TimeValue(MilliSeconds(5)));

  PointToPointHelper p2pLowBandwidth;
  p2pLowBandwidth.SetDeviceAttribute("DataRate", DataRateValue(DataRate("10Mbps")));
  p2pLowBandwidth.SetChannelAttribute("Delay", TimeValue(MilliSeconds(10)));

  NetDeviceContainer senderR1 = p2pHighBandwidth.Install(sender.Get(0), r1.Get(0));
  NetDeviceContainer r1r2 = p2pLowBandwidth.Install(r1.Get(0), r2.Get(0));
  NetDeviceContainer r2Receiver = p2pHighBandwidth.Install(r2.Get(0), receiver.Get(0));

  InternetStackHelper stack;
  stack.InstallAll();

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer senderR1Ifs = address.Assign(senderR1);
  address.NewNetwork();
  Ipv4InterfaceContainer r1r2Ifs = address.Assign(r1r2);
  address.NewNetwork();
  Ipv4InterfaceContainer r2ReceiverIfs = address.Assign(r2Receiver);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t sinkPort = 8080;
  Address sinkAddress(InetSocketAddress(r2ReceiverIfs.GetAddress(1), sinkPort));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
  ApplicationContainer sinkApps = packetSinkHelper.Install(receiver.Get(0));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(100.0));

  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(sender.Get(0), TcpSocketFactory::GetTypeId());
  ns3TcpSocket->TraceConnectWithoutContext("CongestionWindow", MakeBoundCallback(&CwndTracer::TraceCwnd, Create<OutputStreamWrapper>("cwnd-trace.txt", std::ios::out)));

  ThroughputTracer::TraceThroughput(Create<OutputStreamWrapper>("throughput-trace.txt", std::ios::out), ns3TcpSocket, TcpHeader(), SocketWho::SENDER);

  AsciiTraceHelper asciiTraceHelper;
  p2pLowBandwidth.EnablePcapAll("bottleneck-link-pcap", true, true);
  p2pHighBandwidth.EnablePcapAll("high-bandwidth-link-pcap", true, true);

  Config::Connect("/NodeList/1/DeviceList/1/$ns3::PointToPointNetDevice/PhyRxDrop", MakeBoundCallback([](Ptr<OutputStreamWrapper> stream, Ptr<const Packet> p) {
    *stream->GetStream() << Simulator::Now().GetSeconds() << " " << p->GetSize() << std::endl;
  }, Create<OutputStreamWrapper>("queue-drops-trace.txt", std::ios::out)));

  Config::Connect("/NodeList/1/DeviceList/1/$ns3::PointToPointNetDevice/TxQueue/BytesInQueue", MakeBoundCallback([](Ptr<OutputStreamWrapper> stream, uint32_t bytes) {
    *stream->GetStream() << Simulator::Now().GetSeconds() << " " << bytes << std::endl;
  }, Create<OutputStreamWrapper>("queue-size-trace.txt", std::ios::out)));

  class MyApp : public Application {
  public:
    MyApp();
    virtual ~MyApp();
    void Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate);
  private:
    virtual void StartApplication(void);
    virtual void StopApplication(void);
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

  void MyApp::Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate)
  {
    m_socket = socket;
    m_peer = address;
    m_packetSize = packetSize;
    m_nPackets = nPackets;
    m_dataRate = dataRate;
  }

  void MyApp::StartApplication()
  {
    m_running = true;
    m_packetsSent = 0;
    m_socket->Bind();
    m_socket->Connect(m_peer);
    SendPacket();
  }

  void MyApp::StopApplication()
  {
    m_running = false;
    if (m_sendEvent.IsRunning()) {
      Simulator::Cancel(m_sendEvent);
    }
    if (m_socket) {
      m_socket->Close();
    }
  }

  void MyApp::SendPacket()
  {
    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    m_socket->Send(packet);
    m_packetsSent++;
    if (m_packetsSent < m_nPackets) {
      Time tNext(Seconds((double)m_packetSize * 8 / (double)m_dataRate.GetBitRate()));
      m_sendEvent = Simulator::Schedule(tNext, &MyApp::SendPacket, this);
    }
  }

  Ptr<MyApp> app = CreateObject<MyApp>();
  DataRate appRate("1000Mbps");
  app->Setup(ns3TcpSocket, sinkAddress, 1024, 1000000, appRate);
  sender.Get(0)->AddApplication(app);
  app->SetStartTime(Seconds(1.0));
  app->SetStopTime(Seconds(100.0));

  Simulator::Stop(Seconds(100.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}