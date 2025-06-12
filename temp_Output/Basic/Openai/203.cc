#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wave-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VANETSimulation");

uint32_t g_packetsSent = 0;
uint32_t g_packetsReceived = 0;
double g_totalDelay = 0.0;

void
RxPacketCallback (Ptr<const Packet> packet, const Address &addr)
{
  g_packetsReceived++;
}

void
DelayTracer (Ptr<const Packet> packet, const Address &from)
{
  static std::map<uint64_t, Time> sendTimes;
  uint64_t packetUid = packet->GetUid ();
  auto it = sendTimes.find (packetUid);
  if (it != sendTimes.end ())
    {
      Time delay = Simulator::Now () - it->second;
      g_totalDelay += delay.GetSeconds ();
      sendTimes.erase (it);
    }
}

void
TxPacketTrace (Ptr<const Packet> packet)
{
  g_packetsSent++;
  static std::map<uint64_t, Time> sendTimes;
  uint64_t packetUid = packet->GetUid ();
  sendTimes[packetUid] = Simulator::Now ();
}

int
main (int argc, char *argv[])
{
  uint32_t numVehicles = 5;
  double txPower = 20.0;
  double simulationTime = 20.0;
  double nodeDistance = 30.0;

  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

  // Create nodes (vehicles)
  NodeContainer vehicles;
  vehicles.Create (numVehicles);

  // Install mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> pos = CreateObject<ListPositionAllocator> ();
  std::vector<double> speeds = {15, 17, 19, 21, 23};
  for (uint32_t i = 0; i < numVehicles; ++i)
    {
      pos->Add (Vector (i * nodeDistance, 0.0, 0.0));
    }
  mobility.SetPositionAllocator (pos);
  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.Install (vehicles);

  // Set initial speeds
  for (uint32_t i = 0; i < numVehicles; ++i)
    {
      Ptr<ConstantVelocityMobilityModel> cvmm = vehicles.Get (i)->GetObject<ConstantVelocityMobilityModel> ();
      cvmm->SetVelocity (Vector (speeds[i], 0.0, 0.0));
    }

  // Configure 802.11p
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());
  wifiPhy.Set ("TxPowerStart", DoubleValue (txPower));
  wifiPhy.Set ("TxPowerEnd", DoubleValue (txPower));

  NqosWaveMacHelper wifi80211pMac = NqosWaveMacHelper::Default ();
  Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default ();

  NetDeviceContainer devices = wifi80211p.Install (wifiPhy, wifi80211pMac, vehicles);

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install (vehicles);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // Install UDP server on vehicle 0
  uint16_t serverPort = 4000;
  UdpServerHelper server (serverPort);
  ApplicationContainer serverApp = server.Install (vehicles.Get (0));
  serverApp.Start (Seconds (1.0));
  serverApp.Stop (Seconds (simulationTime));

  // Install UDP clients on vehicles 1-4
  for (uint32_t i = 1; i < numVehicles; ++i)
    {
      UdpClientHelper client (interfaces.GetAddress (0), serverPort);
      client.SetAttribute ("MaxPackets", UintegerValue (1000));
      client.SetAttribute ("Interval", TimeValue (Seconds (0.5)));
      client.SetAttribute ("PacketSize", UintegerValue (200));
      ApplicationContainer clientApp = client.Install (vehicles.Get (i));
      clientApp.Start (Seconds (2.0 + i * 0.1));
      clientApp.Stop (Seconds (simulationTime));
    }

  // Tracing
  // Count sent pkts and packet delays
  Config::Connect ("/NodeList/*/ApplicationList/*/$ns3::UdpClient/Tx", MakeCallback (&TxPacketTrace));
  // Count received pkts and packet delays
  Config::Connect ("/NodeList/0/ApplicationList/*/$ns3::UdpServer/Rx", MakeCallback (&RxPacketCallback));
  // For NetAnim
  AnimationInterface anim ("vanet.xml");
  anim.SetMaxPktsPerTraceFile (1000000);
  for (uint32_t i = 0; i < numVehicles; ++i)
    {
      std::ostringstream nodeLabel;
      nodeLabel << "Veh" << i+1;
      anim.UpdateNodeDescription (vehicles.Get (i), nodeLabel.str ());
      anim.UpdateNodeColor (vehicles.Get (i), 255, 100, 100);
    }

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();

  // Compute metrics
  double pdr = 0.0;
  double avgDelay = 0.0;
  if (g_packetsSent > 0)
    {
      pdr = (double)g_packetsReceived / g_packetsSent * 100.0;
      avgDelay = (g_packetsReceived > 0) ? g_totalDelay / g_packetsReceived : 0.0;
    }
  std::cout << "Simulation Results\n";
  std::cout << "Packets Sent:     " << g_packetsSent << std::endl;
  std::cout << "Packets Received: " << g_packetsReceived << std::endl;
  std::cout << "Packet Delivery Ratio (%): " << pdr << std::endl;
  std::cout << "Average End-to-End Delay (s): " << avgDelay << std::endl;

  Simulator::Destroy ();
  return 0;
}