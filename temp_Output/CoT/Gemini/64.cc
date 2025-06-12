#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpStarServer");

int main (int argc, char *argv[])
{
  uint32_t nNodes = 9;
  uint32_t packetSize = 1024;
  std::string dataRate = "5Mbps";

  CommandLine cmd;
  cmd.AddValue ("nNodes", "Number of nodes in the star topology", nNodes);
  cmd.AddValue ("packetSize", "Size of packets generated", packetSize);
  cmd.AddValue ("dataRate", "Data rate of the CBR traffic", dataRate);
  cmd.Parse (argc, argv);

  if (nNodes < 3) {
      std::cout << "Must have at least 3 nodes." << std::endl;
      return 1;
  }

  NodeContainer hubNode;
  hubNode.Create (1);

  NodeContainer armNodes;
  armNodes.Create (nNodes - 1);

  NodeContainer allNodes;
  allNodes.Add (hubNode);
  allNodes.Add (armNodes);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  InternetStackHelper stack;
  stack.Install (allNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer hubInterfaces;
  for (uint32_t i = 0; i < nNodes - 1; ++i) {
      NetDeviceContainer devices = pointToPoint.Install (hubNode.Get (0), armNodes.Get (i));
      Ipv4InterfaceContainer interfaces = address.Assign (devices);
      hubInterfaces.Add (interfaces.Get (0));
      Simulator::Schedule (Seconds (0.1), [&, i, devices, interfaces](){
          pointToPoint.EnablePcap ("tcp-star-server", devices.Get (0), true);
          pointToPoint.EnablePcap ("tcp-star-server", devices.Get (1), true);
      });
      address.NewNetwork ();
  }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 50000;

  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApp = sinkHelper.Install (hubNode.Get (0));
  sinkApp.Start (Seconds (1.0));
  sinkApp.Stop (Seconds (10.0));

  for (uint32_t i = 0; i < nNodes - 1; ++i) {
    OnOffHelper onOffHelper ("ns3::TcpSocketFactory", InetSocketAddress (hubInterfaces.GetAddress (i), port));
    onOffHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    onOffHelper.SetAttribute ("PacketSize", UintegerValue (packetSize));
    onOffHelper.SetAttribute ("DataRate", StringValue (dataRate));

    ApplicationContainer clientApps = onOffHelper.Install (armNodes.Get (i));
    clientApps.Start (Seconds (2.0));
    clientApps.Stop (Seconds (10.0));
  }

  Simulator::Stop (Seconds (10.0));

  // Enable Tracing
  AsciiTraceHelper ascii;
  pointToPoint.EnableAsciiAll (ascii.CreateFileStream ("tcp-star-server.tr"));

  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}