#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-header.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/bbr-module.h"

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
  ThroughputTracer(Ptr<OutputStreamWrapper> stream)
    : m_stream(stream), m_lastTotalRx(0), m_lastTime(Simulator::Now()) {}

  void TraceRxBytes(uint32_t bytesReceived) {
    Time now = Simulator::Now();
    double delta = (now.GetSeconds() - m_lastTime.GetSeconds());
    if (delta >= 1.0) {
      double throughput = (bytesReceived - m_lastTotalRx) * 8.0 / (delta * 1000000.0); // Mbps
      *m_stream->GetStream() << now.GetSeconds() << " " << throughput << std::endl;
      m_lastTotalRx = bytesReceived;
      m_lastTime = now;
    }
  }

private:
  Ptr<OutputStreamWrapper> m_stream;
  uint64_t m_lastTotalRx;
  Time m_lastTime;
};

int main(int argc, char *argv[]) {
  Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpBbr::GetTypeId()));
  Config::SetDefault("ns3::TcpBbrSocketState::RttProbeInterval", TimeValue(Seconds(10)));
  Config::SetDefault("ns3::TcpBbrSocketState::MinPipeCwnd", UintegerValue(4));

  NodeContainer nodes;
  nodes.Create(4);

  NodeContainer senderRouter = NodeContainer(nodes.Get(0), nodes.Get(1));
  NodeContainer routers = NodeContainer(nodes.Get(1), nodes.Get(2));
  NodeContainer routerReceiver = NodeContainer(nodes.Get(2), nodes.Get(3));

  PointToPointHelper p2pHighBandwidth;
  p2pHighBandwidth.SetDeviceAttribute("DataRate", DataRateValue(DataRate("1000Mbps")));
  p2pHighBandwidth.SetChannelAttribute("Delay", TimeValue(MilliSeconds(5)));

  PointToPointHelper p2pLowBandwidth;
  p2pLowBandwidth.SetDeviceAttribute("DataRate", DataRateValue(DataRate("10Mbps")));
  p2pLowBandwidth.SetChannelAttribute("Delay", TimeValue(MilliSeconds(10)));

  NetDeviceContainer senderRouterDevices = p2pHighBandwidth.Install(senderRouter);
  NetDeviceContainer routerReceiverDevices = p2pLowBandwidth.Install(routers);
  NetDeviceContainer receiverDevices = p2pHighBandwidth.Install(routerReceiver);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer senderRouterInterfaces = address.Assign(senderRouterDevices);

  address.NewNetwork();
  Ipv4InterfaceContainer routerInterfaces = address.Assign(routerReceiverDevices);

  address.NewNetwork();
  Ipv4InterfaceContainer receiverInterface = address.Assign(receiverDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t sinkPort = 8080;
  Address sinkAddress(InetSocketAddress(receiverInterface.GetAddress(1), sinkPort));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
  ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(3));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(100.0));

  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());

  AsciiTraceHelper asciiTraceHelper;
  Ptr<OutputStreamWrapper> cwndStream = asciiTraceHelper.CreateFileStream("cwnd-trace.txt");
  ns3TcpSocket->TraceConnectWithoutContext("CongestionWindow", MakeBoundCallback(&CwndTracer::TraceCwnd, cwndStream));

  ThroughputTracer throughputTracer(asciiTraceHelper.CreateFileStream("throughput-trace.txt"));
  ns3TcpSocket->TraceConnectWithoutContext("Rx", MakeCallback(&ThroughputTracer::TraceRxBytes, &throughputTracer));

  class MyApp : public Application {
  public:
    MyApp();
    virtual ~MyApp();

    void Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, DataRate dataRate);
  private:
    virtual void StartApplication(void);
    virtual void StopApplication(void);

    void SendPacket(void);

    Ptr<Socket> m_socket;
    Address m_peer;
    uint32_t m_packetSize;
    DataRate m_dataRate;
    EventId m_sendEvent;
    bool m_running;
  };

  MyApp::MyApp()
    : m_socket(0),
      m_peer(),
      m_packetSize(0),
      m_dataRate(),
      m_running(false)
  {
  }

  MyApp::~MyApp()
  {
    m_socket = nullptr;
  }

  void
  MyApp::Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, DataRate dataRate)
  {
    m_socket = socket;
    m_peer = address;
    m_packetSize = packetSize;
    m_dataRate = dataRate;
  }

  void
  MyApp::StartApplication(void)
  {
    m_running = true;
    m_socket->Bind();
    m_socket->Connect(m_peer);
    SendPacket();
  }

  void
  MyApp::StopApplication(void)
  {
    m_running = false;

    if (m_sendEvent.IsRunning()) {
      Simulator::Cancel(m_sendEvent);
    }

    if (m_socket) {
      m_socket->Close();
    }
  }

  void
  MyApp::SendPacket(void)
  {
    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    m_socket->Send(packet);

    if (m_running) {
      Time tNext = Seconds(m_packetSize * 8.0 / m_dataRate.GetBitRate());
      m_sendEvent = Simulator::Schedule(tNext, &MyApp::SendPacket, this);
    }
  }

  Ptr<MyApp> app = CreateObject<MyApp>();
  app->Setup(ns3TcpSocket, sinkAddress, 1024, DataRate("100Mbps"));
  nodes.Get(0)->AddApplication(app);
  app->SetStartTime(Seconds(1.0));
  app->SetStopTime(Seconds(100.0));

  p2pLowBandwidth.EnablePcapAll("bottleneck-link-pcap");

  Ptr<PointToPointNetDevice> bottleneckDev = DynamicCast<PointToPointNetDevice>(routers.Get(0)->GetDevice(1));
  if (bottleneckDev) {
    Ptr<Queue<Packet>> queue = bottleneckDev->GetQueue();
    if (queue) {
      queue->TraceConnectWithoutContext("Enqueue", MakeBoundCallback([](Ptr<const Packet> p) {
        *asciiTraceHelper.CreateFileStream("queue-size-trace.txt")->GetStream() << Simulator::Now().GetSeconds() << " " << queue->GetSize() << std::endl;
      }));
    }
  }

  Simulator::Stop(Seconds(100.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}