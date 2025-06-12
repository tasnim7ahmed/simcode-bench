#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/aodv-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/random-variable-stream.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AodvManetSimulation");

int main (int argc, char *argv[])
{
  bool verbose = false;
  uint32_t numNodes = 10;
  double simulationTime = 20;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if set to true", verbose);
  cmd.AddValue ("numNodes", "Number of mobile nodes", numNodes);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);

  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    }

  NodeContainer nodes;
  nodes.Create (numNodes);

  AodvHelper aodv;
  InternetStackHelper internet;
  internet.SetRoutingHelper (aodv);
  internet.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomBoxPositionAllocator",
                                 "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                 "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                 "Z", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=0.0]"));
  mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue ("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"),
                             "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"));
  mobility.Install (nodes);

  uint16_t port = 9;
  UdpServerHelper server (port);
  ApplicationContainer serverApps = server.Install (nodes.Get (numNodes / 2));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simulationTime));

  UniformRandomVariable random;
  random.SetStream (1);

  for (uint32_t i = 0; i < numNodes; ++i)
    {
      if (i == numNodes / 2) continue;

      UdpClientHelper client (interfaces.GetAddress (numNodes / 2), port);
      client.SetAttribute ("MaxPackets", UintegerValue (1000));
      client.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
      client.SetAttribute ("PacketSize", UintegerValue (1024));
      ApplicationContainer clientApps = client.Install (nodes.Get (i));
      clientApps.Start (Seconds (2.0 + random.GetValue ()));
      clientApps.Stop (Seconds (simulationTime));
    }

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}