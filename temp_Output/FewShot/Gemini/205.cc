#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(4);

  // Mobility model
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

  for (uint32_t i = 0; i < nodes.GetN(); ++i) {
    Ptr<ConstantVelocityMobilityModel> mob =
        nodes.Get(i)->GetObject<ConstantVelocityMobilityModel>();
    mob->SetVelocity(Vector(10, 0, 0)); // 10 m/s along the x-axis
  }

  // Wifi setup
  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  mac.SetType("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  // Internet stack
  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // UDP application (V2V communication)
  uint16_t port = 9;

  for (uint32_t i = 0; i < nodes.GetN(); ++i) {
    for (uint32_t j = 0; j < nodes.GetN(); ++j) {
      if (i != j) {
        UdpClientHelper client(interfaces.GetAddress(j), port);
        client.SetAttribute("MaxPackets", UintegerValue(1000));
        client.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
        client.SetAttribute("PacketSize", UintegerValue(1472));

        ApplicationContainer clientApps = client.Install(nodes.Get(i));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));
      }
    }

    UdpServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(nodes.Get(i));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));
  }

  // NetAnim
  AnimationInterface anim("v2v_netanim.xml");
  for (uint32_t i = 0; i < nodes.GetN(); ++i) {
      anim.UpdateNodeDescription(nodes.Get(i), "Vehicle " + std::to_string(i));
      anim.UpdateNodeSize(nodes.Get(i), 5.0, 5.0);
  }
  anim.EnablePacketMetadata(true);

  // Flow monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Run simulation
  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  // Print flow statistics
  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier =
      DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i =
           stats.begin();
       i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> "
              << t.destinationAddress << ")\n";
    std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
    std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
    std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
    std::cout << "  Packet Delivery Ratio: "
              << double(i->second.rxPackets) / i->second.txPackets << "\n";
    std::cout << "  Delay Sum: " << i->second.delaySum << "\n";
    std::cout << "  Jitter Sum: " << i->second.jitterSum << "\n";
  }

  Simulator::Destroy();
  return 0;
}