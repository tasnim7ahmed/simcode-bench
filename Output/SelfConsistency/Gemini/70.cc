#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/queue.h"
#include "ns3/queue-disc.h"
#include "ns3/red-queue-disc.h"
#include "ns3/nlred-queue-disc.h"
#include "ns3/random-variable-stream.h"
#include <iostream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DumbbellRedNlred");

int main (int argc, char *argv[])
{
  // Enable logging
  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
  LogComponentEnable ("RedQueueDisc", LOG_LEVEL_ALL);
  LogComponentEnable ("NlredQueueDisc", LOG_LEVEL_ALL);

  // Set default values
  uint32_t nLeafNodes = 10;
  std::string queueDiscType = "RedQueueDisc";
  std::string queueMode = "packets";
  uint32_t maxPackets = 20;
  uint32_t packetSize = 512;
  std::string dataRate = "10Mbps";
  std::string delay = "20ms";
  double qDiscLimit = 20;
  double redMinTh = 5;
  double redMaxTh = 15;
  double redLinkBandwidth = 10;
  double redQueueWeight = 0.002;
  double nlredLinkBandwidth = 10;
  double nlredQueueWeight = 0.002;

  // Command line arguments
  CommandLine cmd;
  cmd.AddValue ("nLeafNodes", "Number of leaf nodes", nLeafNodes);
  cmd.AddValue ("queueDiscType", "Queue disc type (RedQueueDisc or NlredQueueDisc)", queueDiscType);
  cmd.AddValue ("queueMode", "Queue mode (bytes or packets)", queueMode);
  cmd.AddValue ("maxPackets", "Max packets in queue", maxPackets);
  cmd.AddValue ("packetSize", "Size of packets", packetSize);
  cmd.AddValue ("dataRate", "Data rate of link", dataRate);
  cmd.AddValue ("delay", "Delay of link", delay);
  cmd.AddValue ("qDiscLimit", "Queue disc limit", qDiscLimit);
  cmd.AddValue ("redMinTh", "RED minimum threshold", redMinTh);
  cmd.AddValue ("redMaxTh", "RED maximum threshold", redMaxTh);
  cmd.AddValue ("redLinkBandwidth", "RED link bandwidth", redLinkBandwidth);
  cmd.AddValue ("redQueueWeight", "RED queue weight", redQueueWeight);
  cmd.AddValue ("nlredLinkBandwidth", "NLRED link bandwidth", nlredLinkBandwidth);
  cmd.AddValue ("nlredQueueWeight", "NLRED queue weight", nlredQueueWeight);
  cmd.Parse (argc, argv);

  // Create nodes
  NodeContainer leftRouter, rightRouter;
  leftRouter.Create (1);
  rightRouter.Create (1);

  NodeContainer leftLeafNodes, rightLeafNodes;
  leftLeafNodes.Create (nLeafNodes);
  rightLeafNodes.Create (nLeafNodes);

  // Create point-to-point helper
  PointToPointHelper p2pRouter;
  p2pRouter.SetDeviceAttribute ("DataRate", StringValue (dataRate));
  p2pRouter.SetChannelAttribute ("Delay", StringValue (delay));

  PointToPointHelper p2pLeaf;
  p2pLeaf.SetDeviceAttribute ("DataRate", StringValue (dataRate));
  p2pLeaf.SetChannelAttribute ("Delay", StringValue (delay));

  // Create links
  NetDeviceContainer routerDevices = p2pRouter.Install (leftRouter.Get (0), rightRouter.Get (0));

  // Install queue disc on the router link
  Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
  em->SetAttribute ("ErrorRate", DoubleValue (0.00001));
  routerDevices.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (em));

  NetDeviceContainer leftLeafDevices, rightLeafDevices;
  for (uint32_t i = 0; i < nLeafNodes; ++i)
    {
      NetDeviceContainer left = p2pLeaf.Install (leftRouter.Get (0), leftLeafNodes.Get (i));
      leftLeafDevices.Add (left.Get (1)); // Add leaf device

      NetDeviceContainer right = p2pLeaf.Install (rightRouter.Get (0), rightLeafNodes.Get (i));
      rightLeafDevices.Add (right.Get (1)); // Add leaf device
    }

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install (leftRouter);
  stack.Install (rightRouter);
  stack.Install (leftLeafNodes);
  stack.Install (rightLeafNodes);

  // Assign addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer routerInterfaces = address.Assign (routerDevices);

  Ipv4InterfaceContainer leftLeafInterfaces;
  address.SetBase ("10.1.2.0", "255.255.255.0");
  leftLeafInterfaces = address.Assign (leftLeafDevices);

  Ipv4InterfaceContainer rightLeafInterfaces;
  address.SetBase ("10.1.3.0", "255.255.255.0");
  rightLeafInterfaces = address.Assign (rightLeafDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Set up queue disc
  QueueDiscContainer queueDiscs;
  QueueDiscHelper queueDiscHelper;

  if (queueDiscType == "RedQueueDisc")
    {
      queueDiscHelper.SetTypeid ("ns3::RedQueueDisc");
      queueDiscHelper.Set ("LinkBandwidth", DoubleValue (redLinkBandwidth * 1e6)); // Mbps to bps
      queueDiscHelper.Set ("QueueWeight", DoubleValue (redQueueWeight));
      queueDiscHelper.Set ("MeanPktSize", UintegerValue (packetSize));

      if (queueMode == "bytes")
        {
          queueDiscHelper.Set ("Mode", EnumValue (Queue::QUEUE_MODE_BYTES));
          queueDiscHelper.Set ("MaxThreshold", DoubleValue (redMaxTh * packetSize));
          queueDiscHelper.Set ("MinThreshold", DoubleValue (redMinTh * packetSize));
          queueDiscHelper.Set ("QueueLimit", DoubleValue (qDiscLimit * packetSize));

        }
      else
        {
          queueDiscHelper.Set ("Mode", EnumValue (Queue::QUEUE_MODE_PACKETS));
          queueDiscHelper.Set ("MaxThreshold", DoubleValue (redMaxTh));
          queueDiscHelper.Set ("MinThreshold", DoubleValue (redMinTh));
          queueDiscHelper.Set ("QueueLimit", DoubleValue (qDiscLimit));
        }
    }
  else if (queueDiscType == "NlredQueueDisc")
    {
      queueDiscHelper.SetTypeid ("ns3::NlredQueueDisc");
      queueDiscHelper.Set ("LinkBandwidth", DoubleValue (nlredLinkBandwidth * 1e6)); // Mbps to bps
      queueDiscHelper.Set ("QueueWeight", DoubleValue (nlredQueueWeight));
      queueDiscHelper.Set ("MeanPktSize", UintegerValue (packetSize));

      if (queueMode == "bytes")
        {
          queueDiscHelper.Set ("Mode", EnumValue (Queue::QUEUE_MODE_BYTES));
          queueDiscHelper.Set ("MaxThreshold", DoubleValue (redMaxTh * packetSize));
          queueDiscHelper.Set ("MinThreshold", DoubleValue (redMinTh * packetSize));
          queueDiscHelper.Set ("QueueLimit", DoubleValue (qDiscLimit * packetSize));
        }
      else
        {
          queueDiscHelper.Set ("Mode", EnumValue (Queue::QUEUE_MODE_PACKETS));
          queueDiscHelper.Set ("MaxThreshold", DoubleValue (redMaxTh));
          queueDiscHelper.Set ("MinThreshold", DoubleValue (redMinTh));
          queueDiscHelper.Set ("QueueLimit", DoubleValue (qDiscLimit));
        }
    }
  else
    {
      std::cerr << "Invalid queue disc type: " << queueDiscType << std::endl;
      return 1;
    }

  queueDiscs = queueDiscHelper.Install (routerDevices.Get (0));

  // Set up OnOff application on right side nodes
  for (uint32_t i = 0; i < nLeafNodes; ++i)
    {
      OnOffHelper onoff ("ns3::UdpSocketFactory", Address (InetSocketAddress (rightLeafInterfaces.GetAddress (i), 9)));
      onoff.SetConstantRate (DataRate ("1Mbps"));
      onoff.SetAttribute ("PacketSize", UintegerValue (packetSize));

      // Random on/off times
      Ptr<UniformRandomVariable> onTime = CreateObject<UniformRandomVariable> ();
      onTime->SetAttribute ("Min", DoubleValue (0.1));
      onTime->SetAttribute ("Max", DoubleValue (1.0));
      onoff.SetAttribute ("OnTime", PointerValue (onTime));

      Ptr<UniformRandomVariable> offTime = CreateObject<UniformRandomVariable> ();
      offTime->SetAttribute ("Min", DoubleValue (0.1));
      offTime->SetAttribute ("Max", DoubleValue (1.0));
      onoff.SetAttribute ("OffTime", PointerValue (offTime));

      ApplicationContainer apps = onoff.Install (leftLeafNodes.Get (i));
      apps.Start (Seconds (1.0));
      apps.Stop (Seconds (10.0));

      // Install UDP client sink on the right side
      UdpClientHelper client (rightLeafInterfaces.GetAddress (i), 9);
      client.SetAttribute ("MaxPackets", UintegerValue (1000000));
      client.SetAttribute ("Interval", TimeValue (Time ("0.00001")));
      client.SetAttribute ("PacketSize", UintegerValue (packetSize));
      ApplicationContainer clientApps = client.Install (rightLeafNodes.Get (i));
      clientApps.Start (Seconds (1.0));
      clientApps.Stop (Seconds (10.0));
    }

  // Add queue disc drop trace
  Config::Connect ("/ChannelList/0/DeviceList/0/$ns3::PointToPointNetDevice/TxQueue/Drop", MakeCallback (&Queue::DropTrace));

  // Add queue disc drop trace.
  for (uint32_t i = 0; i < queueDiscs.GetN(); ++i) {
    Ptr<QueueDisc> queueDisc = queueDiscs.Get (i);
    if (queueDisc != nullptr) {
        Config::Connect ("/NodeList/" + std::to_string (leftRouter.Get (0)->GetId ()) + "/$ns3::TrafficControlLayer/RootQueueDiscList/" + std::to_string (i) + "/Drop",
                       MakeCallback (&QueueDisc::DropTrace));

        Config::Connect ("/NodeList/" + std::to_string (leftRouter.Get (0)->GetId ()) + "/$ns3::TrafficControlLayer/RootQueueDiscList/" + std::to_string (i) + "/Mark",
                       MakeCallback (&QueueDisc::MarkTrace));

    } else {
        std::cerr << "Error: QueueDisc " << i << " is null." << std::endl;
    }
  }

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}