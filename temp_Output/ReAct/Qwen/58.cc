#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-header.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NetworkSimulation");

class CwndTracer {
public:
  static void TraceCwnd(Ptr<OutputStreamWrapper> stream, uint32_t oldVal, uint32_t newVal) {
    *stream->GetStream() << Simulator::Now().GetSeconds() << " " << newVal << std::endl;
  }
};

class ThroughputTracer {
public:
  ThroughputTracer()
    : m_lastTime(0), m_lastRx(0) {}

  static void TraceThroughput(Ptr<Socket> socket, ThroughputTracer* thObj, const Address&) {
    thObj->DoTraceThroughput(socket);
  }

  void DoTraceThroughput(Ptr<Socket> socket) {
    Time now = Simulator::Now();
    double diff_s = (now.GetSeconds() - m_lastTime);
    uint64_t rxBytes = socket->GetRx();
    uint64_t diff_bytes = rxBytes - m_lastRx;
    if (diff_s >= 1.0) {
      *m_stream->GetStream() << now.GetSeconds() << " " << (diff_bytes * 8.0 / diff_s) / 1e6 << std::endl;
      m_lastTime = now.GetSeconds();
      m_lastRx = rxBytes;
    }
  }

  void SetStream(Ptr<OutputStreamWrapper> stream) { m_stream = stream; }

private:
  double m_lastTime;
  uint64_t m_lastRx;
  Ptr<OutputStreamWrapper> m_stream;
};

void ScheduleBbrProbeRtt(uint32_t nodeId, uint32_t intervalSec) {
  Config::Set("/NodeList/" + std::to_string(nodeId) + "/$ns3::TcpL4Protocol/SocketType", TypeIdValue(TcpBbr::GetTypeId()));
  Simulator::Schedule(Seconds(intervalSec), &ScheduleBbrProbeRtt, nodeId, intervalSec);
}

int main(int argc, char *argv[]) {
  Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpBbr::GetTypeId()));
  Config::SetDefault("ns3::TcpBbr::EnableProbeRtt", BooleanValue(true));
  Config::SetDefault("ns3::TcpBbr::MinPipeCwnd", UintegerValue(4));

  NodeContainer sender, router1, router2, receiver;
  sender.Create(1);
  router1.Create(1);
  router2.Create(1);
  receiver.Create(1);

  PointToPointHelper p2pSenderRouter1;
  p2pSenderRouter1.SetDeviceAttribute("DataRate", DataRateValue(DataRate("1000Mbps")));
  p2pSenderRouter1.SetChannelAttribute("Delay", TimeValue(MilliSeconds(5)));

  PointToPointHelper p2pRouter1Router2;
  p2pRouter1Router2.SetDeviceAttribute("DataRate", DataRateValue(DataRate("10Mbps")));
  p2pRouter1Router2.SetChannelAttribute("Delay", TimeValue(MilliSeconds(10)));

  PointToPointHelper p2pRouter2Receiver;
  p2pRouter2Receiver.SetDeviceAttribute("DataRate", DataRateValue(DataRate("1000Mbps")));
  p2pRouter2Receiver.SetChannelAttribute("Delay", TimeValue(MilliSeconds(5)));

  NetDeviceContainer devSenderRouter1 = p2pSenderRouter1.Install(sender, router1);
  NetDeviceContainer devRouter1Router2 = p2pRouter1Router2.Install(router1, router2);
  NetDeviceContainer devRouter2Receiver = p2pRouter2Receiver.Install(router2, receiver);

  InternetStackHelper stack;
  stack.InstallAll();

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ifSenderRouter1 = address.Assign(devSenderRouter1);

  address.NewNetwork();
  Ipv4InterfaceContainer ifRouter1Router2 = address.Assign(devRouter1Router2);

  address.NewNetwork();
  Ipv4InterfaceContainer ifRouter2Receiver = address.Assign(devRouter2Receiver);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t sinkPort = 8080;
  Address sinkAddress(InetSocketAddress(ifRouter2Receiver.GetAddress(1), sinkPort));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
  ApplicationContainer sinkApp = packetSinkHelper.Install(receiver);
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(100.0));

  Config::Set("/NodeList/0/$ns3::TcpL4Protocol/SocketType", TypeIdValue(TcpBbr::GetTypeId()));

  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(sender.Get(0), TcpSocketFactory::GetTypeId());

  Ptr<OnOffApplication> app = CreateObject<OnOffApplication>();
  app->SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  app->SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  app->SetAttribute("DataRate", DataRateValue(DataRate("900Mbps")));
  app->SetAttribute("PacketSize", UintegerValue(1448));
  app->SetAttribute("Remote", AddressValue(sinkAddress));
  sender.Get(0)->AddApplication(app);
  app->SetStartTime(Seconds(0.1));
  app->SetStopTime(Seconds(100.0));

  AsciiTraceHelper asciiTraceHelper;
  p2pSenderRouter1.EnablePcapAll("p2p-sender-router1");
  p2pRouter1Router2.EnablePcapAll("p2p-router1-router2");
  p2pRouter2Receiver.EnablePcapAll("p2p-router2-receiver");

  Ptr<OutputStreamWrapper> cwndStream = asciiTraceHelper.CreateFileStream("cwnd-trace.txt");
  Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow", MakeBoundCallback(&CwndTracer::TraceCwnd, cwndStream));

  ThroughputTracer throughputTracer;
  Ptr<OutputStreamWrapper> throughputStream = asciiTraceHelper.CreateFileStream("throughput-trace.txt");
  throughputTracer.SetStream(throughputStream);
  Config::Connect("/NodeList/3/ApplicationList/0/$ns3::PacketSink/Rx", MakeBoundCallback(&ThroughputTracer::TraceThroughput, &throughputTracer));

  Ptr<PointToPointCloudQueueDisc> queue = DynamicCast<PointToPointCloudQueueDisc>(devRouter1Router2.Get(1)->GetQueueDisc());
  if (queue) {
    queue->SetQueueSizeTraceCallback([&](uint32_t size) {
      static double lastLog = 0;
      if (Simulator::Now().GetSeconds() - lastLog >= 0.1) {
        std::ofstream of("queue-size-trace.txt", std::ios_base::app);
        of << Simulator::Now().GetSeconds() << " " << size << std::endl;
        lastLog = Simulator::Now().GetSeconds();
      }
    });
  }

  ScheduleBbrProbeRtt(0, 10);

  Simulator::Stop(Seconds(100));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}