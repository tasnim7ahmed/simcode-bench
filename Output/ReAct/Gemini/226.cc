#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/olsr-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/energy-module.h"
#include "ns3/uinteger.h"
#include "ns3/on-off-application.h"
#include "ns3/udp-client.h"
#include "ns3/udp-server.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WSN_OLSR_UDP");

int main (int argc, char *argv[])
{
  bool enablePcap = false;
  uint32_t numNodes = 10;
  double simulationTime = 50.0;
  double txInterval = 1.0;

  CommandLine cmd;
  cmd.AddValue ("numNodes", "Number of sensor nodes", numNodes);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("txInterval", "interval between packets", txInterval);
  cmd.AddValue ("enablePcap", "Enable pcap tracing", enablePcap);
  cmd.Parse (argc, argv);

  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (numNodes);

  NodeContainer sinkNode;
  sinkNode.Create (1);

  NodeContainer allNodes;
  allNodes.Add (nodes);
  allNodes.Add (sinkNode);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default ();
  Ssid ssid = Ssid ("wsn-network");
  mac.SetType ("ns3::AdhocWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer devices = wifi.Install (phy, mac, allNodes);

  OlsrHelper olsr;
  InternetStackHelper internet;
  internet.SetRoutingHelper (olsr);
  internet.Install (allNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  UdpServerHelper server (9);
  ApplicationContainer serverApps = server.Install (sinkNode.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simulationTime - 1));

  UdpClientHelper client (interfaces.GetAddress (numNodes), 9);
  client.SetAttribute ("MaxPackets", UintegerValue (4294967295));
  client.SetAttribute ("Interval", TimeValue (Seconds (txInterval)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < numNodes; ++i)
    {
      clientApps.Add (client.Install (nodes.Get (i)));
    }
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (simulationTime - 2));

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomDiscPositionAllocator",
                                 "X", StringValue ("50.0"),
                                 "Y", StringValue ("50.0"),
                                 "Rho", StringValue ("ns3::UniformRandomVariable[Min=0|Max=30]"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (allNodes);

  BasicEnergySourceHelper basicSourceHelper;
  basicSourceHelper.Set ("EnergySourceInitialEnergyJ", DoubleValue (1000));
  EnergySourceContainer sources = basicSourceHelper.Install (nodes);

  WifiRadioEnergyModelHelper radioEnergyHelper;
  radioEnergyHelper.Set ("TxCurrentA", DoubleValue (0.0174));
  radioEnergyHelper.Set ("RxCurrentA", DoubleValue (0.012));
  EnergySourceContainer deviceModels = radioEnergyHelper.Install (devices);

  if (enablePcap)
    {
      phy.EnablePcapAll ("wsn_olsr_udp");
    }

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}