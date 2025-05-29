#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet-sink.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  bool tracing = false;
  bool useNanoSeconds = false;

  CommandLine cmd;
  cmd.AddValue("tracing", "Enable pcap tracing", tracing);
  cmd.AddValue("nano-seconds", "Enable nano-second timestamps for pcap traces",
                 useNanoSeconds);
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("500Kbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("5ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t sinkPort = 8080;
  Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), sinkPort));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory",
                                    InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
  ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(1));
  sinkApps.Start(Seconds(1.0));
  sinkApps.Stop(Seconds(10.0));

  BulkSendHelper source("ns3::TcpSocketFactory", sinkAddress);
  source.SetAttribute("SendSize", UintegerValue(1024));
  source.SetAttribute("MaxBytes", UintegerValue(0));

  ApplicationContainer sourceApps = source.Install(nodes.Get(0));
  sourceApps.Start(Seconds(2.0));
  sourceApps.Stop(Seconds(10.0));

  if (tracing) {
    pointToPoint.EnablePcapAll("tcp-pcap-nanosec-example", false);
    if(useNanoSeconds){
        Simulator::EnablePcap("tcp-pcap-nanosec-example-nano", devices, true);
    }
  }

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}