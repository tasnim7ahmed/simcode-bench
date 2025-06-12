#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-address-generator.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpCsmaSimulation");

class UdpCsmaSimulation
{
public:
  UdpCsmaSimulation();
  void Setup(bool useIpv6, bool enableLogging);

private:
  void SetupIpv4(Ptr<Node> n0, Ptr<Node> n1);
  void SetupIpv6(Ptr<Node> n0, Ptr<Node> n1);
  void SetupApplications(Ptr<Node> n0, Ptr<Node> n1, Ipv4Address serverIp, uint16_t port);
  void SetupApplicationsIpv6(Ptr<Node> n0, Ptr<Node> n1, Ipv6Address serverIp, uint16_t port);

  bool m_enableLogging;
};

UdpCsmaSimulation::UdpCsmaSimulation()
{
}

void
UdpCsmaSimulation::Setup(bool useIpv6, bool enableLogging)
{
  m_enableLogging = enableLogging;

  if (enableLogging)
  {
    LogComponentEnable("UdpCsmaSimulation", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_ALL);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_ALL);
  }

  // Create nodes
  Ptr<Node> n0 = CreateObject<Node>();
  Ptr<Node> n1 = CreateObject<Node>();

  NodeContainer nodes(n0, n1);

  // Create CSMA channel
  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(5000000)));
  csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

  NetDeviceContainer devices = csma.Install(nodes);

  // Setup internet stack
  if (!useIpv6)
  {
    InternetStackHelper internet;
    internet.Install(nodes);
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);
    SetupApplications(n0, n1, interfaces.GetAddress(1), 9); // port 9
  }
  else
  {
    InternetStackHelper internet;
    internet.SetIpv4StackEnabled(false);
    internet.SetIpv6StackEnabled(true);
    internet.Install(nodes);

    Ipv6Address network = Ipv6Address("2001:db8::");
    Ipv6AddressGenerator::Initialize(network, Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces;
    for (auto it = nodes.Begin(); it != nodes.End(); ++it)
    {
      Ptr<Ipv6> ipv6 = (*it)->GetObject<Ipv6>();
      int32_t interface = 0; // loopback is index 0
      Ipv6InterfaceAddress ipv6Addr = Ipv6InterfaceAddress(Ipv6AddressGenerator::GetNetwork(64),
                                                          Ipv6Prefix(64));
      Ipv6AddressGenerator::NextNetwork();
      ipv6->AddInterfaceAddress(interface, ipv6Addr);
      ipv6->SetForwarding(0, true);
      interfaces.Add(*it, interface, ipv6Addr);
    }

    SetupApplicationsIpv6(n0, n1, interfaces.GetAddress(1, 0), 9); // port 9
  }
}

void
UdpCsmaSimulation::SetupApplications(Ptr<Node> n0, Ptr<Node> n1, Ipv4Address serverIp, uint16_t port)
{
  // Server application
  UdpEchoServerHelper echoServer(port);
  ApplicationContainer serverApps = echoServer.Install(n1);
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(10.0));

  // Client application
  UdpEchoClientHelper echoClient(serverIp, port);
  echoClient.SetAttribute("MaxPackets", UintegerValue(320));
  echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(50)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = echoClient.Install(n0);
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));
}

void
UdpCsmaSimulation::SetupApplicationsIpv6(Ptr<Node> n0, Ptr<Node> n1, Ipv6Address serverIp,
                                         uint16_t port)
{
  // Server application
  UdpEchoServerHelper echoServer(port);
  ApplicationContainer serverApps = echoServer.Install(n1);
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(10.0));

  // Client application
  UdpEchoClientHelper echoClient(serverIp, port);
  echoClient.SetAttribute("MaxPackets", UintegerValue(320));
  echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(50)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = echoClient.Install(n0);
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));
}

int
main(int argc, char *argv[])
{
  bool useIpv6 = false;
  bool enableLogging = false;

  CommandLine cmd(__FILE__);
  cmd.AddValue("useIpv6", "Use IPv6 addressing instead of IPv4", useIpv6);
  cmd.AddValue("enableLogging", "Enable logging components", enableLogging);
  cmd.Parse(argc, argv);

  UdpCsmaSimulation simulation;
  simulation.Setup(useIpv6, enableLogging);

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}