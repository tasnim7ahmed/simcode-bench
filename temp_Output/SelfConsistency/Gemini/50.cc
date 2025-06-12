#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/spectrum-wifi-helper.h"
#include "ns3/command-line.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/string.h"
#include "ns3/names.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/error-model.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMultiMCS");

int main(int argc, char *argv[]) {
  bool verbose = false;
  double simulationTime = 10.0;
  double distance = 10.0;
  std::string wifiStandard = "802.11ax";
  std::string errorModelType = "NistErrorRateModel";

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell application to log if set to true", verbose);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("distance", "Distance between nodes in meters", distance);
  cmd.AddValue("wifiStandard", "Wifi standard (e.g., 802.11ax)", wifiStandard);
  cmd.AddValue("errorModelType", "Error model type (e.g., NistErrorRateModel)", errorModelType);
  cmd.Parse(argc, argv);

  if (verbose) {
    LogComponentEnable("WifiMultiMCS", LOG_LEVEL_INFO);
  }

  NodeContainer nodes;
  nodes.Create(2);

  NodeContainer staNodes = NodeContainer(nodes.Get(0));
  NodeContainer apNodes = NodeContainer(nodes.Get(1));

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(wifiStandard);

  WifiMacHelper mac;

  Ssid ssid = Ssid("ns-3-ssid");
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices = wifi.Install(phy, mac, staNodes);

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));

  NetDeviceContainer apDevices = wifi.Install(phy, mac, apNodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(distance),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staNodeIface = address.Assign(staDevices);
  Ipv4InterfaceContainer apNodeIface = address.Assign(apDevices);

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(apNodes);
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(simulationTime));

  UdpEchoClientHelper echoClient(apNodeIface.GetAddress(0), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(4294967295));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = echoClient.Install(staNodes);
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(simulationTime));


  Ptr<Node> apNode = apNodes.Get(0);
  Ptr<Ipv4> ipv4 = apNode->GetObject<Ipv4>();
  Ipv4InterfaceAddress iaddr = ipv4->GetAddress(1, 0);
  Ipv4Address ipAddr = iaddr.GetLocal();

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(simulationTime));

  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
  {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);

    if (t.destinationAddress == ipAddr)
    {
      std::cout << "Flow ID: " << i->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress << std::endl;
      std::cout << "  Tx Packets: " << i->second.txPackets << std::endl;
      std::cout << "  Rx Packets: " << i->second.rxPackets << std::endl;
      std::cout << "  Lost Packets: " << i->second.lostPackets << std::endl;
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps" << std::endl;
    }
  }

  Simulator::Destroy();

  return 0;
}