#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/wave-mac-helper.h"
#include "ns3/wave-net-device.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VanetSimulation");

int main (int argc, char *argv[])
{
  bool enableFlowMonitor = true;
  std::string phyMode ("OfdmRate6MbpsBW10MHz");
  double simulationTime = 10; //seconds
  double distance = 50; //meters
  uint32_t packetSize = 1024; //bytes
  uint16_t port = 9; // port number
  double interPacketInterval = 0.1; //seconds

  CommandLine cmd;
  cmd.AddValue ("EnableMonitor", "Enable Flow Monitor", enableFlowMonitor);
  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("distance", "Distance in meters", distance);
  cmd.Parse (argc, argv);

  Config::SetDefault ("ns3::Wifi80211pPhy::ChannelNumber", UintegerValue (180));
  Config::SetDefault ("ns3::Wifi80211pPhy::TxPowerStart", DoubleValue (23));
  Config::SetDefault ("ns3::Wifi80211pPhy::TxPowerEnd", DoubleValue (23));

  NodeContainer nodes;
  nodes.Create (5);

  // Set up mobility model
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (distance),
                                 "DeltaY", DoubleValue (0.0),
                                 "GridWidth", UintegerValue (5),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.Install (nodes);

  for (uint32_t i = 0; i < nodes.GetN (); ++i)
    {
      Ptr<ConstantVelocityMobilityModel> vm = nodes.Get (i)->GetObject<ConstantVelocityMobilityModel> ();
      vm->SetVelocity (Vector (15 + i*2, 0, 0)); // Varying speeds
    }

  // Configure 802.11p devices
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  Wifi80211pHelper wifi80211pHelper = Wifi80211pHelper::Default ();
  wifi80211pHelper.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                              "DataMode", StringValue (phyMode),
                                              "ControlMode", StringValue (phyMode));

  WaveMacHelper waveMacHelper = WaveMacHelper::Default ();

  NetDeviceContainer devices = wifi80211pHelper.Install (wifiPhy, waveMacHelper, nodes);

  // Install TCP/IP stack
  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (devices);

  // Create UDP server on node 0
  UdpServerHelper server (port);
  ApplicationContainer apps = server.Install (nodes.Get (0));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (simulationTime));

  // Create UDP client on nodes 1-4
  UdpClientHelper client (i.GetAddress (0), port);
  client.SetAttribute ("MaxPackets", UintegerValue (4294967295));
  client.SetAttribute ("Interval", TimeValue (Seconds (interPacketInterval)));
  client.SetAttribute ("PacketSize", UintegerValue (packetSize));

  for (uint32_t j = 1; j < nodes.GetN (); ++j)
    {
      ApplicationContainer clientApps = client.Install (nodes.Get (j));
      clientApps.Start (Seconds (2.0));
      clientApps.Stop (Seconds (simulationTime));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Flow monitor
  FlowMonitorHelper flowmon;
  if (enableFlowMonitor)
    {
      flowmon.InstallAll ();
    }

  // Set up NetAnim
  AnimationInterface anim ("vanet.xml");
  for (uint32_t k = 0; k < nodes.GetN (); ++k)
    {
      anim.UpdateNodeColor(nodes.Get(k), 0, 0, 255);
    }

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();

  if (enableFlowMonitor)
    {
      flowmon.SerializeToXmlFile("vanet.flowmon", false, true);
    }

  Simulator::Destroy ();
  return 0;
}