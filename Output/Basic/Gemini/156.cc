#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/csma-module.h"
#include "ns3/tcp-socket-base.h"

#include <fstream>
#include <iostream>
#include <string>

using namespace ns3;

std::ofstream csvFile;

void
PacketReceive(std::string context, Ptr<const Packet> p, const Address& addr)
{
  Time arrival_time = Simulator::Now();

  std::string sourceNode = context.substr(11, 1);
  std::string destinationNode = context.substr(13, 1);

  csvFile << sourceNode << ","
          << destinationNode << ","
          << p->GetSize() << ","
          << p->GetUid() << ","
          << arrival_time.GetSeconds() << std::endl;
}

int
main(int argc, char* argv[])
{
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(5);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices1 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
  NetDeviceContainer devices2 = pointToPoint.Install(nodes.Get(0), nodes.Get(2));
  NetDeviceContainer devices3 = pointToPoint.Install(nodes.Get(0), nodes.Get(3));
  NetDeviceContainer devices4 = pointToPoint.Install(nodes.Get(0), nodes.Get(4));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces1 = address.Assign(devices1);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces2 = address.Assign(devices2);

  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces3 = address.Assign(devices3);

  address.SetBase("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces4 = address.Assign(devices4);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 50000;

  BulkSendHelper source1("ns3::TcpSocketFactory",
                          InetSocketAddress(interfaces1.GetAddress(0), port));
  source1.SetAttribute("SendSize", UintegerValue(1024));
  source1.SetAttribute("MaxBytes", UintegerValue(1000000));
  ApplicationContainer sourceApps1 = source1.Install(nodes.Get(1));
  sourceApps1.Start(Seconds(1.0));
  sourceApps1.Stop(Seconds(10.0));

  BulkSendHelper source2("ns3::TcpSocketFactory",
                          InetSocketAddress(interfaces2.GetAddress(0), port));
  source2.SetAttribute("SendSize", UintegerValue(1024));
  source2.SetAttribute("MaxBytes", UintegerValue(1000000));
  ApplicationContainer sourceApps2 = source2.Install(nodes.Get(2));
  sourceApps2.Start(Seconds(1.0));
  sourceApps2.Stop(Seconds(10.0));

  BulkSendHelper source3("ns3::TcpSocketFactory",
                          InetSocketAddress(interfaces3.GetAddress(0), port));
  source3.SetAttribute("SendSize", UintegerValue(1024));
  source3.SetAttribute("MaxBytes", UintegerValue(1000000));
  ApplicationContainer sourceApps3 = source3.Install(nodes.Get(3));
  sourceApps3.Start(Seconds(1.0));
  sourceApps3.Stop(Seconds(10.0));

    BulkSendHelper source4("ns3::TcpSocketFactory",
                          InetSocketAddress(interfaces4.GetAddress(0), port));
  source4.SetAttribute("SendSize", UintegerValue(1024));
  source4.SetAttribute("MaxBytes", UintegerValue(1000000));
  ApplicationContainer sourceApps4 = source4.Install(nodes.Get(4));
  sourceApps4.Start(Seconds(1.0));
  sourceApps4.Stop(Seconds(10.0));

  PacketSinkHelper sink1("ns3::TcpSocketFactory",
                         InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApps1 = sink1.Install(nodes.Get(0));
  sinkApps1.Start(Seconds(0.0));
  sinkApps1.Stop(Seconds(11.0));

  PacketSinkHelper sink2("ns3::TcpSocketFactory",
                         InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApps2 = sink2.Install(nodes.Get(0));
  sinkApps2.Start(Seconds(0.0));
  sinkApps2.Stop(Seconds(11.0));

  PacketSinkHelper sink3("ns3::TcpSocketFactory",
                         InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApps3 = sink3.Install(nodes.Get(0));
  sinkApps3.Start(Seconds(0.0));
  sinkApps3.Stop(Seconds(11.0));

  PacketSinkHelper sink4("ns3::TcpSocketFactory",
                         InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApps4 = sink4.Install(nodes.Get(0));
  sinkApps4.Start(Seconds(0.0));
  sinkApps4.Stop(Seconds(11.0));

  csvFile.open("star_network_data.csv");
  csvFile << "SourceNode,DestinationNode,PacketSize,PacketUID,ArrivalTime(s)" << std::endl;

  Config::Connect("/NodeList/0/ApplicationList/*/$ns3::PacketSink/Rx",
                    MakeCallback(&PacketReceive));
  Config::Connect("/NodeList/0/ApplicationList/*/$ns3::PacketSink/Rx",
                  MakeCallback(&PacketReceive));
  Config::Connect("/NodeList/0/ApplicationList/*/$ns3::PacketSink/Rx",
                  MakeCallback(&PacketReceive));
  Config::Connect("/NodeList/0/ApplicationList/*/$ns3::PacketSink/Rx",
                  MakeCallback(&PacketReceive));

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();
  Simulator::Destroy();

  csvFile.close();

  return 0;
}