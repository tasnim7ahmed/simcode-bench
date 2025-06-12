#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RingTopology");

int
main (int argc, char *argv[])
{
  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
  LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);

  uint32_t nNodes = 4;
  double simTime = 5.0;

  NodeContainer nodes;
  nodes.Create (nNodes);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("1ms"));

  NetDeviceContainer devices[nNodes];
  NodeContainer linkNodes[nNodes];

  // Create links in a ring (node i to node (i+1)%nNodes)
  for (uint32_t i = 0; i < nNodes; ++i)
    {
      linkNodes[i] = NodeContainer (nodes.Get (i), nodes.Get ((i + 1) % nNodes));
      devices[i] = p2p.Install (linkNodes[i]);
    }

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  Ipv4InterfaceContainer interfaces[nNodes];

  for (uint32_t i = 0; i < nNodes; ++i)
    {
      std::ostringstream subnet;
      subnet << "10.1." << i + 1 << ".0";
      address.SetBase (subnet.str ().c_str (), "255.255.255.0");
      interfaces[i] = address.Assign (devices[i]);
    }

  // Setup UDP Traffic (each node sends to next node)
  uint16_t port = 9000;
  ApplicationContainer clientApps, serverApps;

  for (uint32_t i = 0; i < nNodes; ++i)
    {
      // Server at neighbor node
      uint32_t recvNode = (i + 1) % nNodes;

      UdpServerHelper server (port);
      ApplicationContainer serverApp = server.Install (nodes.Get (recvNode));
      serverApp.Start (Seconds (0.0));
      serverApp.Stop (Seconds (simTime));
      serverApps.Add (serverApp);

      // Client at current node, sending to neighbor's address on right-facing interface
      Ipv4Address remoteAddr = interfaces[i].GetAddress (1);
      UdpClientHelper client (remoteAddr, port);
      client.SetAttribute ("MaxPackets", UintegerValue (1000));
      client.SetAttribute ("Interval", TimeValue (MilliSeconds (50)));
      client.SetAttribute ("PacketSize", UintegerValue (512));
      ApplicationContainer clientApp = client.Install (nodes.Get (i));
      clientApp.Start (Seconds (1.0));
      clientApp.Stop (Seconds (simTime));
      clientApps.Add (clientApp);
    }

  // Enable packet capture
  for (uint32_t i = 0; i < nNodes; ++i)
    {
      p2p.EnablePcapAll (std::string ("ring-node") + std::to_string (i));
    }

  // Enable ASCII trace
  AsciiTraceHelper ascii;
  p2p.EnableAsciiAll (ascii.CreateFileStream ("ring.tr"));

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}