#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiUdpManyClientsOneServer");

void
ReceivePacket (Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom (from)))
    {
      NS_LOG_UNCOND("Server received " << packet->GetSize()
                      << " bytes from " << InetSocketAddress::ConvertFrom(from).GetIpv4()
                      << " at " << Simulator::Now ().GetSeconds () << "s");
    }
}

int
main (int argc, char *argv[])
{
  uint32_t nClients = 5;
  double simulationTime = 10.0; // seconds
  uint16_t port = 50000;

  CommandLine cmd;
  cmd.AddValue ("nClients", "Number of client nodes", nClients);
  cmd.Parse (argc, argv);

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (nClients);
  NodeContainer wifiApNode;
  wifiApNode.Create (1);
  NodeContainer allNodes = NodeContainer (wifiStaNodes, wifiApNode);

  // Wi-Fi PHY and channel
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211g);

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns3-wifi");

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, wifiStaNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevice;
  apDevice = wifi.Install (phy, mac, wifiApNode);

  // Mobility (static, no movement)
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (allNodes);

  // Internet stack
  InternetStackHelper stack;
  stack.Install (allNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces;
  staInterfaces = address.Assign (staDevices);
  Ipv4InterfaceContainer apInterface;
  apInterface = address.Assign (apDevice);

  // UDP server app using Socket for logging
  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> recvSocket = Socket::CreateSocket (wifiApNode.Get (0), tid);
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), port);
  recvSocket->Bind (local);
  recvSocket->SetRecvCallback (MakeCallback (&ReceivePacket));

  // UDP Client Apps
  for (uint32_t i = 0; i < nClients; ++i)
    {
      Ptr<Socket> source = Socket::CreateSocket (wifiStaNodes.Get(i), tid);
      InetSocketAddress remote = InetSocketAddress (apInterface.GetAddress (0), port);
      source->Connect (remote);

      // Send packets periodically
      Simulator::ScheduleWithContext (source->GetNode ()->GetId (),
        Seconds (1.0 + i*0.1),
        [source](){
          for (int j = 0; j < 5; ++j)
            {
              Ptr<Packet> pkt = Create<Packet> (100);
              source->Send (pkt);
              Simulator::Schedule (Seconds (1.0 * (j+1)), [source](){
                  Ptr<Packet> p = Create<Packet> (100);
                  source->Send (p);
                });
            }
        });
    }

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}