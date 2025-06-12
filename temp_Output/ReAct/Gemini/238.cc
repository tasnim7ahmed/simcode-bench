#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mesh-module.h"
#include "ns3/internet-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/udp-client-server-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiMeshMobile");

int main (int argc, char *argv[])
{
  bool verbose = false;
  uint32_t packetSize = 1024;
  std::string dataRate = "1Mbps";
  std::string phyMode ("OfdmRate6Mbps");
  double simulationTime = 10.0;
  double distance = 5.0;
  bool enableFlowMonitor = true;

  CommandLine cmd;
  cmd.AddValue ("packetSize", "size of packets generated", packetSize);
  cmd.AddValue ("dataRate", "Rate at which data is sent", dataRate);
  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("distance", "Distance to travel in meters", distance);
  cmd.AddValue ("verbose", "turn on all WifiNetDevice log components", verbose);
  cmd.AddValue ("enableFlowMonitor", "Enable Flow Monitor", enableFlowMonitor);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    }

  Config::SetDefault ("ns3::WifiMac::BE_MaxAmpduSize", UintegerValue (65535));
  Config::SetDefault ("ns3::WifiMac::BE_MaxAmpduAggregation", BooleanValue (true));
  Config::SetDefault ("ns3::WifiMac::FragmentThreshold", UintegerValue (2346));
  Config::SetDefault ("ns3::WifiMacQueue::MaxPackets", UintegerValue (10000));

  NodeContainer meshNodes;
  meshNodes.Create (2);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211s);

  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
  wifiPhy.SetChannel (wifiChannel.Create ());

  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                  "DataMode",StringValue (phyMode),
                                  "ControlMode",StringValue (phyMode));

  MeshHelper mesh;
  mesh.SetStackInstaller ("ns3::Dot11sStack");
  NetDeviceContainer meshDevices = mesh.Install (wifi, wifiPhy, meshNodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (meshNodes.Get (1));

  Ptr<ConstantPositionMobilityModel> mob = meshNodes.Get (1)->GetObject<ConstantPositionMobilityModel> ();
  Vector3 pos (100, 100, 0);
  mob->SetPosition (pos);

  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.Install (meshNodes.Get (0));

  Ptr<ConstantVelocityMobilityModel> vel = meshNodes.Get (0)->GetObject<ConstantVelocityMobilityModel> ();
  vel->SetVelocity (Vector3D (distance / simulationTime, 0, 0));

  InternetStackHelper internet;
  internet.Install (meshNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (meshDevices);

  UdpServerHelper server (9);
  ApplicationContainer serverApps = server.Install (meshNodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simulationTime));

  UdpClientHelper client (interfaces.GetAddress (1), 9);
  client.SetAttribute ("MaxPackets", UintegerValue ((uint32_t)(simulationTime * (1000000 / (packetSize * 8.0 / StringValue (dataRate).GetDouble ()) ))));
  client.SetAttribute ("Interval", TimeValue (Time::FromDouble (packetSize * 8.0 / StringValue (dataRate).GetDouble (), Time::S)));
  client.SetAttribute ("PacketSize", UintegerValue (packetSize));

  ApplicationContainer clientApps = client.Install (meshNodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (simulationTime));

  if (enableFlowMonitor)
    {
      FlowMonitorHelper flowmon;
      Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

      Simulator::Stop (Seconds (simulationTime));

      Simulator::Run ();

      monitor->CheckForLostPackets ();

      Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
      FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

      for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
        {
          Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
          std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
          std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
          std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
          std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024  << " Mbps\n";
        }

      monitor->SerializeToXmlFile("wifi-mesh-mobile.flowmon", true, true);
    }
  else
    {
      Simulator::Stop (Seconds (simulationTime));
      Simulator::Run ();
    }

  Simulator::Destroy ();
  return 0;
}