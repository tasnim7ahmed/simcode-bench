#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mesh-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Mesh3NodeExample");

uint32_t g_packetsTx = 0;
uint32_t g_packetsRx = 0;

void TxCallback (Ptr<const Packet> pkt)
{
  g_packetsTx++;
}

void RxCallback (Ptr<const Packet> pkt, const Address &addr)
{
  g_packetsRx++;
}

int main (int argc, char *argv[])
{
  // LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  // LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  Time::SetResolution (Time::NS);

  NodeContainer nodes;
  nodes.Create (3);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  MeshHelper mesh = MeshHelper::Default ();
  mesh.SetStackInstaller ("ns3::Dot11sStack");
  mesh.SetSpreadInterfaceChannels (MeshHelper::SPREAD_CHANNELS);
  mesh.SetMacType ("RandomStart", TimeValue (Seconds (0.1)));
  NetDeviceContainer meshDevices = mesh.Install (wifiPhy, nodes);

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (5.0, 0.0, 0.0));
  positionAlloc->Add (Vector (2.5, 5.0, 0.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  InternetStackHelper internetStack;
  internetStack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (meshDevices);

  // Setup UDP server on node 2 (index 2)
  uint16_t serverPort = 4000;
  UdpServerHelper udpServer (serverPort);
  ApplicationContainer serverApps = udpServer.Install (nodes.Get (2));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // Setup UDP client on node 0, destination is node 2
  uint32_t maxPackets = 20;
  Time interPacketInterval = Seconds (0.5);
  UdpClientHelper udpClient (interfaces.GetAddress (2), serverPort);
  udpClient.SetAttribute ("MaxPackets", UintegerValue (maxPackets));
  udpClient.SetAttribute ("Interval", TimeValue (interPacketInterval));
  udpClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = udpClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Setup UDP server on node 0 (to receive from node 1)
  uint16_t serverPort2 = 4001;
  UdpServerHelper udpServer2 (serverPort2);
  ApplicationContainer serverApps2 = udpServer2.Install (nodes.Get (0));
  serverApps2.Start (Seconds (1.0));
  serverApps2.Stop (Seconds (10.0));

  // Client on node 1 sending to node 0
  UdpClientHelper udpClient2 (interfaces.GetAddress (0), serverPort2);
  udpClient2.SetAttribute ("MaxPackets", UintegerValue (maxPackets));
  udpClient2.SetAttribute ("Interval", TimeValue (interPacketInterval));
  udpClient2.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps2 = udpClient2.Install (nodes.Get (1));
  clientApps2.Start (Seconds (2.0));
  clientApps2.Stop (Seconds (10.0));

  // Packet trace connection: count packets sent by the UdpClient apps
  Config::ConnectWithoutContext ("/NodeList/0/ApplicationList/0/$ns3::UdpClient/Tx", MakeCallback (&TxCallback));
  Config::ConnectWithoutContext ("/NodeList/1/ApplicationList/0/$ns3::UdpClient/Tx", MakeCallback (&TxCallback));

  // Packet trace connection: count packets received by the UdpServer apps
  Config::ConnectWithoutContext ("/NodeList/2/ApplicationList/0/$ns3::UdpServer/Rx", MakeCallback (&RxCallback));
  Config::ConnectWithoutContext ("/NodeList/0/ApplicationList/1/$ns3::UdpServer/Rx", MakeCallback (&RxCallback));

  wifiPhy.EnablePcapAll ("mesh-3node");

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  std::cout << "Packets sent: " << g_packetsTx << std::endl;
  std::cout << "Packets received: " << g_packetsRx << std::endl;

  Simulator::Destroy ();
  return 0;
}