#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-address-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpCsmaSimulation");

class UdpCsmaSimulation {
public:
  UdpCsmaSimulation();
  void Setup(bool useIpv6, bool enableLogging);
  void Run();

private:
  NodeContainer nodes;
  CsmaHelper csma;
  InternetStackHelper internet;
  Ipv4AddressHelper ipv4AddrHelper;
  Ipv6AddressHelper ipv6AddrHelper;
  uint16_t port = 9;
  bool m_enableLogging;
};

UdpCsmaSimulation::UdpCsmaSimulation()
  : m_enableLogging(false)
{
  nodes.Create(2);
}

void
UdpCsmaSimulation::Setup(bool useIpv6, bool enableLogging)
{
  m_enableLogging = enableLogging;

  if (m_enableLogging) {
    LogComponentEnable("UdpCsmaSimulation", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_ALL);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_ALL);
  }

  csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(100000000)));
  csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

  NetDeviceContainer devices = csma.Install(nodes);

  internet.SetIpv4StackInstall(useIpv6 ? false : true);
  internet.SetIpv6StackInstall(useIpv6);
  internet.Install(nodes);

  if (useIpv6) {
    ipv6AddrHelper.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces = ipv6AddrHelper.Assign(devices);
  } else {
    ipv4AddrHelper.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4AddrHelper.Assign(devices);
  }
}

void
UdpCsmaSimulation::Run()
{
  Address sinkAddress;
  Address sourceAddress;

  if (internet.GetIpv4StackInstall()) {
    sinkAddress = InetSocketAddress(Ipv4Address("10.1.1.2"), port);
    sourceAddress = InetSocketAddress(Ipv4Address("10.1.1.1"), port);
  } else {
    sinkAddress = Inet6SocketAddress(Ipv6Address("2001:db8::200:ff:fe00:2"), port);
    sourceAddress = Inet6SocketAddress(Ipv6Address("2001:db8::200:ff:fe00:1"), port);
  }

  // Server node (n1) receives packets
  PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", sourceAddress);
  ApplicationContainer serverApp = packetSinkHelper.Install(nodes.Get(1));
  serverApp.Start(Seconds(0.0));
  serverApp.Stop(Seconds(10.0));

  // Client node (n0) sends packets
  OnOffHelper onOffHelper("ns3::UdpSocketFactory", sinkAddress);
  onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onOffHelper.SetAttribute("DataRate", DataRateValue(DataRate(1000000)));
  onOffHelper.SetAttribute("PacketSize", UintegerValue(1024));
  onOffHelper.SetAttribute("MaxBytes", UintegerValue(1024 * 320));

  ApplicationContainer clientApp = onOffHelper.Install(nodes.Get(0));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
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

  UdpCsmaSimulation sim;
  sim.Setup(useIpv6, enableLogging);
  sim.Run();

  return 0;
}