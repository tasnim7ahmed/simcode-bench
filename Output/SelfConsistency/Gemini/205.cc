#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VehicularNetworkSimulation");

int main(int argc, char *argv[]) {

  // Enable command line arguments parsing
  CommandLine cmd;
  cmd.Parse(argc, argv);

  // Set simulation time and packet interval
  double simulationTime = 10.0;
  Time packetInterval = MilliSeconds(100);

  // Create nodes (vehicles)
  NodeContainer nodes;
  nodes.Create(4);

  // Configure mobility model
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(100.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(4),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  mobility.Install(nodes);

  // Set initial velocity for each node
  for (uint32_t i = 0; i < nodes.GetN(); ++i) {
    Ptr<ConstantVelocityMobilityModel> mobilityModel =
        nodes.Get(i)->GetObject<ConstantVelocityMobilityModel>();
    mobilityModel->SetVelocity(Vector(10, 0, 0)); // 10 m/s in X direction
  }

  // Configure WiFi
  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  mac.SetType("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Create UDP applications (send packets between nodes)
  uint16_t port = 9; // Discard port
  for (uint32_t i = 0; i < nodes.GetN(); ++i) {
    for (uint32_t j = 0; j < nodes.GetN(); ++j) {
      if (i != j) {
        // Create receiver application on node j
        UdpServerHelper server(port);
        ApplicationContainer serverApps = server.Install(nodes.Get(j));
        serverApps.Start(Seconds(0.0));
        serverApps.Stop(Seconds(simulationTime));

        // Create sender application on node i, sending to node j
        UdpClientHelper client(interfaces.GetAddress(j), port);
        client.SetAttribute("MaxPackets", UintegerValue(1000000));
        client.SetAttribute("Interval", TimeValue(packetInterval));
        client.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApps = client.Install(nodes.Get(i));
        clientApps.Start(Seconds(1.0));
        clientApps.Stop(Seconds(simulationTime));
      }
    }
  }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Enable FlowMonitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Enable NetAnim
  AnimationInterface anim("vehicular-network.xml");
  anim.SetConstantPosition(nodes.Get(0), 0.0, 0.0);
  anim.SetConstantPosition(nodes.Get(1), 100.0, 0.0);
  anim.SetConstantPosition(nodes.Get(2), 200.0, 0.0);
  anim.SetConstantPosition(nodes.Get(3), 300.0, 0.0);

  // Run the simulation
  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();

  // Analyze the results
  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier =
      DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i =
           stats.begin();
       i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> "
              << t.destinationAddress << ")\n";
    std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
    std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
    std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
    std::cout << "  Delay Sum: " << i->second.delaySum << "\n";
  }

  Simulator::Destroy();

  return 0;
}