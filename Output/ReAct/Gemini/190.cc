#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/uinteger.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetSimulation");

int main(int argc, char *argv[]) {
  bool verbose = false;
  bool tracing = false;
  uint32_t nNodes = 3;
  double simulationTime = 10;

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell application to log if set to true", verbose);
  cmd.AddValue("tracing", "Enable pcap tracing", tracing);
  cmd.AddValue("nNodes", "Number of nodes", nNodes);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.Parse(argc, argv);

  if (verbose) {
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
  }

  NodeContainer nodes;
  nodes.Create(nNodes);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns-3-adhoc");
  mac.SetType("ns3::AdhocWifiMac", "Ssid", SsidValue(ssid));

  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                "X", StringValue("0"),
                                "Y", StringValue("0"),
                                "Rho", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
  mobility.SetMobilityModel("ns3::RandomDirection2dMobilityModel",
                             "Speed", StringValue("ns3::ConstantRandomVariable[Constant=10]"),
                             "Bounds", StringValue("50x50"));
  mobility.Install(nodes);

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  UdpServerHelper server(9);
  ApplicationContainer serverApps = server.Install(nodes.Get(nNodes - 1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(simulationTime));

  UdpClientHelper client(interfaces.GetAddress(nNodes - 1), 9);
  client.SetAttribute("MaxPackets", UintegerValue(1000));
  client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
  client.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = client.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(simulationTime));

  if (tracing) {
    phy.EnablePcapAll("manet-simulation");
  }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  AnimationInterface anim("manet-animation.xml");
  anim.SetMaxPktsPerTraceFile(1000000);
  for (uint32_t i = 0; i < nNodes; ++i)
    anim.UpdateNodeDescription(nodes.Get(i), "Node");
    anim.UpdateNodeSize(nodes.Get(0), 10, 10);

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();

  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
    std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
    std::cout << "  Packet Loss: " << (double)(i->second.txPackets - i->second.rxPackets) / (double)i->second.txPackets * 100 << "%\n";
    std::cout << "  Delay Sum: " << i->second.delaySum << "\n";
  }

  Simulator::Destroy();
  return 0;
}