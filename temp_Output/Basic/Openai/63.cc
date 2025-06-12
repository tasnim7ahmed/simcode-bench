#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
  bool enableTracing = false;
  bool useNanoSeconds = false;

  CommandLine cmd;
  cmd.AddValue("tracing", "Enable/disable pcap tracing", enableTracing);
  cmd.AddValue("pcapNanoSeconds", "Use nanosecond timestamps in pcap trace", useNanoSeconds);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("500Kbps"));
  p2p.SetChannelAttribute("Delay", StringValue("5ms"));

  NetDeviceContainer devices = p2p.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t sinkPort = 8080;
  Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), sinkPort));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory",
                                    InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
  ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(10.0));

  BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", sinkAddress);
  bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(0));
  ApplicationContainer sourceApp = bulkSendHelper.Install(nodes.Get(0));
  sourceApp.Start(Seconds(0.0));
  sourceApp.Stop(Seconds(10.0));

  if (enableTracing)
    {
      p2p.EnablePcapAll("tcp-pcap-nanosec-example", false, useNanoSeconds);
    }

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}