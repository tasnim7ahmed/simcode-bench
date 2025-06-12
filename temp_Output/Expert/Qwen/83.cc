#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CsmaUdpSimulation");

class CsmaUdpSimulation
{
public:
  CsmaUdpSimulation();
  void Setup(bool useIpv6, bool enableLogging);

private:
  NodeContainer nodes;
  CsmaHelper csma;
  InternetStackHelper stack;
  Ipv4InterfaceContainer interfacesv4;
  Ipv6InterfaceContainer interfacesv6;
  uint16_t sinkPort = 9;
};

CsmaUdpSimulation::CsmaUdpSimulation()
{
  nodes.Create(2);
  csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(100e6)));
  csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
}

void
CsmaUdpSimulation::Setup(bool useIpv6, bool enableLogging)
{
  if (enableLogging)
  {
    LogComponentEnable("CsmaUdpSimulation", LOG_LEVEL_INFO);
  }

  csma.Install(nodes);

  stack.Install(nodes);

  if (useIpv6)
  {
    interfacesv6 = stack.AssignIpv6Addresses(Prefix(64));
    interfacesv6.SetForwarding(1, true);
    interfacesv6.SetDefaultRouteInAllNodes(1);
  }
  else
  {
    interfacesv4 = stack.AssignIpv4Addresses(Ipv4AddressGenerator::GetNextNetwork(), Ipv4Mask("/24"));
  }

  Address sinkAddress;
  if (useIpv6)
  {
    sinkAddress = Inet6SocketAddress(interfacesv6.GetAddress(1, 1), sinkPort);
  }
  else
  {
    sinkAddress = InetSocketAddress(interfacesv4.GetAddress(1), sinkPort);
  }

  // Packet sink on n1
  PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory",
                                    InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
  ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(10.0));

  // UDP client on n0
  OnOffHelper client("ns3::UdpSocketFactory", sinkAddress);
  client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  client.SetAttribute("DataRate", DataRateValue(DataRate(1024 * 8 / 0.05))); // Calculate rate from packet size and interval
  client.SetAttribute("PacketSize", UintegerValue(1024));
  client.SetAttribute("MaxBytes", UintegerValue(1024 * 320));

  ApplicationContainer clientApp = client.Install(nodes.Get(0));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(10.0));
}

int
main(int argc, char* argv[])
{
  bool useIpv6 = false;
  bool enableLogging = false;

  CommandLine cmd(__FILE__);
  cmd.AddValue("useIpv6", "Use IPv6 addressing instead of IPv4", useIpv6);
  cmd.AddValue("enableLogging", "Enable simulation logging", enableLogging);
  cmd.Parse(argc, argv);

  CsmaUdpSimulation sim;
  sim.Setup(useIpv6, enableLogging);

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}