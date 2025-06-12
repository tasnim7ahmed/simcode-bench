#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet-sink.h"
#include "ns3/traffic-control-module.h"
#include "ns3/trace-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpStarServerExample");

void QueueTrace(std::string context, uint32_t oldValue, uint32_t newValue)
{
  static std::ofstream out("tcp-star-server.tr", std::ios_base::app);
  out << Simulator::Now().GetSeconds() << " " << context << " QueueSize " << oldValue << " -> " << newValue << std::endl;
}

void RxTrace(std::string context, Ptr<const Packet> packet, const Address &address)
{
  static std::ofstream out("tcp-star-server.tr", std::ios_base::app);
  out << Simulator::Now().GetSeconds() << " " << context << " Rx " << packet->GetSize() << " bytes" << std::endl;
}

int main(int argc, char *argv[])
{
  uint32_t numArmNodes = 8;
  uint32_t packetSize = 1024;
  std::string dataRate = "1Mbps";

  CommandLine cmd;
  cmd.AddValue("numNodes", "Number of nodes in star (including hub, default 9)", numArmNodes);
  cmd.AddValue("packetSize", "CBR packet size in bytes", packetSize);
  cmd.AddValue("dataRate", "CBR application data rate", dataRate);
  cmd.Parse(argc, argv);

  uint32_t nSpokes = (numArmNodes < 2) ? 2 : (numArmNodes);
  Ptr<Node> hub = CreateObject<Node>();
  NodeContainer armNodes;
  armNodes.Create(nSpokes);

  NodeContainer allNodes;
  allNodes.Add(hub);
  for (uint32_t i = 0; i < nSpokes; ++i)
    allNodes.Add(armNodes.Get(i));

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));
  NetDeviceContainer hubDevices;
  NetDeviceContainer armDevices;
  std::vector<NetDeviceContainer> devicePairs;

  for (uint32_t i = 0; i < nSpokes; ++i)
  {
    NodeContainer pair(hub, armNodes.Get(i));
    NetDeviceContainer link = p2p.Install(pair);
    hubDevices.Add(link.Get(0));
    armDevices.Add(link.Get(1));
    devicePairs.push_back(link);
  }

  InternetStackHelper stack;
  stack.Install(allNodes);

  Ipv4AddressHelper address;
  std::vector<Ipv4InterfaceContainer> interfaces;
  for (uint32_t i = 0; i < nSpokes; ++i)
  {
    std::ostringstream subnet;
    subnet << "10.1." << i + 1 << ".0";
    address.SetBase(subnet.str().c_str(), "255.255.255.0");
    interfaces.push_back(address.Assign(devicePairs[i]));
  }

  TrafficControlHelper tch;
  tch.SetRootQueueDisc("ns3::PfifoFastQueueDisc");
  std::vector<Ptr<QueueDisc>> hubQueueDiscs;
  for (uint32_t i = 0; i < hubDevices.GetN(); ++i)
  {
    NetDeviceContainer singleHubDev;
    singleHubDev.Add(hubDevices.Get(i));
    Ptr<QueueDisc> q = tch.Install(singleHubDev).Get(0);
    std::ostringstream ctx;
    ctx << "/NodeList/" << hub->GetId()
        << "/DeviceList/" << i
        << "/$ns3::PointToPointNetDevice/TxQueue";
    q->TraceConnect("PacketsInQueue", ctx.str(), MakeCallback(&QueueTrace));
    hubQueueDiscs.push_back(q);
  }

  ApplicationContainer sinkApps, clientApps;
  for (uint32_t i = 0; i < nSpokes; ++i)
  {
    // Install Sink (server) on the hub for each source
    uint16_t port = 50000 + i;
    Address sinkLocalAddress(InetSocketAddress(interfaces[i].GetAddress(0), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer thisSink = sinkHelper.Install(hub);
    thisSink.Start(Seconds(0.0));
    thisSink.Stop(Seconds(10.0));
    sinkApps.Add(thisSink);

    Ptr<Application> app = thisSink.Get(0);
    std::ostringstream ctx;
    ctx << "/NodeList/" << hub->GetId() << "/ApplicationList/" << app->GetInstanceTypeId ().GetName();
    app->TraceConnect("Rx", ctx.str(), MakeCallback(&RxTrace));

    // Install client on each arm node
    OnOffHelper onoff("ns3::TcpSocketFactory", Address());
    onoff.SetAttribute("Remote", AddressValue(InetSocketAddress(interfaces[i].GetAddress(0), port)));
    onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate(dataRate)));
    onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(10.0)));
    ApplicationContainer temp = onoff.Install(armNodes.Get(i));
    clientApps.Add(temp);
  }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Enable pcap tracing
  for (uint32_t i = 0; i < nSpokes; ++i)
  {
    std::ostringstream pcapname;
    pcapname << "tcp-star-server-" << hub->GetId() << "-" << i << ".pcap";
    p2p.EnablePcap(pcapname.str(), hubDevices.Get(i), true, true);
    pcapname.str("");
    pcapname << "tcp-star-server-" << armNodes.Get(i)->GetId() << "-0.pcap";
    p2p.EnablePcap(pcapname.str(), armDevices.Get(i), true, true);
  }

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}