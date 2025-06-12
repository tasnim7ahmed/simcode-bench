#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpBulkSend");

int main(int argc, char* argv[]) {
  CommandLine cmd;
  bool tracing = false;
  uint32_t maxBytes = 0;

  cmd.AddValue("tracing", "Enable pcap tracing", tracing);
  cmd.AddValue("maxBytes",
               "Maximum number of bytes allowed to be sent", maxBytes);
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
  Address sinkAddress(
      InetSocketAddress(interfaces.GetAddress(1), sinkPort));

  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory",
                                    InetSocketAddress(Ipv4Address::GetAny(),
                                                      sinkPort));
  ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(1));
  sinkApps.Start(Seconds(1.0));
  sinkApps.Stop(Seconds(10.0));

  BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", sinkAddress);
  bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(maxBytes));
  ApplicationContainer sourceApps = bulkSendHelper.Install(nodes.Get(0));
  sourceApps.Start(Seconds(1.0));
  sourceApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));

  if (tracing) {
    AsciiTraceHelper ascii;
    pointToPoint.EnableAsciiAll(ascii.CreateFileStream("tcp-bulk-send.tr"));
    pointToPoint.EnablePcapAll("tcp-bulk-send");
  }

  Simulator::Run();

  uint64_t totalBytesReceived =
      DynamicCast<PacketSink>(sinkApps.Get(0))->GetTotalRx();
  std::cout << "Total Bytes Received: " << totalBytesReceived << std::endl;

  Simulator::Destroy();
  return 0;
}