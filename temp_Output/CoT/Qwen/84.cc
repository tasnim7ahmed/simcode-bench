#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/trace-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpTraceFileSimulation");

class UdpTraceClientHelper : public ApplicationHelper
{
public:
  UdpTraceClientHelper(Address address, uint16_t port, std::string filename)
  {
    m_factory.SetTypeId("ns3::UdpTraceClient");
    m_factory.Set("RemoteAddress", AddressValue(address));
    m_factory.Set("RemotePort", UintegerValue(port));
    m_factory.Set("TraceFilename", StringValue(filename));
  }
};

int main(int argc, char *argv[])
{
  bool enableLogging = false;
  bool useIpv6 = false;
  std::string traceFile = "file.pcap";

  CommandLine cmd(__FILE__);
  cmd.AddValue("enableLogging", "Enable logging", enableLogging);
  cmd.AddValue("useIpv6", "Use IPv6 instead of IPv4", useIpv6);
  cmd.AddValue("traceFile", "Trace file to send packets from", traceFile);
  cmd.Parse(argc, argv);

  if (enableLogging)
    {
      LogComponentEnable("UdpTraceFileSimulation", LOG_LEVEL_INFO);
      LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_ALL);
      LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_ALL);
    }

  NodeContainer nodes;
  nodes.Create(2);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
  csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
  csma.SetDeviceAttribute("Mtu", UintegerValue(1500));

  NetDeviceContainer devices = csma.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4InterfaceContainer ipv4Interfaces;
  Ipv6InterfaceContainer ipv6Interfaces;

  if (useIpv6)
    {
      ipv6Interfaces = stack.AssignIpv6Addresses(devices, Ipv6Address("2001:db8::"), Ipv6Prefix(64));
    }
  else
    {
      Ipv4AddressHelper address;
      address.SetBase("192.168.1.0", "255.255.255.0");
      ipv4Interfaces = address.Assign(devices);
    }

  uint16_t port = 4000;

  // Server on n1
  UdpEchoServerHelper echoServer(port);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  // Client on n0
  Address serverAddress;
  if (useIpv6)
    {
      serverAddress = Inet6SocketAddress(ipv6Interfaces.GetAddress(1, 1), port);
    }
  else
    {
      serverAddress = InetSocketAddress(ipv4Interfaces.GetAddress(1), port);
    }

  UdpTraceClientHelper clientHelper(serverAddress, port, traceFile);
  ApplicationContainer clientApps = clientHelper.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}