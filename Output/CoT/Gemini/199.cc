#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/csma-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpExample");

int main (int argc, char *argv[])
{
  bool verbose = false;
  bool tracing = false;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);

  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("TcpExample", LOG_LEVEL_INFO);
    }

  Time::SetResolution (Time::NS);
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  NodeContainer routers;
  routers.Create (3);

  NodeContainer n0n1 = NodeContainer (routers.Get (0), routers.Get (1));
  NodeContainer n1n2 = NodeContainer (routers.Get (1), routers.Get (2));

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer d0d1 = pointToPoint.Install (n0n1);
  NetDeviceContainer d1d2 = pointToPoint.Install (n1n2);

  NodeContainer lanNodes;
  lanNodes.Create (2);
  NodeContainer lanNodesWithRouter = NodeContainer (lanNodes.Get (0), lanNodes.Get (1), routers.Get (1));

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  NetDeviceContainer lanDevices = csma.Install (lanNodesWithRouter);

  InternetStackHelper stack;
  stack.Install (routers);
  stack.Install (lanNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i1 = address.Assign (d0d1);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i2 = address.Assign (d1d2);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer lanInterfaces = address.Assign (lanDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 9;

  Address sinkAddress (InetSocketAddress (i1i2.GetAddress (1, 0), port));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", sinkAddress);
  ApplicationContainer sinkApps = packetSinkHelper.Install (routers.Get (2));

  sinkApps.Start (Seconds (1.0));
  sinkApps.Stop (Seconds (10.0));

  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (lanNodes.Get (0), TcpSocketFactory::GetTypeId ());

  Ptr<Application> app = CreateObject<BulkSendApplication> ();
  Ptr<BulkSendApplication> bulkSendApp = DynamicCast<BulkSendApplication> (app);
  bulkSendApp->SetSocket (ns3TcpSocket);
  bulkSendApp->SetPeer (InetSocketAddress (i1i2.GetAddress (1, 0), port));
  bulkSendApp->SetSendSize (1448);
  bulkSendApp->SetMaxBytes (0);

  ApplicationContainer clientApps;
  clientApps.Add (bulkSendApp);
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  if (tracing)
    {
      pointToPoint.EnablePcapAll ("tcp-example");
    }

  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll ();

  Simulator::Stop (Seconds (10.0));

  AnimationInterface anim ("tcp-example.xml");
  anim.SetConstantPosition (routers.Get (0), 10, 10);
  anim.SetConstantPosition (routers.Get (1), 50, 10);
  anim.SetConstantPosition (routers.Get (2), 90, 10);
  anim.SetConstantPosition (lanNodes.Get (0), 50, 50);
  anim.SetConstantPosition (lanNodes.Get (1), 50, 90);

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  std::ofstream fileFlow;
  fileFlow.open ("tcp-example.flowmon", std::ios::out);
  monitor->SerializeToXmlFile ("tcp-example.flowmon", true, true);

  Simulator::Destroy ();

  return 0;
}