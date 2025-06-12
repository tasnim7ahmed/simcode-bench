#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer vehicles;
  vehicles.Create(5);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                "X", StringValue("0"),
                                "Y", StringValue("0"),
                                "Z", StringValue("0"),
                                "Rho", StringValue("ns3::UniformRandomVariable[Min=0|Max=1]"));
  mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  mobility.Install(vehicles);

  for (uint32_t i = 0; i < vehicles.GetN(); ++i) {
    Ptr<ConstantVelocityMobilityModel> mob = vehicles.Get(i)->GetObject<ConstantVelocityMobilityModel>();
    mob->SetVelocity(Vector(10 + i*2, 0, 0)); // Varying speeds
  }

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default();
  Ssid ssid = Ssid("vanet-ssid");
  wifiMac.SetType("ns3::AdhocWifiMac", "Ssid", SsidValue(ssid));

  WifiHelper wifi;
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue("OfdmRate6MbpsBW10MHz"),
                               "RtsCtsThreshold", UintegerValue(0));

  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, vehicles);

  InternetStackHelper internet;
  internet.Install(vehicles);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

  uint16_t port = 9;
  UdpEchoServerHelper echoServer(port);
  ApplicationContainer serverApp = echoServer.Install(vehicles.Get(0));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(interfaces.GetAddress(0), port);
  echoClient.SetAttribute("MaxPackets", UintegerValue(100));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps;
  for (uint32_t i = 1; i < vehicles.GetN(); ++i) {
    clientApps.Add(echoClient.Install(vehicles.Get(i)));
  }
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(11.0));

  AnimationInterface anim("vanet.xml");
  for (uint32_t i = 0; i < vehicles.GetN(); ++i) {
        anim.UpdateNodeDescription (vehicles.Get(i), "Vehicle");
        anim.UpdateNodeColor (vehicles.Get(i), 255, 0, 0);
  }

  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
    std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
    std::cout << "  Packet Loss Ratio: " << ((double)(i->second.txPackets - i->second.rxPackets)) / ((double)i->second.txPackets) << "\n";
    std::cout << "  End-to-End Delay: " << i->second.delaySum << "\n";
  }

  Simulator::Destroy();
  return 0;
}