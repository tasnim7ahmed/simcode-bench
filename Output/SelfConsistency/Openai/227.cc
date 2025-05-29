// uav-udp-aodv-simulation.cc

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-mac-helper.h"
#include "ns3/wifi-phy-helper.h"
#include "ns3/aodv-helper.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/three-gpp-3d-mobility-model.h"
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("UavUdpAodvSimulation");

// Parameters
static const uint32_t nUavs = 5;
static const double simTime = 60.0; // seconds

void ReceivePacket (Ptr<Socket> socket)
{
  while (Ptr<Packet> packet = socket->Recv ())
    {
      NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
                   << "s, packet received of size " << packet->GetSize ());
    }
}

int main (int argc, char *argv[])
{
  LogComponentEnable("UavUdpAodvSimulation", LOG_LEVEL_INFO);

  // Command line parameters
  uint32_t numUavs = nUavs;
  double simulationTime = simTime;
  CommandLine cmd;
  cmd.AddValue ("numUavs", "Number of UAVs", numUavs);
  cmd.AddValue ("simulationTime", "Simulation time [s]", simulationTime);
  cmd.Parse (argc, argv);

  // Create nodes: numUavs UAVs + 1 GCS
  NodeContainer uavNodes;
  uavNodes.Create (numUavs);

  Ptr<Node> gcsNode = CreateObject<Node> ();
  NodeContainer allNodes;
  allNodes.Add (gcsNode);
  allNodes.Add (uavNodes);

  // Wifi configuration
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211g);

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, allNodes);

  // Mobility Model
  MobilityHelper mobility;

  // UAVs: Randomly move in 3D airspace (ThreeGpp3dMobilityModel)
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  for (uint32_t i = 0; i < numUavs; ++i)
    {
      // Set initial positions randomly in a box 500x500x100
      positionAlloc->Add (Vector (100 + 300*drand48(), 100 + 300*drand48(), 50.0 + 30*drand48()));
    }
  mobility.SetMobilityModel ("ns3::ThreeGpp3dMobilityModel",
                            "Bounds", BoxValue (Box (0, 500, 0, 500, 20, 120)),
                            "Speed", StringValue ("ns3::UniformRandomVariable[Min=10.0|Max=20.0]"), // m/s
                            "Distance", DoubleValue (50.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.Install (uavNodes);

  // GCS: fixed position at ground
  MobilityHelper mobilityGcs;
  mobilityGcs.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobilityGcs.SetPositionAllocator ("ns3::ListPositionAllocator",
                                   "PositionList", VectorValue (std::vector<Vector> ({Vector (250.0, 250.0, 0.0)})));
  mobilityGcs.Install (gcsNode);

  // Internet stack with AODV
  InternetStackHelper stack;
  AodvHelper aodv;
  stack.SetRoutingHelper (aodv);
  stack.Install (allNodes);

  // Assign IPv4 addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // UDP apps: UAVs send UDP packets to GCS
  uint16_t udpPort = 4000;

  // UDP server (sink) on GCS
  Ptr<Socket> recvSink = Socket::CreateSocket (gcsNode, UdpSocketFactory::GetTypeId ());
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), udpPort);
  recvSink->Bind (local);
  recvSink->SetRecvCallback (MakeCallback (&ReceivePacket));

  // Each UAV sends UDP packets to GCS periodically
  for (uint32_t i = 0; i < numUavs; ++i)
    {
      Ptr<Socket> source = Socket::CreateSocket (uavNodes.Get (i), UdpSocketFactory::GetTypeId ());
      InetSocketAddress remote = InetSocketAddress (interfaces.GetAddress (0), udpPort); // GCS address
      source->Connect (remote);

      // Schedule sending packets every second
      for (double t = 1.0; t < simulationTime; t += 1.0)
        {
          Simulator::ScheduleWithContext (source->GetNode ()->GetId (),
                                          Seconds (t),
                                          [source, i, t] () {
                                              std::ostringstream msg;
                                              msg << "UAV" << i << " at t=" << t;
                                              Ptr<Packet> packet = Create<Packet> ((uint8_t*) msg.str ().c_str (), msg.str ().size ());
                                              source->Send (packet);
                                          });
        }
    }

  // Enable PCAP tracing if needed
  // wifiPhy.EnablePcapAll ("uav-udp-aodv-simulation", true);

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}