#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpEchoLanSimulation");

class QueueLengthTracer {
public:
  QueueLengthTracer(const std::string& filename) {
    m_file.open(filename.c_str());
  }

  ~QueueLengthTracer() {
    if (m_file.is_open()) {
      m_file.close();
    }
  }

  void TraceQueueLength(Ptr<const QueueDisc> queue, uint32_t oldValue, uint32_t newValue) {
    m_file << Simulator::Now().GetSeconds() << " " << newValue << std::endl;
  }

private:
  std::ofstream m_file;
};

int main(int argc, char* argv[]) {
  bool useIpv6 = false;

  CommandLine cmd(__FILE__);
  cmd.AddValue("useIpv6", "Use IPv6 instead of IPv4", useIpv6);
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(4);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices[6];
  devices[0] = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
  devices[1] = pointToPoint.Install(nodes.Get(0), nodes.Get(2));
  devices[2] = pointToPoint.Install(nodes.Get(0), nodes.Get(3));
  devices[3] = pointToPoint.Install(nodes.Get(1), nodes.Get(2));
  devices[4] = pointToPoint.Install(nodes.Get(1), nodes.Get(3));
  devices[5] = pointToPoint.Install(nodes.Get(2), nodes.Get(3));

  TrafficControlHelper tch;
  tch.SetRootQueueDisc("ns3::DropTailQueueDisc");
  for (auto& dev : devices) {
    tch.Install(dev);
  }

  InternetStackHelper stack;
  if (useIpv6) {
    stack.SetIpv4StackEnabled(false);
    stack.SetIpv6StackEnabled(true);
  } else {
    stack.SetIpv4StackEnabled(true);
    stack.SetIpv6StackEnabled(false);
  }
  stack.Install(nodes);

  Ipv4AddressHelper address;
  Ipv4InterfaceContainer interfaces[6];
  for (int i = 0; i < 6; ++i) {
    std::ostringstream subnet;
    subnet << "10.1." << i << ".0";
    address.SetBase(subnet.str().c_str(), "255.255.255.0");
    interfaces[i] = address.Assign(devices[i]);
  }

  if (!useIpv6) {
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
  }

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(interfaces[0].GetAddress(1), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(5));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  AsciiTraceHelper ascii;
  pointToPoint.EnableAsciiAll(ascii.CreateFileStream("udp-echo.tr"));

  QueueLengthTracer queueTracer("queue-length.tr");
  Config::Connect("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/TxQueue/QueueDisc", 
                  MakeCallback(&QueueLengthTracer::TraceQueueLength, &queueTracer));

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}