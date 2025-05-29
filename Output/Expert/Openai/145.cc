#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mesh-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/packet-sink.h"
#include "ns3/pcap-file-wrapper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MeshExampleStats");

uint32_t g_txPackets = 0, g_rxPackets = 0;

void
TxTrace (Ptr<const Packet> packet)
{
  g_txPackets++;
}

void
RxTrace (Ptr<const Packet> packet, const Address &from)
{
  g_rxPackets++;
}

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  // Create three mesh nodes
  NodeContainer meshNodes;
  meshNodes.Create (3);

  // Install Wifi Mesh on all nodes
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());
  MeshHelper mesh;
  mesh = MeshHelper::Default ();
  mesh.SetStackInstaller ("ns3::Dot11sStack");
  mesh.SetSpreadInterfaceChannels (MeshHelper::SPREAD_CHANNELS);
  mesh.SetMacType ("RandomStart", TimeValue (Seconds (0.1)));
  NetDeviceContainer meshDevices = mesh.Install (wifiPhy, meshNodes);

  // Enable PCAP
  wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
  wifiPhy.EnablePcap ("mesh", meshDevices);

  // Set static positions for nodes
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (30.0, 0.0, 0.0));
  positionAlloc->Add (Vector (60.0, 0.0, 0.0));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.SetPositionAllocator (positionAlloc);
  mobility.Install (meshNodes);

  // Install Internet stack
  InternetStackHelper internetStack;
  internetStack.Install (meshNodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (meshDevices);

  // Set up UDP server (sink) on node 2
  uint16_t port = 9000;
  UdpServerHelper udpServer (port);
  ApplicationContainer serverApp = udpServer.Install (meshNodes.Get (2));
  serverApp.Start (Seconds (1.0));
  serverApp.Stop (Seconds (10.0));

  // Set up UDP client on node 0 to send to node 2
  uint32_t maxPacketCount = 100;
  Time interPacketInterval = Seconds (0.1);
  UdpClientHelper udpClient (interfaces.GetAddress (2), port);
  udpClient.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
  udpClient.SetAttribute ("Interval", TimeValue (interPacketInterval));
  udpClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApp = udpClient.Install (meshNodes.Get (0));
  clientApp.Start (Seconds (2.0));
  clientApp.Stop (Seconds (10.0));

  // Trace sent packets (node 0)
  meshDevices.Get (0)->TraceConnectWithoutContext ("MacTx", MakeCallback (&TxTrace));

  // Trace received packets at the sink (node 2)
  Ptr<PacketSink> sink = DynamicCast<PacketSink> (serverApp.Get (0));
  sink->TraceConnectWithoutContext ("Rx", MakeCallback (&RxTrace));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  std::cout << "UDP Packets transmitted: " << g_txPackets << std::endl;
  std::cout << "UDP Packets received: " << g_rxPackets << std::endl;

  Simulator::Destroy ();
  return 0;
}