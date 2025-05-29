#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wave-mac-helper.h"
#include "ns3/wave-net-device.h"
#include "ns3/ocb-wifi-mac.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VanetRsuExample");

int main (int argc, char *argv[])
{
  bool tracing = false;

  CommandLine cmd;
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);
  cmd.Parse (argc,argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetLogLevel(LogComponent::GetComponent ("UdpClient"), LOG_LEVEL_INFO);
  LogComponent::SetLogLevel(LogComponent::GetComponent ("UdpServer"), LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (2); // One vehicle and one RSU

  // Configure WAVE (802.11p) PHY and MAC
  YansWifiChannelHelper wifiChannelHelper = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper wifiPhyHelper = YansWifiPhyHelper::Default ();
  wifiPhyHelper.SetChannel (wifiChannelHelper.Create ());

  NqosWaveMacHelper waveMacHelper = NqosWaveMacHelper::Default ();
  Wifi80211pHelper wifi80211pHelper = Wifi80211pHelper::Default ();

  NetDeviceContainer devices = wifi80211pHelper.Install (wifiPhyHelper, waveMacHelper, nodes);

  // Configure Mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel"); // RSU
  mobility.Install (nodes.Get (1)); // RSU

  ConstantVelocityMobilityModel vehicleMobility;
  vehicleMobility.SetPosition (Vector (0.0, 0.0, 0.0));
  vehicleMobility.SetVelocity (Vector (10.0, 0.0, 0.0)); // 10 m/s along x axis
  Ptr<Node> vehicleNode = nodes.Get (0);
  vehicleNode->AggregateObject (&vehicleMobility);

  // Install Internet Stack
  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (devices);

  // Configure UDP application: Vehicle (client) sends to RSU (server)
  UdpClientServerHelper udpClientServer (9, i.GetAddress (1)); // port 9, RSU address
  udpClientServer.SetAttribute ("MaxPackets", UintegerValue (1000));
  udpClientServer.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
  udpClientServer.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = udpClientServer.Install (nodes.Get (0));
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (10.0));

  ApplicationContainer serverApps = udpClientServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  if (tracing == true)
    {
      phyHelper.EnablePcapAll ("VanetRsuExample");
    }

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}