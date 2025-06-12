#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("FourNodeReroutingSimulation");

void
PacketReceiveCallback (std::string context, Ptr<const Packet> p, const Address &addr)
{
  static std::ofstream recvStream;
  static bool streamOpened = false;
  if (!streamOpened)
    {
      recvStream.open ("packet-receive.log");
      streamOpened = true;
    }
  recvStream << Simulator::Now ().GetSeconds () << "s " << context
             << " Received " << p->GetSize () << " bytes" << std::endl;
}

int
main (int argc, char *argv[])
{
  std::string n1n3Metric = "10";
  CommandLine cmd;
  cmd.AddValue ("n1n3Metric", "Routing metric (cost) for the n1<->n3 link", n1n3Metric);
  cmd.Parse (argc, argv);

  // Create nodes
  NodeContainer n0, n1, n2, n3;
  n0.Create(1);
  n1.Create(1);
  n2.Create(1);
  n3.Create(1);

  // Name nodes
  Names::Add("n0", n0.Get(0));
  Names::Add("n1", n1.Get(0));
  Names::Add("n2", n2.Get(0));
  Names::Add("n3", n3.Get(0));

  // Set up node containers for links
  NodeContainer n0n2(n0.Get(0), n2.Get(0));
  NodeContainer n1n2(n1.Get(0), n2.Get(0));
  NodeContainer n1n3(n1.Get(0), n3.Get(0));
  NodeContainer n2n3(n2.Get(0), n3.Get(0));

  // Configure PointToPoint helpers
  PointToPointHelper p2p_5Mbps_2ms;
  p2p_5Mbps_2ms.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p_5Mbps_2ms.SetChannelAttribute("Delay", StringValue("2ms"));

  PointToPointHelper p2p_1_5Mbps_100ms;
  p2p_1_5Mbps_100ms.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
  p2p_1_5Mbps_100ms.SetChannelAttribute("Delay", StringValue("100ms"));

  PointToPointHelper p2p_1_5Mbps_10ms;
  p2p_1_5Mbps_10ms.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
  p2p_1_5Mbps_10ms.SetChannelAttribute("Delay", StringValue("10ms"));

  // Install NetDevices
  NetDeviceContainer ndc_n0n2 = p2p_5Mbps_2ms.Install(n0n2);
  NetDeviceContainer ndc_n1n2 = p2p_5Mbps_2ms.Install(n1n2);
  NetDeviceContainer ndc_n1n3 = p2p_1_5Mbps_100ms.Install(n1n3);
  NetDeviceContainer ndc_n2n3 = p2p_1_5Mbps_10ms.Install(n2n3);

  // Install Internet stack and enable OSPF-like routing (RIP supports metrics, but OLSR does not)
  InternetStackHelper internet;
  Ipv4ListRoutingHelper listRH;
  RipHelper ripRouting;
  ripRouting.ExcludeInterface(n0.Get(0), 1); // n0:n0n2; only one interface so index 1 safe
  listRH.Add(ripRouting, 0);
  internet.SetRoutingHelper(listRH);
  internet.Install(NodeContainer(n0, n1, n2, n3));

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer if_n0n2 = ipv4.Assign(ndc_n0n2);

  ipv4.SetBase("10.0.1.0", "255.255.255.0");
  Ipv4InterfaceContainer if_n1n2 = ipv4.Assign(ndc_n1n2);

  ipv4.SetBase("10.0.2.0", "255.255.255.0");
  Ipv4InterfaceContainer if_n1n3 = ipv4.Assign(ndc_n1n3);

  ipv4.SetBase("10.0.3.0", "255.255.255.0");
  Ipv4InterfaceContainer if_n2n3 = ipv4.Assign(ndc_n2n3);

  // Set routing metric on n1's interface toward n3
  Ptr<Ipv4> ipv4_n1 = n1.Get(0)->GetObject<Ipv4>();
  uint32_t ifIndex_n1n3 = 2; // n1: eth0(n1-n2), eth1(n1-n3)
  ipv4_n1->SetMetric(ifIndex_n1n3, std::stoi(n1n3Metric));

  // Enable global routing table print
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Enable packet queue tracing
  AsciiTraceHelper ascii;
  p2p_5Mbps_2ms.EnableAsciiAll(ascii.CreateFileStream("queues-trace.log"));
  p2p_1_5Mbps_100ms.EnableAsciiAll(ascii.CreateFileStream("queues-trace.log"));
  p2p_1_5Mbps_10ms.EnableAsciiAll(ascii.CreateFileStream("queues-trace.log"));

  // Install UDP server on n1
  uint16_t udpPort = 4000;
  UdpServerHelper udpServer (udpPort);
  ApplicationContainer serverApps = udpServer.Install(n1.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(12.0));

  // Install UDP client on n3 sending to n1
  UdpClientHelper udpClient (if_n1n2.GetAddress(0), udpPort); // n1 IP on n1n2 link
  udpClient.SetAttribute("MaxPackets", UintegerValue(320));
  udpClient.SetAttribute("Interval", TimeValue(Seconds(0.05)));
  udpClient.SetAttribute("PacketSize", UintegerValue(512));
  ApplicationContainer clientApps = udpClient.Install(n3.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(12.0));

  // Connect receive trace
  Config::Connect("/NodeList/1/ApplicationList/0/$ns3::UdpServer/Rx", MakeCallback(&PacketReceiveCallback));

  // Flow Monitor
  Ptr<FlowMonitor> flowMonitor;
  FlowMonitorHelper flowHelper;
  flowMonitor = flowHelper.InstallAll();

  // Run simulation
  Simulator::Stop(Seconds(13.0));
  Simulator::Run();

  // Write flow monitor statistics
  flowMonitor->SerializeToXmlFile("flowmon-results.xml", true, true);

  // Log UDP Server stats
  Ptr<UdpServer> server = DynamicCast<UdpServer>(serverApps.Get(0));
  uint32_t totalRx = server->GetReceived();
  std::ofstream resultFile;
  resultFile.open ("simulation-summary.log");
  resultFile << "UDP Packets received by n1: " << totalRx << std::endl;
  resultFile.close();

  Simulator::Destroy();
  return 0;
}