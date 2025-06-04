#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VanetExample");

int main (int argc, char *argv[])
{
  LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

  // Create nodes (5 vehicles)
  NodeContainer vehicles;
  vehicles.Create(5);

  // Set up 802.11p Wi-Fi (vehicular communication)
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211p);

  NqosWaveMacHelper mac = NqosWaveMacHelper::Default();
  Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default();
  NetDeviceContainer devices = wifi80211p.Install(phy, mac, vehicles);

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install(vehicles);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer vehicleInterfaces = ipv4.Assign(devices);

  // Set up mobility model for vehicles
  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0.0, 0.0, 0.0));    // First vehicle position
  positionAlloc->Add(Vector(20.0, 0.0, 0.0));   // Second vehicle
  positionAlloc->Add(Vector(40.0, 0.0, 0.0));   // Third vehicle
  positionAlloc->Add(Vector(60.0, 0.0, 0.0));   // Fourth vehicle
  positionAlloc->Add(Vector(80.0, 0.0, 0.0));   // Fifth vehicle
  mobility.SetPositionAllocator(positionAlloc);
  mobility.Install(vehicles);

  // Set different speeds for the vehicles
  for (uint32_t i = 0; i < vehicles.GetN(); ++i) {
    Ptr<ConstantVelocityMobilityModel> mob = vehicles.Get(i)->GetObject<ConstantVelocityMobilityModel>();
    mob->SetVelocity(Vector(10.0 + i, 0.0, 0.0));  // Speed varies slightly for each vehicle
  }

  // Set up UDP echo server on the first vehicle
  uint16_t port = 9;
  UdpEchoServerHelper echoServer(port);
  ApplicationContainer serverApp = echoServer.Install(vehicles.Get(0));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(20.0));

  // Set up UDP echo clients on the other vehicles
  UdpEchoClientHelper echoClient(vehicleInterfaces.GetAddress(0), port);
  echoClient.SetAttribute("MaxPackets", UintegerValue(10));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(2.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps;
  for (uint32_t i = 1; i < vehicles.GetN(); ++i)
  {
    clientApps.Add(echoClient.Install(vehicles.Get(i)));
  }
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(20.0));

  // Enable tracing for Wi-Fi packets
  phy.EnablePcapAll("vanet-example");

  // Set up FlowMonitor for performance metrics
  FlowMonitorHelper flowMonitorHelper;
  Ptr<FlowMonitor> flowMonitor = flowMonitorHelper.InstallAll();

  // Set up NetAnim for visualization
  AnimationInterface anim("vanet-animation.xml");
  anim.SetConstantPosition(vehicles.Get(0), 0.0, 0.0);
  anim.SetConstantPosition(vehicles.Get(1), 20.0, 0.0);
  anim.SetConstantPosition(vehicles.Get(2), 40.0, 0.0);
  anim.SetConstantPosition(vehicles.Get(3), 60.0, 0.0);
  anim.SetConstantPosition(vehicles.Get(4), 80.0, 0.0);

  // Run the simulation
  Simulator::Stop(Seconds(20.0));
  Simulator::Run();

  // FlowMonitor statistics
  flowMonitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitorHelper.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();

  for (auto it = stats.begin(); it != stats.end(); ++it)
  {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(it->first);
    std::cout << "Flow from " << t.sourceAddress << " to " << t.destinationAddress << std::endl;
    std::cout << "  Tx Packets: " << it->second.txPackets << std::endl;
    std::cout << "  Rx Packets: " << it->second.rxPackets << std::endl;
    std::cout << "  Throughput: " << it->second.rxBytes * 8.0 / 20.0 / 1000 / 1000 << " Mbps" << std::endl;
    std::cout << "  End-to-End Delay: " << it->second.delaySum.GetSeconds() << " seconds" << std::endl;
  }

  // Clean up and exit
  Simulator::Destroy();
  return 0;
}

