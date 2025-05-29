#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VANET-Dsdv-Example");

int main (int argc, char *argv[])
{
  uint32_t numVehicles = 10;
  double simTime = 60.0;
  double distance = 30.0;
  double nodeSpeed = 15.0;
  uint32_t pktSize = 512;
  double pktInterval = 0.5;

  CommandLine cmd;
  cmd.AddValue ("numVehicles", "Number of vehicles", numVehicles);
  cmd.AddValue ("simTime", "Simulation time (s)", simTime);
  cmd.AddValue ("distance", "Distance between vehicles (m)", distance);
  cmd.AddValue ("nodeSpeed", "Speed of vehicles (m/s)", nodeSpeed);
  cmd.AddValue ("pktSize", "Packet size (bytes)", pktSize);
  cmd.AddValue ("pktInterval", "Packet sending interval (s)", pktInterval);
  cmd.Parse (argc, argv);

  NodeContainer vehicles;
  vehicles.Create (numVehicles);

  // Mobility: Constant velocity along X axis (straight road)
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> posAllocator = CreateObject<ListPositionAllocator> ();
  for (uint32_t i = 0; i < numVehicles; ++i)
    posAllocator->Add (Vector (i * distance, 0, 0));
  mobility.SetPositionAllocator (posAllocator);

  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.Install (vehicles);

  for (uint32_t i = 0; i < numVehicles; ++i)
    {
      Ptr<ConstantVelocityMobilityModel> vel = vehicles.Get (i)->GetObject<ConstantVelocityMobilityModel> ();
      vel->SetVelocity (Vector (nodeSpeed, 0, 0));
    }

  // WiFi: 802.11p for vehicular (WAVE)
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  NqosWaveMacHelper wifiMac = NqosWaveMacHelper::Default ();
  Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default ();
  wifi80211p.SetRemoteStationManager ("ns3::ConstantRateWifiManager", 
                                      "DataMode", StringValue ("OfdmRate6MbpsBW10MHz"),
                                      "ControlMode", StringValue ("OfdmRate6MbpsBW10MHz"));

  NetDeviceContainer devices = wifi80211p.Install (wifiPhy, wifiMac, vehicles);

  // Internet and DSDV
  DsdvHelper dsdv;
  InternetStackHelper internet;
  internet.SetRoutingHelper (dsdv);
  internet.Install (vehicles);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.0.0", "255.255.0.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // Applications: UDP traffic from first to last vehicle
  uint16_t port = 5000;
  OnOffHelper onoff ("ns3::UdpSocketFactory", 
                     InetSocketAddress (interfaces.GetAddress (numVehicles - 1), port));
  onoff.SetAttribute ("DataRate", StringValue ("2Mbps"));
  onoff.SetAttribute ("PacketSize", UintegerValue (pktSize));
  onoff.SetAttribute ("Interval", TimeValue (Seconds (pktInterval)));
  onoff.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
  onoff.SetAttribute ("StopTime", TimeValue (Seconds (simTime)));

  ApplicationContainer app = onoff.Install (vehicles.Get (0));

  PacketSinkHelper sink ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApp = sink.Install (vehicles.Get (numVehicles - 1));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (simTime));

  // Enable PCAP tracing
  wifiPhy.EnablePcapAll ("vanet-dsdv");

  // Flow monitoring
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  uint32_t rxPackets = 0;
  uint32_t txPackets = 0;
  for (auto it = stats.begin (); it != stats.end (); ++it)
    {
      txPackets += it->second.txPackets;
      rxPackets += it->second.rxPackets;
    }
  std::cout << "Tx Packets: " << txPackets << std::endl;
  std::cout << "Rx Packets: " << rxPackets << std::endl;
  std::cout << "Packet Delivery Ratio: "
            << ((txPackets > 0) ? (100.0 * rxPackets / txPackets) : 0) << " %" << std::endl;

  Simulator::Destroy ();
  return 0;
}