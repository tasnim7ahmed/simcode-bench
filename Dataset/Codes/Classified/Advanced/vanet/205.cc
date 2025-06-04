#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VehicularNetworkSimulation");

int main (int argc, char *argv[])
{
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  // Create nodes (vehicles)
  NodeContainer vehicles;
  vehicles.Create (4);

  // Set up WiFi for ad-hoc communication
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211g);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, vehicles);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install (vehicles);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer vehicleInterfaces = ipv4.Assign (devices);

  // Set up mobility model for vehicles (straight road with constant velocity)
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (50.0),
                                 "DeltaY", DoubleValue (0.0),
                                 "GridWidth", UintegerValue (4),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.Install (vehicles);

  for (uint32_t i = 0; i < vehicles.GetN (); ++i)
    {
      Ptr<ConstantVelocityMobilityModel> mobility = vehicles.Get (i)->GetObject<ConstantVelocityMobilityModel> ();
      mobility->SetVelocity (Vector (20.0, 0.0, 0.0)); // 20 m/s constant speed
    }

  // Set up UDP server on vehicle 0
  uint16_t port = 9;
  UdpEchoServerHelper echoServer (port);
  ApplicationContainer serverApp = echoServer.Install (vehicles.Get (0));
  serverApp.Start (Seconds (1.0));
  serverApp.Stop (Seconds (10.0));

  // Set up UDP clients on vehicles 1, 2, and 3
  UdpEchoClientHelper echoClient (vehicleInterfaces.GetAddress (0), port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (10));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps;
  for (uint32_t i = 1; i < vehicles.GetN (); ++i)
    {
      clientApps.Add (echoClient.Install (vehicles.Get (i)));
    }
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Enable pcap tracing
  wifiPhy.EnablePcap ("vehicular-network", devices);

  // FlowMonitor for performance analysis
  FlowMonitorHelper flowMonitorHelper;
  Ptr<FlowMonitor> flowMonitor = flowMonitorHelper.InstallAll ();

  // NetAnim for visualization
  AnimationInterface anim ("vehicular-network-animation.xml");
  anim.SetConstantPosition (vehicles.Get (0), 0.0, 0.0);
  anim.SetConstantPosition (vehicles.Get (1), 50.0, 0.0);
  anim.SetConstantPosition (vehicles.Get (2), 100.0, 0.0);
  anim.SetConstantPosition (vehicles.Get (3), 150.0, 0.0);

  // Run the simulation
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  // Analyze flow statistics
  flowMonitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowMonitorHelper.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats ();

  for (auto it = stats.begin (); it != stats.end (); ++it)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (it->first);
      std::cout << "Flow from " << t.sourceAddress << " to " << t.destinationAddress << std::endl;
      std::cout << "  Tx Packets: " << it->second.txPackets << std::endl;
      std::cout << "  Rx Packets: " << it->second.rxPackets << std::endl;
      std::cout << "  Delay: " << it->second.delaySum.GetSeconds () << " seconds" << std::endl;
    }

  Simulator::Destroy ();
  return 0;
}

