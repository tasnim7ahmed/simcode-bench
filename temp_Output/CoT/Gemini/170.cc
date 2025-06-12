#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiTcpExample");

int main (int argc, char *argv[])
{
  bool verbose = false;
  bool tracing = true;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);
  cmd.Parse (argc,argv);

  if (verbose)
    {
      LogComponentEnable ("WifiTcpExample", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

  NodeContainer staNodes;
  staNodes.Create (2);
  NodeContainer apNode;
  apNode.Create (1);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns-3-ssid");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, staNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid),
               "BeaconInterval", TimeValue (Seconds (0.1)));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, apNode);

  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (staNodes);
  mobility.Install (apNode);

  InternetStackHelper internet;
  internet.Install (apNode);
  internet.Install (staNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces;
  apInterfaces = address.Assign (apDevices);
  Ipv4InterfaceContainer staInterfaces;
  staInterfaces = address.Assign (staDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 50000;
  Address serverAddress (InetSocketAddress (apInterfaces.GetAddress (0), port));

  TcpClientHelper clientHelper (apInterfaces.GetAddress (0), port);
  clientHelper.SetAttribute ("MaxPackets", UintegerValue (1000000));
  clientHelper.SetAttribute ("Interval", TimeValue (MicroSeconds (50)));
  clientHelper.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = clientHelper.Install (staNodes.Get (0));
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (10.0));

  TcpServerHelper serverHelper (port);
  ApplicationContainer serverApps = serverHelper.Install (apNode.Get (0));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (10.0));

  if (tracing == true)
    {
      phy.EnablePcap ("wifi-tcp-example", apDevices);
    }

  Simulator::Stop (Seconds (10.0));

  Simulator::Run ();

  uint64_t totalRxBytes = DynamicCast<TcpServer>(serverApps.Get(0))->GetReceivedBytes();
  double throughput = (totalRxBytes * 8.0) / (10.0 * 1000000);

  std::cout << "Throughput: " << throughput << " Mbps" << std::endl;
  std::cout << "Received Packets: " << DynamicCast<TcpServer>(serverApps.Get(0))->GetReceivedPackets() << std::endl;
  std::cout << "Sent Packets: " << DynamicCast<TcpClient>(clientApps.Get(0))->GetSentPackets() << std::endl;
  Simulator::Destroy ();
  return 0;
}