#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"
#include "ns3/queue.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Dumbbell");

void QueueSizeCobaltTrace (std::string context, Ptr<Queue<Packet>> queue)
{
  std::ofstream outfile;
  outfile.open ("dumbbell-cobalt-queue.dat", std::ios::app);
  outfile << Simulator::Now ().GetSeconds () << " " << queue->GetNPackets () << std::endl;
  outfile.close ();
}

void QueueSizeCodelTrace (std::string context, Ptr<Queue<Packet>> queue)
{
  std::ofstream outfile;
  outfile.open ("dumbbell-codel-queue.dat", std::ios::app);
  outfile << Simulator::Now ().GetSeconds () << " " << queue->GetNPackets () << std::endl;
  outfile.close ();
}

void CwndChange (std::string context, uint32_t oldCwnd, uint32_t newCwnd)
{
  std::ofstream outfile;
  outfile.open ("dumbbell-cwnd.dat", std::ios::app);
  outfile << Simulator::Now ().GetSeconds () << " " << newCwnd << std::endl;
  outfile.close ();
}

void experiment (std::string qdisc)
{
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpNewReno"));
  uint32_t nLeftLeaf = 7;
  uint32_t nRightLeaf = 1;
  double simulationTime = 10;
  std::string bandwidth = "5Mbps";
  std::string delay = "2ms";
  std::string queueSize = "20p";

  NodeContainer leftLeafNodes, rightLeafNodes, routerNodes;
  leftLeafNodes.Create (nLeftLeaf);
  rightLeafNodes.Create (nRightLeaf);
  routerNodes.Create (2);

  InternetStackHelper internet;
  internet.Install (leftLeafNodes);
  internet.Install (rightLeafNodes);
  internet.Install (routerNodes);

  PointToPointHelper pointToPointLeaf;
  pointToPointLeaf.SetDeviceAttribute ("DataRate", StringValue (bandwidth));
  pointToPointLeaf.SetChannelAttribute ("Delay", StringValue (delay));

  PointToPointHelper pointToPointRouter;
  pointToPointRouter.SetDeviceAttribute ("DataRate", StringValue (bandwidth));
  pointToPointRouter.SetChannelAttribute ("Delay", StringValue (delay));
  QueueDiscHelper queueDiscHelper;

  if (qdisc == "CoDel") {
      queueDiscHelper.SetRootQueueDiscType ("ns3::CoDelQueueDisc");
      queueDiscHelper.SetQueueDiscAttribute ("Target", TimeValue (MicroSeconds (50)));
      queueDiscHelper.SetQueueDiscAttribute ("Interval", TimeValue (MilliSeconds (100)));
  } else if (qdisc == "Cobalt") {
      queueDiscHelper.SetRootQueueDiscType ("ns3::CobaltQueueDisc");
  } else {
      std::cerr << "Invalid qdisc: " << qdisc << std::endl;
      return;
  }
  
  NetDeviceContainer leftLeafDevices, rightLeafDevices, routerDevices;

  for (uint32_t i = 0; i < nLeftLeaf; ++i)
    {
      NetDeviceContainer link = pointToPointLeaf.Install (leftLeafNodes.Get (i), routerNodes.Get (0));
      leftLeafDevices.Add (link.Get (0));
      routerDevices.Add (link.Get (1));
    }

  for (uint32_t i = 0; i < nRightLeaf; ++i)
    {
      NetDeviceContainer link = pointToPointLeaf.Install (rightLeafNodes.Get (i), routerNodes.Get (1));
      rightLeafDevices.Add (link.Get (0));
      routerDevices.Add (link.Get (1));
    }

  NetDeviceContainer routerLink = pointToPointRouter.Install (routerNodes.Get (0), routerNodes.Get (1));

  TrafficControlHelper tch;
  tch.SetRootQueueDisc ("ns3::FifoQueueDisc", "MaxSize", StringValue (queueSize));
  QueueDiscContainer queueDiscs;

  queueDiscs.Add (tch.Install (routerLink.Get (0)));
  queueDiscs.Add (tch.Install (routerLink.Get (1)));

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer leftLeafInterfaces = ipv4.Assign (leftLeafDevices);
  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer rightLeafInterfaces = ipv4.Assign (rightLeafDevices);
  ipv4.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer routerInterfaces = ipv4.Assign (routerDevices);
  ipv4.SetBase ("10.1.4.0", "255.255.255.0");
  ipv4.Assign (routerLink);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 50000;
  UdpEchoServerHelper echoServer (port);
  ApplicationContainer serverApps = echoServer.Install (rightLeafNodes.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simulationTime));

  UdpEchoClientHelper echoClient (rightLeafInterfaces.GetAddress (0), port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1000000));
  echoClient.SetAttribute ("Interval", TimeValue (MicroSeconds (1)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < nLeftLeaf; ++i)
    {
      clientApps.Add (echoClient.Install (leftLeafNodes.Get (i)));
    }
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (simulationTime));

  BulkSendHelper source ("ns3::TcpSocketFactory",
                           InetSocketAddress (rightLeafInterfaces.GetAddress (0), 9));

    source.SetAttribute ("SendSize", UintegerValue (1024));
    source.SetAttribute ("MaxBytes", UintegerValue (0));

  ApplicationContainer sourceApps;
   for (uint32_t i = 0; i < nLeftLeaf; ++i)
    {
      sourceApps.Add (source.Install (leftLeafNodes.Get (i)));
    }
    sourceApps.Start (Seconds (2.0));
    sourceApps.Stop (Seconds (simulationTime));


  Config::ConnectWithoutContext ("/NodeList/*/ApplicationList/*/$ns3::BulkSendApplication/Socket[0]/CongestionWindow", MakeCallback (&CwndChange));

  if (qdisc == "CoDel") {
      Config::ConnectWithoutContext ("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/Qdisc/$ns3::QueueDisc/Queue", MakeCallback (&QueueSizeCodelTrace));
  } else if (qdisc == "Cobalt") {
      Config::ConnectWithoutContext ("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/Qdisc/$ns3::QueueDisc/Queue", MakeCallback (&QueueSizeCobaltTrace));
  }

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();
  Simulator::Destroy ();

  std::ofstream emptyFile;
  emptyFile.open ("dumbbell-cwnd.dat", std::ios::trunc);
  emptyFile.close ();

    if (qdisc == "CoDel") {
        emptyFile.open ("dumbbell-codel-queue.dat", std::ios::trunc);
        emptyFile.close ();
    } else if (qdisc == "Cobalt") {
        emptyFile.open ("dumbbell-cobalt-queue.dat", std::ios::trunc);
        emptyFile.close ();
    }
}

int main (int argc, char *argv[])
{
  LogComponent::SetAttribute ("UdpEchoClientApplication", "MaxPackets", UintegerValue (1000000));
  experiment ("Cobalt");
  experiment ("CoDel");

  return 0;
}