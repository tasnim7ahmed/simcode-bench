#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet-sink.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

uint64_t totalBytesReceived = 0;
uint64_t totalBytesSent = 0;

void
RxTrace(Ptr<const Packet> packet, const Address &address)
{
  totalBytesReceived += packet->GetSize();
}

void
TxTrace(Ptr<const Packet> packet)
{
  totalBytesSent += packet->GetSize();
}

int main(int argc, char *argv[])
{
  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("10ms"));

  NetDeviceContainer devices;
  devices = p2p.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Install PacketSink (TCP Server) on node 1
  uint16_t port = 8080;
  Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), port));
  PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(1));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(10.0));

  // Install OnOffApplication (TCP Client) on node 0
  OnOffHelper client("ns3::TcpSocketFactory", sinkAddress);
  client.SetAttribute("DataRate", StringValue("500Kbps"));
  client.SetAttribute("PacketSize", UintegerValue(1024));
  client.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
  client.SetAttribute("StopTime", TimeValue(Seconds(9.0)));
  ApplicationContainer clientApp = client.Install(nodes.Get(0));

  // Tracing TX and RX packets at TCP level
  // TX - on node 0, the client
  Ptr<NetDevice> clientDev = devices.Get(0);
  clientDev->TraceConnectWithoutContext("PhyTxEnd", MakeCallback(&TxTrace));

  // RX - on node 1, the server
  Ptr<Application> app = sinkApp.Get(0);
  Ptr<PacketSink> sink = DynamicCast<PacketSink>(app);
  sink->TraceConnectWithoutContext("Rx", MakeCallback(&RxTrace));

  // Enable PCAP tracing at the point-to-point device layer
  p2p.EnablePcapAll("tcp-p2p");

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  std::cout << "Total Bytes Sent: " << totalBytesSent << std::endl;
  std::cout << "Total Bytes Received: " << totalBytesReceived << std::endl;
  std::cout << "PacketSink Bytes Received: " << sink->GetTotalRx() << std::endl;

  Simulator::Destroy();
  return 0;
}